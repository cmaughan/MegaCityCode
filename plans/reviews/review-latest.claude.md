# Spectre Codebase Review

## Executive Summary

Spectre is a focused, well-structured Neovim GUI built on clean abstraction boundaries. The module separation is sound and the test infrastructure is above average for a project at this stage. That said, several architectural landmines will slow multi-agent work, the GPU path has avoidable stalls, and several critical end-user features are missing entirely.

---

## Detailed Findings

### Module Separation and Architecture

The dependency graph (`types → window/renderer/font/grid → nvim → app`) is enforced and correct. The renderer factory lives inside `spectre-renderer` rather than `app/`, which is the right call per `AGENTS.md`. The `IRenderer`/`IWindow` interfaces cleanly isolate platform code.

**Friction points for multi-agent work:**

- `app/app.cpp` is a ~500-line god object that owns all subsystems, wires all callbacks, and contains business logic (cursor blink state machine, font size changes, resize math). Any two agents touching app-level behaviour will collide here. There is no clean way to extract a cursor controller or a resize policy without touching `app.h` and `app.cpp` simultaneously.
- `RendererState` is shared between the Vulkan and Metal paths via a header in `libs/spectre-renderer/src/`. It has a stateful cursor save/restore that both backends must call in the right order inside `begin_frame`/`end_frame`. A change to cursor geometry affects both backends.
- `GridRenderingPipeline` holds four raw non-owning pointers (`IRenderer*`, `Grid*`, `HighlightTable*`, `TextService*`). There is no lifetime guarantee or dependency injection — callers must manually call `initialize()` with the right objects and keep those objects alive. This is invisible to the type system.

---

### Bad Code Smells

**1. Duplicate swapchain recreation in `vk_renderer.cpp`**
`begin_frame()` and `end_frame()` both contain identical five-step swapchain rebuild sequences:
```cpp
ctx_.recreate_swapchain(pixel_w_, pixel_h_);
pipeline_.shutdown(ctx_.device());
pipeline_.initialize(ctx_, "shaders");
// destroy/recreate descriptor pool
```
This should be a single private method. As written, keeping both in sync requires editing two places every time the rebuild sequence changes.

**2. Dead `create_render_pass()` call in `vk_context.cpp`**
`VkContext::initialize()` calls `create_render_pass()` which creates a render pass, then immediately calls `recreate_swapchain()` which destroys and recreates it. The first creation is wasted work and the function is never called again. This is a stale refactor artifact.

**3. Synchronous GPU stalls in `vk_atlas.cpp`**
Every `upload()` and `upload_region()` call ends with `vkQueueWaitIdle` via the `end_single_command` helper. For a terminal that rasterizes new glyphs on every new character, this stalls the entire GPU pipeline on the render thread. The Metal path avoids this by using shared-memory textures, but the Vulkan path regresses back to serialised copies.

**4. Alpha channel as "unset colour" sentinel in `highlight.h`**
```cpp
fg = (attr.fg.a > 0) ? attr.fg : default_fg_;
```
`HlAttr` uses `a == 0` to mean "no colour set". A legitimately transparent foreground (alpha-blended text) would be indistinguishable from "inherit default". This conflation will cause rendering bugs if translucency is ever supported.

**5. `FontManager::face()` leaks FreeType internals across module boundaries**
`FontManager` exposes a raw `FT_Face` via a public accessor. `GlyphCache` and `TextService` store and use it after `FontManager` has returned it. If `FontManager` is moved or destroyed (e.g. during font-size changes where `set_point_size` reconstructs the HarfBuzz font), any retained `FT_Face` pointer becomes dangling. The `FontManager` move constructor transfers these pointers, making this especially easy to misuse.

**6. Atlas overflow silently discards glyphs**
`GlyphCache::reserve_region()` logs a warning and returns `false` when the 2048×2048 atlas is full. The callers return an empty `AtlasRegion`, which renders as invisible text. There is no eviction policy, no atlas rebuild, and no user-visible indication. In practice this affects ligature-heavy or emoji-heavy sessions.

**7. Redundant CMake if/else for executable sources**
```cmake
if(APPLE)
    add_executable(spectre app/main.cpp app/app.cpp app/grid_rendering_pipeline.cpp)
else()
    add_executable(spectre app/main.cpp app/app.cpp app/grid_rendering_pipeline.cpp)
endif()
```
Both branches are identical. This is inert but confusing dead weight.

**8. Unchecked Vulkan return values in `vk_atlas.cpp` and `vk_pipeline.cpp`**
`vkCreateCommandPool`, `vkCreateImageView`, `vkCreateSampler`, `vkCreateFramebuffer`, `vkCreateGraphicsPipelines`, and several others do not check `VkResult`. A failed allocation returns `VK_NULL_HANDLE` silently and the renderer will likely crash later with an obscure validation error or access violation.

**9. `App::initialize()` — 150-line sequential initializer with numbered comments**
The ten numbered steps in `initialize()` are a sign that this function is doing too much. Steps 5–10 (nvim spawn, RPC init, UI event setup, input setup, window event wiring, UI attach) could live in focused helper methods or separate objects. As written, any failure at step 8 requires unwinding steps 1–7 manually, and the shutdown path is separate from the init path.

**10. Hardcoded shader directory string `"shaders"` throughout `vk_renderer.cpp`**
`pipeline_.initialize(ctx_, "shaders")` is called in multiple places including swapchain rebuild. If the binary is ever run from a different working directory, or if shaders are relocated, all these call sites must be updated. The Metal path correctly looks up the metallib relative to the bundle/executable.

---

### Testing Holes

The test suite is strong for its size — the replay fixture, RPC integration against a fake server, and unicode conformance tests are all excellent. The following are uncovered:

- **Cursor blink state machine**: `CursorBlinkPhase` transitions (Steady → Wait → Off → On) live in `App` and have no unit test. The blink timer arithmetic is tested nowhere.
- **`TextService` fallback font selection**: `resolve_font_for_text()` with primary and fallback fonts is not covered. The `can_render_cluster` waterfall is exercised only implicitly via font_tests.
- **Atlas overflow/eviction**: No test fills the glyph atlas and verifies the failure path or any future eviction strategy.
- **`App::change_font_size()`**: The recalculation of grid dimensions after a font size change is non-trivial and untested.
- **`HighlightTable::resolve()`** reverse attribute: No test verifies that `reverse = true` swaps fg/bg correctly end-to-end.
- **Grid scroll at boundary conditions**: Partial-region scrolls (`left != 0` or `right != cols`) are only lightly exercised.
- **`NvimProcess` read returning 0 bytes**: The POSIX path handles `n == 0` (EOF) differently from `n < 0` (error); this distinction is not tested.
- **Double-width cell at the rightmost column**: Writing a double-wide cell at column `cols - 1` should not write the continuation cell out of bounds.
- **`apply_ui_option()` for the `ambiwidth` option**: The option parsing in `app.cpp` is tested only via the full ui_events path; the app-level handler itself has no dedicated test.
- **VkRenderer/MetalRenderer initialise-shutdown cycle**: No headless test verifies the GPU object lifecycle even at a stub level.

---

## Top 10 Good Things

1. **Clean `IRenderer` / `IWindow` interfaces**: Platform-specific code is fully contained in the backend implementations. App code only touches the public API and never includes backend headers.
2. **Shared `RendererState` across backends**: Dirty-cell tracking, cursor overlay geometry, and incremental buffer upload logic is written once and shared by both Vulkan and Metal.
3. **Replay fixture for UI event testing**: `replay_fixture.h` enables deterministic unit tests for all redraw event parsing without spawning a real nvim process.
4. **RPC fake-server integration tests**: A compiled fake server simulates all important RPC transport scenarios (success, error, abort, malformed, interleaved notification). This is well above average for a project of this size.
5. **Unicode conformance tests against live nvim**: `unicode_tests.cpp` queries a headless nvim to validate `cluster_cell_width()` against the reference implementation, catching divergence automatically.
6. **Physical PPI from OS APIs**: `display_ppi()` uses `MDT_RAW_DPI` on Windows and CoreGraphics pixel-width on macOS rather than logical DPI, producing correct font sizing on HiDPI displays.
7. **Incremental atlas dirty-rect uploads**: `GridRenderingPipeline` tracks the minimal dirty rectangle on the atlas and uploads only that region, reducing GPU bandwidth proportional to new-glyph frequency.
8. **Well-structured CMake with FetchContent**: All dependencies are pinned by tag and fetched automatically. No system-installed libraries are required beyond the GPU SDK.
9. **Structured logging with injectable sink**: `set_log_sink()` allows tests to capture log output without subprocess redirection, enabling `rpc_integration_tests.cpp` to assert log-level warnings.
10. **`IGridSink` abstraction for UI event handler**: `UiEventHandler` targets `IGridSink` rather than the concrete `Grid`, enabling the `RecordingGridSink` in tests without linking the full grid implementation.

---

## Top 10 Bad Things

1. **`App` is a god object**: All subsystems, all callbacks, cursor blink, resize math, and font management live in one 500-line class. Multi-agent editing and unit testing are both impaired.
2. **Duplicated swapchain recreation**: Identical five-step sequences appear in both `begin_frame()` and `end_frame()` of `VkRenderer`. Maintenance cost is doubled.
3. **Synchronous `vkQueueWaitIdle` on every atlas upload**: The Vulkan atlas serialises the GPU pipeline on each new glyph, creating frame stalls proportional to the number of unique glyphs per frame.
4. **Atlas full = silent invisible glyphs**: There is no eviction, no rebuild, and no user signal. In emoji-heavy or ligature-heavy sessions, text silently disappears.
5. **Raw pointer soup in `GridRenderingPipeline`**: Four non-owning raw pointers with no lifetime guarantees, initialised via a separate `initialize()` call that has no RAII equivalent.
6. **Unchecked Vulkan return codes**: Key allocations in `vk_atlas.cpp` and `vk_pipeline.cpp` silently produce null handles, turning GPU errors into deferred crashes.
7. **Dead `create_render_pass()` in `VkContext::initialize()`**: The initial render pass is created and then destroyed and recreated inside `recreate_swapchain()`. The first creation is wasted and the dead function creates confusion.
8. **Alpha-as-sentinel in `HighlightTable::resolve()`**: Using `a == 0` to mean "colour not set" conflates unset with transparent and will be incorrect if alpha-blended highlights are ever supported.
9. **`FT_Face` exposed across module boundaries**: `GlyphCache` stores a `FT_Face` obtained from `FontManager`. If the manager is moved or rebuilt (during font-size changes), the cached face pointer becomes dangling.
10. **Hardcoded font path with silent fallback**: The primary font resolver falls back to `"fonts/JetBrainsMonoNerdFont-Regular.ttf"` even when no font is found, meaning font-not-found silently produces missing glyphs rather than an error.

---

## Top 10 Quality-of-Life Features

1. **Configurable font path and fallback list** — currently hardcoded to JetBrains Mono Nerd Font. A configuration file or env var specifying the primary font path is the single most impactful UX change.
2. **Clipboard integration** — no clipboard read/write is visible in the codebase. Neovim has `nvim_get_option`/clipboard support but the GUI must implement the system clipboard bridge via SDL or OS APIs.
3. **Window title reflecting nvim's `titlestring`** — Neovim emits `set_title` events (or `title` in the legacy protocol). The window title currently never changes from "Spectre".
4. **Configuration file** (TOML or INI) — font size, padding, key bindings, window dimensions, and background colour currently require recompilation. A `~/.config/spectre/config.toml` would unlock broad customisation.
5. **Ligature rendering** — HarfBuzz already shapes clusters but there is no mechanism to shape across adjacent cells (which ligatures require). Common programming ligatures (`->`, `!=`, `=>`) are currently rendered as separate glyphs.
6. **IME / input method support** — `SDL_StartTextInput` is called but there is no `SDL_EVENT_TEXT_EDITING` handler for composition strings. CJK users cannot use input method editors.
7. **Tab bar / tabline rendering** — Neovim reports tab events via `tabline_update`. The current renderer only handles the grid; a native tab bar would improve navigation.
8. **Scrollbar position indicator** — a thin visual indicator showing the viewport position in the buffer (read from nvim's `topline`/`botline` via `win_viewport`) would improve orientation in long files.
9. **Font size persisted across sessions** — Ctrl+Plus/Minus changes the font size at runtime but the change is lost on exit. Writing the last-used size to a state file would make this useful.
10. **Underline/undercurl rendering** — `style_flags` carries `underline` and `undercurl` bits but the shaders do not draw them. Diagnostic highlights and spell checking are invisible.

---

## Top 10 Tests to Add for Stability

1. **Cursor blink state machine unit test** — exercise all `CursorBlinkPhase` transitions in `App::advance_cursor_blink()` and `restart_cursor_blink()` with a mocked clock, verifying correct phase sequencing and deadline arithmetic.
2. **Atlas overflow / eviction** — fill the glyph atlas to capacity and verify that subsequent rasterisation requests do not crash or silently discard, and that the system handles a forced full-upload correctly after a font-size change.
3. **Double-width cell at column `cols - 1`** — confirm that `Grid::set_cell()` with `double_width = true` at the rightmost column does not write out of bounds and does not mark a non-existent continuation cell.
4. **`HighlightTable::resolve()` reverse attribute** — verify that `reverse = true` swaps foreground and background colours, including when combined with missing fg or bg (inherited from default).
5. **`TextService` fallback font waterfall** — create a `TextService` with a primary font that cannot render a specific codepoint, verify the fallback font is selected and the glyph cache entry reflects the fallback face.
6. **`App::change_font_size()` grid recalculation** — given a known window size and old/new font metrics, verify that the new `grid_cols_`/`grid_rows_` values are correct and that `nvim_ui_try_resize` is called with the right dimensions.
7. **Grid scroll partial region** — test `Grid::scroll()` with `left != 0` and `right != cols` to verify that cells outside the scrolled region are untouched and dirty flags are set only within the region.
8. **RPC request timeout** — inject a fake server that never responds, verify `NvimRpc::request()` returns within the 5-second timeout and sets `transport_ok = false` without blocking the calling thread indefinitely.
9. **`RendererState` dirty-range incremental upload** — after a full-grid layout, clear dirty, update two non-adjacent cells, and verify that `dirty_cell_offset_bytes()` and `dirty_cell_size_bytes()` span exactly the range from first to last updated cell (the existing test only partially covers this).
10. **`NvimProcess::read()` returns 0 (EOF) on POSIX** — inject a fake child process that writes nothing and closes its pipe, verify the reader thread detects EOF, sets `read_failed_`, and notifies any blocked `request()` calls promptly.

---

## Top 10 Worst Existing Features

1. **No clipboard support** — copy/paste requires workarounds (OSC 52, terminal clipboard). For daily use this is critical missing functionality.
2. **Hardcoded font, no user configuration** — users who do not have JetBrains Mono Nerd Font installed see blank text. There is no way to select a different font without recompiling.
3. **Underline and undercurl not rendered** — `style_flags` is stored and transmitted to the GPU but the shaders ignore it. Neovim's diagnostic highlights, spell checking, and many colour schemes rely on these.
4. **Atlas full = invisible glyphs** — when the 2048×2048 atlas is exhausted, new glyphs silently vanish. There is no recovery, eviction, or user warning. A long session with many unique characters will hit this.
5. **Synchronous GPU stalls on atlas upload (Vulkan)** — every new unique glyph on the Vulkan path stalls the GPU pipeline. On a cold start with a large file, this produces visible frame drops.
6. **No IME / composition input** — CJK and other users who rely on input method editors cannot enter text. `SDL_EVENT_TEXT_EDITING` is unhandled.
7. **No window title updates** — the title is always "Spectre" regardless of the current file or nvim's `titlestring` setting. This breaks task-switcher and terminal multiplexer labelling.
8. **No configuration file** — padding, default font size, and background colour require source edits. Background is hardcoded to `(0.1, 0.1, 0.1)` in multiple places in the renderer.
9. **Shader path hardcoded to `"shaders"` relative to CWD** — `main.cpp` sets the working directory to the executable's directory on startup, which usually works, but the Vulkan renderer's shader path is not resolved relative to the executable in the way Metal's is. A mismatched build layout silently fails to open shaders.
10. **No ligature support** — HarfBuzz is present but is used only to shape individual grid cells. Programming ligatures (`!=`, `->`, `=>`, `<=`) that span cells cannot be rendered, limiting visual fidelity compared to other Neovim GUIs.

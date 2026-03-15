# Spectre Codebase Review

## Overview

Spectre is a cross-platform Neovim GUI frontend with a well-layered architecture. The code is generally clean and purposeful, but there are several structural issues that will create friction as the codebase grows—especially under multi-agent development.

---

## Architecture and Modular Separation

### What Works

The dependency graph is well-enforced: `spectre-types` → `spectre-grid/font/window/renderer` → `spectre-nvim` → `app`. Public headers are clean. `IRenderer`, `IWindow`, `IGridSink`, and `IRpcChannel` interfaces give genuine platform/implementation independence, and `RendererState` is correctly shared between the two renderer backends.

### Structural Problems

**God object: `App`**
`app/app.cpp` (~600 lines) is the most dangerous file in the codebase for parallel agents. It owns the cursor blink state machine, font size change logic, startup resize deferral, smoke test mode, render test mode, event routing, and grid column/row tracking. Any feature touching the frame loop will conflict. The cursor blink phase machine alone (`restart_cursor_blink`, `advance_cursor_blink`, `apply_cursor_visibility`) is a ~100-line untested state machine embedded in the orchestration class.

**Test infrastructure compiled into the production binary**
`render_test.cpp` / `render_test.h` are compiled into `spectre.exe`. They bring in BMP I/O, pixel comparison, JSON report writing, and scenario parsing—none of which belong in a production build. `App::run_smoke_test()` and `App::run_render_test()` are test harness methods on the production orchestration class.

**Duplicate TOML parsers**
`app/app_config.cpp` and `app/render_test.cpp` each contain their own `trim()`, `unquote()`, and `parse_string_array()` functions. They are almost identical but differ in edge cases (escape handling in render_test). This means bug fixes need to be applied twice.

**`font.h` is a passthrough**
`libs/spectre-font/include/spectre/font.h` contains only `#include <spectre/text_service.h>`. This is misleading and adds no value.

**`GLYPH_ATLAS_SIZE` in shared types**
`libs/spectre-types/include/spectre/types.h` defines `GLYPH_ATLAS_SIZE = 2048`. This is a renderer/font implementation detail in the shared POD types layer, creating an implicit coupling between font and renderer that circumvents the interface boundary.

**`IWindow` leaks SDL**
`IWindow::native_handle()` returns `SDL_Window*`. Any alternative window backend would need to return an SDL type it doesn't use. The `SDL_Window` forward declaration appears in the `IWindow` public header.

---

## Code Smells

**Dead code in `render_test.cpp`**
```cpp
for (auto& entry : items)
    entry = entry;  // no-op
```
This loop in `parse_string_array` does nothing and is a copy-paste artifact.

**Undocumented threading assumption in `TextService::Impl`**
`font_choice_cache` is mutated during `resolve_cluster`. CLAUDE.md confirms this is main-thread only, but there is no comment in the code. A future agent touching font loading has no visible signal.

**Missing return value check in `VkGridBuffer::ensure_size`**
```cpp
vmaCreateBuffer(allocator, &buf_ci, &alloc_ci, &buffer_, &allocation_, &alloc_info);
mapped_ = alloc_info.pMappedData;
```
The `vmaCreateBuffer` return value is ignored on the resize path. If allocation fails, `mapped_` is null and the next write will crash.

**`const char*` / `std::string` mismatch**
`App::queue_resize_request` takes `const char* reason`, but `UiRequestWorker::request_resize` takes `std::string reason`. The conversion is implicit and harmless now but the API inconsistency is a smell.

**Multiple `FT_Library` instances**
Each `FontManager` creates its own `FT_Library`. With multiple fallback fonts, there are N separate FreeType library contexts. A single shared library instance would be correct and more efficient.

**Synchronous GPU stall on every atlas upload**
`VkAtlas::upload_internal` waits on a fence after every atlas region update (`vkWaitForFences` in `end_single_command`). This stalls the GPU-CPU pipeline on every new glyph, causing potential frame jitter when many new glyphs appear simultaneously.

**Metal renderer stores all objects as `void*`**
Every Metal object is stored as `void*` with manual `__bridge_retained`/`__bridge_transfer` casts. While technically correct in ARC, this pattern is fragile, defeats static analysis, and makes reviewer errors easy to introduce.

**Metal frame semaphore count = 1 with `MAX_FRAMES_IN_FLIGHT = 2`**
The semaphore comment explains this is necessary because the grid buffer is shared. But this means the 2-frames-in-flight constant is misleading—the Metal renderer effectively serializes frames.

**Descriptor set update tracking**
The `needs_descriptor_update_` + `desc_update_pending_frames_` bitmask logic in `vk_renderer.cpp` is complex. The interaction between `update_all_descriptor_sets`, `update_descriptor_sets_for_frame`, and the pending bitmask is easy to get wrong if the atlas or grid buffer is ever replaced.

**`wake_event_type_ == 0` check in `SdlWindow::wake()`**
SDL_RegisterEvents allocates from 0x8000 upward; it returns `(Uint32)-1` on failure, not 0. The `== 0` guard in `wake()` is redundant and suggests the intent was to check for an uninitialized state that doesn't exist.

---

## Testing Holes

The test suite is significantly better than average for a GPU application, but these gaps are notable:

1. **`AppConfig` round-trip** — no test validates that `save()` + `load()` is lossless, or that out-of-range values are clamped correctly.
2. **Cursor blink state machine** — the phase transitions (`Steady → Wait → Off → On`) and the timing logic are entirely untested.
3. **`change_font_size` path** — font size changes trigger renderer relayout, grid resize requests, atlas resets, and config saves. No test covers this chain.
4. **Startup resize deferral** — the `startup_resize_pending_` flag accumulates a resize before the first flush. No test exercises this path.
5. **Atlas overflow recovery in `TextService`** — `resolve_cluster` has a retry path after atlas exhaustion that calls `glyph_cache.reset()`. The font test only tests the cache overflow signal, not the recovery.
6. **`Grid::scroll` at double-width boundaries** — scrolling a double-width cell into column 0 or with the continuation cell at `cols-1` is untested.
7. **TOML escape handling** — `render_test.cpp` parses escaped quotes and backslashes in array literals, but there are no tests for `\"`, `\\`, or malformed escape sequences.
8. **Clipboard operations** — the `Ctrl+Shift+C` / `Ctrl+Shift+V` paths in `wire_window_callbacks` are untested.
9. **RPC 5-second timeout** — `kRequestTimeout` in `rpc.cpp` is only implicitly tested by `abort_after_read`, which exits immediately and triggers the pipe-EOF path, not the timeout path.
10. **`RendererState` cursor at grid edges** — `apply_cursor` with a cursor at col 0 or `grid_cols-1` is not explicitly tested.

---

## Top 10 Good Things

1. **Clean library layering** — the dependency graph is enforced in CMake and respected in code. Libraries link downward only.
2. **`IGridSink` / `IRpcChannel` interfaces** — testable seams at the right level. `FakeRpcChannel` and `RecordingGridSink` in tests are direct payoff.
3. **`RendererState` shared between backends** — the GPU cell state, dirty tracking, and cursor overlay are implemented once and tested independently.
4. **`replay_fixture.h`** — the builder DSL for msgpack values makes UI event tests readable and portable; this is the right way to test redraw parsing.
5. **Deterministic render snapshots** — pixel-diff BMP output, per-platform reference images, and tolerance thresholds are a high-value investment for a GPU application.
6. **Dirty rect tracking for atlas uploads** — incremental atlas uploads with a growing dirty rect avoid uploading the full 2048×2048 texture on every frame.
7. **Cursor overlay design** — the extra GpuCell slot at the end of the grid buffer for non-block cursors is elegant and avoids per-frame grid mutation.
8. **`UiRequestWorker`** — off-loads blocking `nvim_ui_try_resize` calls to a background thread while debouncing rapid resize events correctly.
9. **CI on both platforms** — the GitHub Actions workflow actually builds and tests on `windows-latest` and `macos-latest` with real nvim installed.
10. **Unicode conformance test against headless nvim** — `run_unicode_tests` validates `cluster_cell_width` against nvim's `strdisplaywidth`. This is the only correct oracle for this behavior.

---

## Top 10 Bad Things

1. **`App` is a god object** — cursor blink, font resize, startup resize deferral, test modes, event routing, and frame pacing are all in one class. This file will conflict under parallel agent work.
2. **Test infrastructure in the production binary** — `render_test.cpp` brings BMP I/O, pixel comparison, and scenario parsing into `spectre.exe`.
3. **Duplicate TOML parsers** — `app_config.cpp` and `render_test.cpp` each maintain their own `trim`/`unquote`/`parse_string_array`. Bug fixes must be applied to both.
4. **Missing error check in `VkGridBuffer::ensure_size`** — silent crash on GPU OOM during resize.
5. **Synchronous GPU stall per atlas region** — `VkAtlas::upload_internal` blocks the CPU until the GPU finishes the copy, causing jank during burst glyph rasterization.
6. **Dead `entry = entry` loop in `render_test.cpp`** — a no-op copy left in `parse_string_array`.
7. **`GLYPH_ATLAS_SIZE` in the shared types layer** — a renderer/font implementation detail has leaked into the shared POD types header.
8. **`font.h` is a passthrough** — adds confusion with no value.
9. **`IWindow::native_handle()` returns `SDL_Window*`** — SDL is in the public interface of the window abstraction.
10. **Metal `void*` object storage** — all Metal objects are `void*` with manual bridge casts, defeating static analysis and making memory errors easy to introduce.

---

## Top 10 Quality-of-Life Features to Add

1. **Tab bar** — display open buffers. Neovim exposes buffer/tab information via RPC. This is the single most requested GUI feature for nvim GUIs.
2. **IME composition display** — `on_text_editing` is wired but does nothing. Showing the in-progress composition string would unblock CJK users.
3. **Ligature support** — the current pipeline shapes each cell cluster independently. Cross-cell shaping (e.g., `=>` → arrow) requires holding cells until a full line is received, which `grid_line` already provides in batches.
4. **Ctrl+scroll font resize** — this is partially there (Ctrl+/- on keyboard), but trackpad pinch-to-zoom and mouse-wheel-with-Ctrl are unimplemented.
5. **System font discovery** — `default_primary_font_candidates()` checks a hardcoded list. A scan of system font directories with a preference ranking would reduce "font not found" support requests.
6. **Performance overlay** — FPS, atlas utilization percentage, and dirty cell count displayed on-demand. Atlas overflow is currently only surfaced via log warnings.
7. **Window title reflects file path** — nvim sends `set_title` events; the current title handling falls back to "Spectre". The full path or short project context would be more useful.
8. **Configurable padding** — `padding_ = 1` is hardcoded in both renderer backends. A user-facing padding option is a low-effort quality-of-life improvement.
9. **Multi-monitor DPI awareness** — when the window moves between monitors with different DPI, the font pixel size should update. Currently the PPI is queried once at startup.
10. **Colorscheme-aware default background** — the clear color is hardcoded to `(0.1, 0.1, 0.1)` in both renderers and both shaders. It should derive from the nvim `default_colors_set` background to eliminate the flash on startup before the first flush.

---

## Top 10 Tests to Add for Stability

1. **`AppConfig` round-trip** — `save()` then `load()`; verify clamping on out-of-range values and that unset fields use defaults.
2. **Cursor blink state machine** — inject mock time points; verify `Steady → Wait → Off → On → Off` phase transitions and that `cursor_busy_` suppresses blinking.
3. **`change_font_size` grid resize request** — verify that a font size change that makes the grid smaller issues a `nvim_ui_try_resize` via `UiRequestWorker`.
4. **Startup resize deferral** — send a window resize before the first flush; verify that after flush the correct resize is queued.
5. **Atlas overflow recovery** — drive `TextService::resolve_cluster` past atlas capacity with a tiny atlas, then verify the reset flag is consumed and the glyph is still returned on retry.
6. **`Grid::scroll` with double-width at left boundary** — scroll a row containing a double-width cell at column 0; verify the continuation cell moves correctly.
7. **TOML multiline array with escape sequences** — test `\"` inside strings and `\\` inside paths in the render test scenario parser.
8. **`RendererState` dirty range coalescing** — verify that two non-adjacent cell updates produce a dirty range covering both, and that `clear_dirty` resets it.
9. **`VkGridBuffer::ensure_size` failure handling** — inject a mock allocator that fails; verify the renderer does not crash and returns `false` from `begin_frame`.
10. **Unicode ambiwidth transitions** — verify that `cluster_cell_width` returns 1 then 2 for the same codepoint when `AmbiWidth::Single` vs `AmbiWidth::Double` options are applied, and that the grid correctly marks the continuation cell on the transition.

---

## Top 10 Worst Features (Candidates for Removal or Redesign)

1. **Custom TOML parser (both copies)** — replace with `tomlplusplus` (header-only, C++17) or `toml11`. This eliminates the duplication and is more correct.
2. **BMP codec in `render_test.cpp`** — a custom BMP reader/writer is ~150 lines of byte manipulation that could be replaced by a single call to `stb_image_write.h` or libpng. PNG would also be smaller on disk for reference images.
3. **`render_test.cpp` compiled into `spectre.exe`** — test infrastructure should live in a test-only build. Extract `RenderTestScenario`, the frame comparison, and the BMP I/O into a library linked only to test targets.
4. **Smoke test and render test methods on `App`** — `run_smoke_test()` and `run_render_test()` have no place on the production orchestration class. These should be free functions or a separate test runner type.
5. **`font.h` passthrough header** — remove it entirely. Any user that needs `TextService` should include `text_service.h` directly.
6. **`GLYPH_ATLAS_SIZE` in `spectre-types`** — move this constant to `spectre-font` and to a renderer-private header. If renderer and font need to agree on atlas size, an explicit parameter or a shared configuration struct is cleaner.
7. **Synchronous `VkAtlas::upload_internal`** — the fence wait on every atlas upload creates CPU stalls. Change to use a timeline semaphore or integrate atlas uploads into the main frame command buffer.
8. **Multiple `FT_Library` instances** — share a single `FT_Library` across all `FontManager` instances. The font module owns the library; individual font faces should not.
9. **Hardcoded clear color in shaders and renderers** — the clear color `(0.1, 0.1, 0.1)` appears in `grid.metal`, `grid_bg.frag`, and both renderers. It should be driven by the default background color from Neovim's color scheme.
10. **`wake_event_type_ == 0` guard in `SdlWindow::wake()`** — this check is meaningless (SDL registers user events starting at 0x8000). Remove the `== 0` branch; the only valid sentinel is `static_cast<Uint32>(-1)` which is already checked.

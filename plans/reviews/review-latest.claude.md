# MegaCityCode Codebase Review

---

## Module Architecture and Separation

The codebase is split into six static libraries plus an app orchestrator. The declared dependency direction (`megacitycode-types → window/renderer/font/grid → nvim → app`) is largely respected in headers. `megacitycode-renderer` exposes `RendererState` as an internal shared type between Vulkan and Metal backends, which is correct. The `IGridSink` interface in `megacitycode-types` gives `UiEventHandler` a clean seam to test against without the concrete `Grid`.

---

## Detailed Findings

### God Object: `App`

`app.h/cpp` owns ~30 private member variables and ~25 private methods, spanning window activation, font size changes, cursor blink, resize coalescing, render test coordination, debug overlay, and the main event loop. Every new feature touches this file. For multi-agent work this is a critical bottleneck—any concurrent changes to `app.cpp` will collide. The `pump_once` function is 60+ lines handling event polling, notification draining, blink advancement, frame rendering, and timeout calculation all in sequence.

### Duplicated Config Parsing

`app_config.cpp` and `render_test.cpp` both implement the same key=value TOML-like parsing loop. They share `toml_util.h` helpers, but the outer parse loop (strip comments, split on `=`, handle continuation lines) is duplicated verbatim. A single `ParsedConfig` utility or a shared `parse_kv_file()` function would eliminate this.

### Renderer Abstraction Leak

`IRenderer` exposes `set_ascender(int a)`. The renderer should know nothing about text metrics—it only needs to know where to draw a glyph quad. Ascender belongs to `GridRenderingPipeline` or `RendererState::apply_update_to_cell()`, which already has all the geometry math. The public API leaks a font concern into the GPU abstraction.

### Renderer State Duplication

`VkRenderer` and `MetalRenderer` both carry identical fields: `cell_w_`, `cell_h_`, `ascender_`, `padding_`, `pixel_w_`, `pixel_h_`. `RendererState` already owns the canonical layout data. Neither backend inherits from a common base carrying these scalars, so any bug in their initialization or update must be fixed twice.

### Hardcoded Hotkeys

`wire_window_callbacks()` in `app.cpp` hardcodes Ctrl+Shift+C/V for clipboard, Ctrl+=/- for font size, and F12 for debug overlay directly in a lambda. There is no key binding configuration, no way to reassign these, and no documentation that these shortcuts exist. A new keybindings table (even if just read from `config.toml`) would decouple this.

### IME is a No-op

`NvimInput::on_text_editing` ignores its argument entirely. This silently breaks CJK, Korean, Arabic, and any other input method that requires a composition phase. The function signature and registration hook are present; only the implementation is missing.

### Atlas Size is Fixed

`VkAtlas::ATLAS_SIZE` and `MetalRenderer::ATLAS_SIZE` are both compile-time constants of `2048`. On a 4K display with a 14pt font, a typical session can fill this in minutes, triggering the atlas reset path and re-uploading the full 16 MB texture. The atlas should either grow dynamically or support multiple atlas pages.

### Magic Clear Color

The default cell background in `RendererState::relayout()` is hardcoded as RGB `(0.1, 0.1, 0.1)` and the renderer clear color is `(0.1, 0.1, 0.1)` in both Vulkan and Metal. These should come from the Neovim default background color once `default_colors_set` fires. Until the first flush, the background color is renderer-specific instead of following the colorscheme.

### `CursorBlinker::Phase::Wait` and `Phase::On` are identical in `advance()`

Both `Wait` and `On` transition to `Phase::Off` with `blinkoff` duration. The `Wait` phase exists to hold the cursor visible for `blinkwait` ms before the first hide, but after that hide it should enter the normal `On/Off` cycle with `blinkon` duration—not another `blinkoff`. The current code correctly uses `blinkwait` as the initial deadline (in `restart`) and `blinkoff` after, but the `Phase::On` path also uses `blinkoff` (should be `blinkon`). Let me re-read... Actually `On` correctly transitions to `Off` with `blinkoff`, and `Off` transitions to `On` with `blinkon`. The issue is that `Wait` also transitions to `Off` with `blinkoff` instead of keeping track of the `blinkon` phase separately—this is correct behavior but the code comment `Phase::On` does the same transition as `Wait` which is misleading. On re-read this is fine; the only issue is that `Wait->Off` should use `blinkoff`, which it does. So this is more of a readability/naming concern.

### `bg_instances()` vs `fg_instances()` Asymmetry

`RendererState::bg_instances()` includes the cursor overlay cell but `fg_instances()` does not. The Vulkan/Metal shaders draw `bg_instances` background quads and `fg_instances = total_cells + overlay_cell_count_` foreground quads, so the cursor overlay cell gets a BG quad but never a FG quad. This is intentional (cursor is BG-only), but the asymmetry is undocumented and easy to misread as a bug.

### `UiRequestWorker` signature mismatch

`ui_request_worker.h` declares `request_resize(int cols, int rows, std::string reason)` but `app.cpp` calls `queue_resize_request(cols, rows, "reason")` where the reason is a `const char*`. The implicit conversion to `std::string` works but is an unnecessary allocation. More importantly, `UiRequestWorker` takes the reason by value and moves it, but `app.cpp` always passes a string literal—the move is a pessimization.

### No `ext_multigrid` Support

`nvim_ui_attach` explicitly sets `ext_multigrid: false`. This disables floating windows, completion menus positioned over text, and the LSP hover popup floating windows that modern Neovim plugins rely on. This is a known limitation but worth calling out as a significant QoL gap.

---

## Top 10 Good Things

1. **Clean layered library architecture** — The six-library split with declared dependency direction (`megacitycode-types` as the base) is enforced by CMake and respected in code. Circular dependencies are absent.
2. **`IRenderer` / `IWindow` abstraction** — Platform backends are completely hidden behind thin interfaces. The app never touches Vulkan or Metal headers. `renderer_factory.cpp` is the single platform dispatch point.
3. **`IGridSink` seam for testing** — `UiEventHandler` depends on an interface, not `Grid`. `RecordingGridSink` in the tests demonstrates the payoff: UI parsing is tested without a real grid.
4. **RPC integration tests with a real subprocess** — `rpc_integration_tests.cpp` spawns `megacitycode-rpc-fake` as a real child process over real pipes. This tests the actual process-spawn and transport stack, not a mock, and catches real shutdown edge cases.
5. **Render snapshot suite** — BMP pixel comparison with configurable tolerance and a `do.py` blessing workflow. Cross-platform reference images are checked in. The scenario TOML format is readable and easily extensible.
6. **`RendererState` shared between backends** — GPU cell layout, dirty tracking, cursor overlay, and overlay cell math live in one place. Both Vulkan and Metal call the same `copy_dirty_cells_to`, eliminating divergence in the hottest path.
7. **Atlas overflow recovery** — The `consume_overflowed()` / `atlas_reset_pending` loop in `TextService::resolve_cluster` gracefully handles a full atlas without crashing. The two-attempt loop in `GridRenderingPipeline::flush` ensures the reset is transparent to the caller.
8. **Cursor blinker state machine** — `CursorBlinker` is a standalone, fully testable class with no renderer dependency. The `next_deadline()` pattern drives the main loop timeout cleanly.
9. **Unicode cluster width matching neovim** — `cluster_cell_width` is tested against a live `nvim_call_function("strdisplaywidth")` oracle, ensuring pixel-perfect alignment with neovim's grid layout for emoji, CJK, ZWJ sequences, and ambiguous-width characters.
10. **Shutdown sequence correctness** — `App::shutdown` sends the quit command, then closes the RPC transport, then stops the resize worker, then kills the nvim process, then joins threads. The ordering prevents deadlocks in the multi-threaded shutdown path.

---

## Top 10 Bad Things

1. **`App` is a God Object** — ~30 member variables and ~25 methods in one class that owns the main loop, all subsystem wiring, font size changes, clipboard, debug overlay, cursor state, and render test coordination. Any multi-agent work on features will collide on `app.cpp`.
2. **Duplicated config parsing loop** — `app_config.cpp` and `render_test.cpp` both implement the same `while(getline → strip comment → find '=' → parse value)` loop. The shared `toml_util.h` helpers don't eliminate the outer loop duplication.
3. **`IRenderer::set_ascender` leaks a font concern** — The renderer interface exposes a text metric. The renderer should only position quads; ascender math belongs in `GridRenderingPipeline` or `RendererState`.
4. **IME `on_text_editing` is a silent no-op** — CJK and all composition-based input methods are silently broken. The hook is wired, the event is received, and the function discards it.
5. **Fixed 2048×2048 atlas** — No growth path, no multi-page support. On HiDPI screens or large fonts, atlas exhaustion is common, causing visible flicker during the full-texture re-upload on reset.
6. **Hardcoded non-configurable keybindings** — Ctrl+Shift+C/V, Ctrl+±/0, and F12 are baked into `wire_window_callbacks()` with no binding table and no documentation.
7. **Renderer state scalars duplicated across backends** — `cell_w_`, `cell_h_`, `ascender_`, `padding_`, `pixel_w_`, `pixel_h_` appear in both `VkRenderer` and `MetalRenderer` with no shared base. Every change must be applied twice.
8. **Magic hardcoded background color** — The initial background `(0.1, 0.1, 0.1)` in `RendererState::relayout()` and the clear color in both renderers is not derived from the Neovim colorscheme; it flashes before `default_colors_set` arrives.
9. **`ext_multigrid: false` disables floating windows** — Completion menus, LSP hover, and all floating window plugins are non-functional. This is a significant compatibility gap with modern Neovim plugin ecosystems.
10. **`pump_once` is monolithic** — A 60+ line function combining event polling, notification draining, blink state advancement, frame rendering, and deadline calculation. It is impossible to test any single concern in isolation.

---

## Best 10 Features to Add for Quality of Life

1. **IME / composition input** — Implement `on_text_editing` to show an in-progress composition string (underlined, inline) using the overlay cell mechanism. This unlocks CJK, Korean, Arabic, and all other composition-based input for a global user base.
2. **`ext_multigrid` support** — Each Neovim grid gets its own z-layer and pixel offset. Floating windows, completion menus, and telescope pickers all require this. It is the single biggest compatibility gap with current Neovim plugin ecosystems.
3. **User-configurable keybindings** — A `[keybindings]` section in `config.toml` mapping action names to key chords. This eliminates the hardcoded clipboard and font-size shortcuts and opens the door to user customization without rebuilding.
4. **Dynamic atlas growth** — Replace the single fixed-size atlas with a multi-page or power-of-two-growable atlas. When the current page is full, allocate a second one and bind it alongside the first in the shader. Eliminates the visible-flicker atlas reset.
5. **Configuration hot-reload** — Watch `config.toml` with a file-system notification or periodic stat, and apply changes (font path, font size, fallback paths) at runtime without restarting nvim.
6. **Smooth / fractional scrolling** — SDL3 provides high-resolution wheel deltas. Accumulate sub-cell fractional scroll and render the viewport with a pixel-level vertical offset instead of snapping to whole cell rows on every wheel tick.
7. **Font variant loading (bold/italic faces)** — Load separate font files for bold, italic, and bold-italic when they exist alongside the primary face. Use the `STYLE_FLAG_BOLD` / `STYLE_FLAG_ITALIC` bits already present in `GpuCell.style_flags` to select the correct face at rasterize time.
8. **Window background opacity** — Pass the Neovim background alpha through to the compositor. On Windows (DWM) and macOS (NSWindow alpha), a configurable `background_opacity` in `config.toml` allows translucent terminal backgrounds.
9. **Hyperlink / URI follow** — Neovim's `nvim_buf_get_extmarks` + `url` virtual text / `OSC 8` can mark cell regions as hyperlinks. A Ctrl+click handler that calls the platform open-URL API (`ShellExecute` / `open`) would make README previews and LSP documentation navigable.
10. **Per-display DPI recalibration on window move** — The current code reads DPI once at window creation. When the window moves to a monitor with a different scale factor (`SDL_EVENT_WINDOW_DISPLAY_CHANGED`), recalculate cell metrics and re-request a grid resize.

---

## Best 10 Tests to Add for Stability

1. **`GridRenderingPipeline::flush` unit test** — Wire a fake `IRenderer` (recording calls to `update_cells`, `set_atlas_texture`, `update_atlas_region`) and verify that dirty cell updates reach the renderer exactly once per flush, that a full atlas upload fires after `force_full_atlas_upload()`, and that an atlas reset triggers a second flush attempt.
2. **`UiRequestWorker` resize coalescing test** — Start the worker, post three resize requests in rapid succession, stop the worker, and assert that only the last request was dispatched to the fake RPC channel. Tests the debounce logic in the condition variable loop.
3. **`App` startup resize pending path** — Use `AppOptions` with a small initial window, call `initialize()`, inject a `grid_resize` event before the first `flush`, then inject a window resize event, and verify the pending resize is dispatched after the first flush and not before.
4. **Atlas overflow recovery end-to-end** — Use a `GlyphCache` with `atlas_size=32`, fill it past capacity, and verify that `consume_overflowed()` returns true exactly once, that subsequent `get_cluster` calls succeed after reset, and that `atlas_reset_count` increments correctly in `TextService`.
5. **`RendererState` block cursor reverse-color path** — Test `apply_cursor` with `use_explicit_colors = false` to verify that `fg` and `bg` are swapped in the GPU cell, and that `restore_cursor` returns the original values. Currently only the explicit-color path is exercised in the test suite.
6. **`AppConfig` round-trip with filesystem** — Write a config to a temp file via `AppConfig::save()`, read it back via `AppConfig::load()`, and assert field equality. Tests the path-resolution and `create_directories` code paths that the existing pure-string tests skip.
7. **`change_font_size` grid reflow** — Call `change_font_size` with a known PPI and window size, and verify that `grid_cols` and `grid_rows` are recalculated correctly from the new cell metrics, and that a resize RPC request is queued with the right dimensions.
8. **`on_resize` with zero pixel dimensions (minimized window)** — Call `App::on_resize(0, 0)` and verify no division-by-zero or zero-dimension grid is produced; the `if (new_cols < 1) new_cols = 1` guard should fire and produce a 1×1 minimum grid.
9. **`NvimInput::paste_text` uses `nvim_paste`** — Verify that `paste_text` dispatches a `nvim_paste` RPC request (not `nvim_input`) with the correct string payload and chunked=false. Currently only `nvim_input` keyboard paths are tested.
10. **`UiEventHandler::handle_option_set` forwards ambiwidth** — Test that an `option_set` redraw event with `ambiwidth=double` causes a subsequent `grid_line` event to produce double-width cells for U+00A7, and that `ambiwidth=single` reverts them. The existing test exercises width classification but not the live option-set→grid interaction.

---

## Worst 10 Existing Features

1. **IME composition (silent no-op)** — `on_text_editing` discards composition events. CJK input shows nothing in the composition phase, then commits garbled text. This is a regression from any system terminal.
2. **Single fixed-size glyph atlas with full re-upload on reset** — When the atlas fills, the app uploads all 16 MB of RGBA data in one synchronous call (`set_atlas_texture`) that blocks the render thread. On HiDPI displays this is a perceptible freeze.
3. **Hardcoded clipboard shortcuts not matching platform conventions** — Ctrl+Shift+C/V is a Linux terminal convention. macOS users expect Cmd+C/V, and Windows GUI apps expect Ctrl+C/V without Shift. There is no way to reconfigure these.
4. **No floating window support (`ext_multigrid: false`)** — Telescope, nvim-cmp, LSP hover, and virtually every modern Neovim plugin that opens a floating window renders incorrectly or not at all.
5. **F12 debug overlay not discoverable** — The only way to trigger the debug overlay is to press F12 and notice the counter in the corner. There is no mention of this in any documentation, `--help` output, or startup message.
6. **`render_test.cpp` BMP I/O is hand-rolled** — The BMP reader/writer is ~150 lines of manual byte manipulation. This works, but the pixel swap (BGRA→RGBA) is duplicated between `write_bmp_rgba` and `read_bmp_rgba` in opposing directions, making it error-prone to maintain.
7. **Startup commands are sequential blocking RPC requests** — Each command in `options_.startup_commands` is a synchronous `rpc_.request("nvim_command", ...)`. For scenarios with 10+ commands, startup latency is measurable. These could be batched with `nvim_exec2` or pipelined.
8. **No user-facing `--help` or version flags** — Running `megacitycode.exe --unknown-flag` silently ignores unknown flags and starts normally. There is no `--help` output, no `--version`, and the only documented flags are in `parse_args`.
9. **`MEGACITYCODE_LOG_CATEGORIES` env var silently drops unknown category names** — `parse_category_list` discards unrecognized tokens without warning. A typo like `MEGACITYCODE_LOG_CATEGORIES=rpc,nvim,rendrer` gives no feedback that `rendrer` was ignored.
10. **Window title update race on startup** — The window title defaults to "MegaCityCode" and is only updated when Neovim sends a `set_title` event. If the user opens a file via startup command, the title stays "MegaCityCode" until the first `flush` cycle. The title should be updated eagerly from the startup command filename if available.

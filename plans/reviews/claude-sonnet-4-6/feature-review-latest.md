# Feature Review — Spectre
**Reviewer:** claude-sonnet-4-6
**Date:** 2026-03-14

---

## Best 5 Features to Add (Quality of Life)

### 1. Cursor blinking

Neovim sends full cursor blink timing via `mode_info_set`: `blinkwait`, `blinkon`, `blinkoff` (milliseconds). `ModeInfo` (`nvim.h:224`) currently extracts `cursor_shape`, `cell_percentage`, and `attr_id` but silently ignores all blink fields. The cursor is always on.

**Impact:** High. A non-blinking cursor is one of the most noticeable differences from a real terminal. Most users set `guicursor` explicitly because of this.

**Implementation:** Add `int blinkwait, blinkon, blinkoff` to `ModeInfo`. In `App::pump_once()`, check elapsed time since last blink event and toggle a `cursor_visible_` bool. Set the dirty-frame flag when the cursor flips. Pass `cursor_visible_` into `on_flush()` to conditionally call `renderer_->set_cursor()` or hide it. Estimated: small — a timer and one extra bool, no architectural change.

---

### 2. User configuration file

The primary font is hardcoded to JetBrains Mono Nerd Font with a static search list (`text_service.cpp:37–62`). Initial window size is hardcoded at 1280×800 (`app.cpp:29`). There is no way for a user to configure either without recompiling.

**Impact:** High. Every user with a different preferred font (Cascadia Code, Fira Code, etc.) hits a silent fallback to `fonts/JetBrainsMonoNerdFont-Regular.ttf` which only works from the build directory.

**Implementation:** A minimal `~/.config/spectre/config.toml` (or `%APPDATA%\spectre\config.toml`) read at startup. Keys: `font.path`, `font.size`, `window.width`, `window.height`. `TextService::initialize()` already accepts a font path and point size — just pass values read from config instead of hardcoded defaults. Estimated: small — `TextService` and `App::initialize()` already accept these as parameters.

---

### 3. Window title tracking

`window_.initialize("Spectre", 1280, 800)` (`app.cpp:29`) hardcodes the title. Neovim sends `set_title` events in the `redraw` stream. `UiEventHandler::process_redraw()` (`ui_events.cpp:8`) dispatches against event names but `set_title` is not handled at all — the string is silently dropped.

**Impact:** Medium-high. Users with multiple Spectre windows cannot distinguish them. Most Neovim UI frontends show the current file path in the title bar.

**Implementation:** Add `on_set_title = std::function<void(const std::string&)>` to `UiEventHandler`. Handle `set_title` in `process_redraw()`. Wire it in `App::initialize()` to call `window_.set_title(title)`. Add `set_title(const std::string&)` to `IWindow` / `SdlWindow` (`SDL_SetWindowTitle`). Estimated: 20 lines across 4 files.

---

### 4. Clipboard paste (Ctrl+V / system clipboard)

There is no clipboard integration. Neovim has a `nvim_paste` API call that inserts text as if it were typed. Without this, users must configure a Neovim clipboard provider (`g:clipboard`) themselves.

**Impact:** Medium. Pasting from the system clipboard is basic terminal behavior. New users will discover this missing immediately.

**Implementation:** In `App::initialize()`, wire a key handler for Ctrl+V (before forwarding to `input_.on_key()`): call `SDL_GetClipboardText()`, then `rpc_.request("nvim_paste", { NvimRpc::make_str(text), NvimRpc::make_bool(true), NvimRpc::make_int(-1) })`. Handle multi-line clipboard content. Estimated: ~15 lines in `app.cpp`.

---

### 5. Bold and italic font variant loading

`HlAttr` has `bold` and `italic` fields. `GpuCell` has `style_flags` with bold/italic bits. But `TextService` loads only a single regular font face. When Neovim renders `bold` or `italic` text, the glyph is looked up in the regular face — no actual bold or italic glyphs are used. The style flags reach the GPU shader but there are no alternate glyph shapes.

**Impact:** Medium. Code with syntax highlighting differentiates keywords, comments, and types via bold/italic. All currently render in the same weight and slant.

**Implementation:** In `TextService`, accept optional paths for bold/italic/bold-italic variants (or discover them via font family name). In `resolve_cluster()`, select face based on the `HlAttr` style flags. `GlyphCache` already keys by `(FT_Face, text)` so the cache handles multiple faces correctly. Estimated: medium — requires threading style flags through the pipeline from `GridRenderingPipeline` to `TextService::resolve_cluster()`.

---

## Best 5 Tests to Add (Stability)

### 1. `GridRenderingPipeline` unit test

The pipeline (`app/grid_rendering_pipeline.cpp`) is the central data path — dirty cells → highlight resolution → cluster lookup → `CellUpdate` → renderer. It currently has zero direct unit tests; it's only exercised through integration.

**What to test:**
- A dirty cell with a known `HlAttr` produces a `CellUpdate` with correct `fg`/`bg` colors
- A double-width continuation cell (`double_width_cont = true`) produces no glyph lookup
- `force_full_atlas_upload()` followed by `flush()` calls `set_atlas_texture()` even when dirty cells is empty (this is the latent bug in the current code — test would expose it)
- `flush()` on a clean grid is a no-op

**Why it matters:** This is the most complex logic in the updated codebase. The early-exit-before-atlas-upload bug would be caught immediately.

---

### 2. `Grid::scroll()` with double-width cells

`grid_tests.cpp` tests basic scroll but does not test double-width characters crossing a scroll boundary.

**What to test:**
- Scroll up by 1 where row `n` contains a double-width cell at column `c` — verify the continuation cell at `c+1` is correctly carried or cleared
- Scroll clears a double-width cell that straddles the boundary of the scrolled region (right edge of `left..right` region)
- Partial-column scroll (`left > 0`) doesn't corrupt cells to the left of the region

**Why it matters:** Double-width handling in `set_cell()` carefully manages continuation cells. The scroll path copies cells blindly without `set_cell()` logic — it could leave inconsistent continuation state.

---

### 3. RPC error response propagation

`rpc_integration_tests.cpp` exercises the happy path (request/response round-trip). The error path is the one that matters at startup: if `nvim_ui_attach` fails, the app must detect it.

**What to test:**
- Fake server returns `[1, msgid, "error message", null]` — verify `RpcResult::is_error()` is true
- Fake server returns `[1, msgid, null, result]` with valid result — verify `ok()` is true and `result` is accessible
- Request timeout (fake server doesn't respond within 5s) — verify `transport_ok = false` and the call doesn't block beyond the timeout

**Why it matters:** `App::initialize()` now checks `attach.ok()` — this path must be reliable or startup failures are silent.

---

### 4. `TextService` font fallback routing

`text_service.cpp` contains `resolve_font_for_text()` which routes clusters to the primary or a fallback font. The logic (cache miss → try primary → try fallbacks → default to primary) is non-trivial and currently untested.

**What to test:**
- A cluster renderable by the primary font → returns primary face
- A cluster not in the primary font but in a fallback → returns fallback face
- Cache hit on second call for same text — no `can_render_cluster` call
- After `set_point_size()`, cache is cleared and fallback shapers are reinitialized

**Why it matters:** Silent fallthrough to the primary font when a fallback would have worked means missing glyphs render as tofu. The fallback logic currently has no regression coverage.

---

### 5. Smoke test in CI (`app/` level)

`App::run_smoke_test()` (`app.cpp:160`) exists and is well-structured: it runs the pump loop until both a `flush` event and a rendered frame are observed, or times out. But it is not wired into `CMakeLists.txt` as a CTest test.

**What to add:**
- A minimal `app_smoke_test.cpp` target that calls `App::initialize()`, `App::run_smoke_test(2s)`, `App::shutdown()`, and exits 0/1 on pass/fail
- Add it to `CMakeLists.txt` as a CTest test (requires `nvim` on PATH — can be guarded with `find_program(NVIM nvim)`)

**Why it matters:** The current CI (`spectre-tests`) has no coverage of `App`, `GridRenderingPipeline`, or the renderer factory. The smoke test would catch: startup regressions, font load failures, window/renderer init breakage, and nvim attach failures — all at CI time instead of at runtime.

---

## Worst 5 Features (Existing — Poorly Implemented or Missing)

### 1. Font discovery silently falls back to build-relative path

`text_service.cpp:64–73`:
```cpp
std::string resolve_primary_font_path() {
    for (const auto& path : primary_font_candidates())
        if (std::filesystem::exists(path))
            return path;
    return "fonts/JetBrainsMonoNerdFont-Regular.ttf";  // silent fallback
}
```

If no candidate exists, it returns a relative path that only resolves correctly when run from the build directory. The failure mode is not "error at startup" but "font not found, FreeType fails, app exits" — with no message explaining what happened. Users with a different Nerd Font variant get no guidance.

**Problem:** Silent failure, poor user experience, not extensible.

---

### 2. Main loop renders every frame unconditionally

`app.cpp:152–158`: `pump_once()` calls `begin_frame()`/`end_frame()` on every iteration regardless of whether any content changed, a cursor blinked, or the window was even visible. On Windows with Vulkan this is a continuous GPU submit loop burning power at idle.

**Problem:** Significant battery and CPU waste. Visible in task manager as constant GPU activity even with no typing or cursor movement.

---

### 3. No cursor blinking

The cursor is always visible, always on. `ModeInfo` ignores `blinkwait`/`blinkon`/`blinkoff` from Neovim's `mode_info_set`. This is the single most noticeable difference from running nvim in a real terminal.

**Problem:** Missing standard terminal behavior, affects usability.

---

### 4. Window size not persisted

Initial window size is hardcoded at 1280×800. If the user resizes the window, the next launch returns to 1280×800. There is no save/restore of window geometry.

**Problem:** Constant friction for users with non-standard setups. Easy fix: write `window.width`/`window.height` to the config file on shutdown, read on init.

---

### 5. Atlas full-upload on every font size change

`app.cpp:285` in `change_font_size()`: `force_full_atlas_upload()` triggers a full 2048×2048 R8 texture upload (4 MB) on every font size change, and then `mark_all_dirty()` causes every cell to be re-shaped and re-rasterized into the atlas on the same frame. The atlas reset is correct (new pixel sizes for glyphs), but the implementation serializes shaping + rasterization + full texture upload into a single frame, causing a visible stutter on large grids.

**Problem:** Causes a noticeable hitch on font size change. The per-cell re-shaping could be deferred (lazy rasterization on demand) and the atlas upload could be incremental (only upload as new glyphs are rasterized). The infrastructure for incremental uploads (`update_atlas_region`) is already in place but bypassed by `force_full_atlas_upload_`.

# Code Review — Spectre
**Reviewer:** claude-sonnet-4-6
**Scope:** Full codebase — all libs, app, tests, build system

---

## Summary of Changes Since Last Review

The codebase has been significantly refactored. All five major seams identified in the previous review have been addressed:

| Previous finding | Status |
|-----------------|--------|
| `spectre-nvim → spectre-grid` dep | ✅ Fixed — `IGridSink` in `spectre-types`, `UiEventHandler` uses `IGridSink*` |
| `MpackValue` unsafe union | ✅ Fixed — `std::variant` storage, typed accessors throw on bad access |
| No `RpcResult` | ✅ Fixed — `RpcResult` with `ok()`, `is_error()`, `transport_ok` |
| `GridRenderingPipeline` in App | ✅ Fixed — extracted to `app/grid_rendering_pipeline.h/cpp` |
| `TextService` / `FontService` in App | ✅ Fixed — `TextService` in `spectre-font`, owns fallback chain, atlas, shaping |
| Renderer CPU-side duplication | ✅ Fixed — `RendererState` in `spectre-renderer/src/renderer_state.h` |
| No unified logging | ✅ Fixed — `spectre-types/include/spectre/log.h` with levels and categories |
| `option_set` not wired | ✅ Fixed — `UiOptions` in `spectre-types`, wired in `App::initialize()` |
| Mouse modifiers missing | ✅ Fixed — all mouse events carry `uint16_t mod`, `NvimInput` sends modifier strings |
| Atlas size duplicated | ✅ Fixed — `GLYPH_ATLAS_SIZE` in `spectre-types/include/spectre/types.h` |
| Fixed 65536-byte RPC buffer | ✅ Fixed — `read_buf_.resize(256 * 1024)` dynamic vector |
| `flush_count_` dead code | ✅ Removed |
| `Grid::scroll()` no bounds check | ✅ Fixed — assert + early return |
| Renderer/RPC/logging tests missing | ✅ Added — `renderer_state_tests.cpp`, `rpc_codec_tests.cpp`, `rpc_integration_tests.cpp`, `log_tests.cpp`, `unicode_tests.cpp` |

App is now genuinely a thin orchestrator: 50-line header, ~320-line implementation covering only init, event loop, resize, and font-size change. The architecture is in good shape.

---

## Remaining Issues

### Major

#### 1. Main loop busy-spins — no dirty-frame gating

`app.cpp:152–158`:
```cpp
void App::run() {
    while (running_) {
        pump_once();
    }
}
```

`pump_once()` always calls `renderer_->begin_frame()` / `renderer_->end_frame()` even when no Neovim events arrived and no grid cells are dirty. On a 200×50 grid this is a ~1 MB buffer write and a full GPU submit on every iteration, burning CPU and GPU continuously even while the editor is idle.

**Fix:** Gate frame submission behind a dirty flag. Set it on `on_flush()`, cursor blink timer, window resize. Skip `begin_frame`/`end_frame` when clean.

#### 2. RPC accumulator uses O(n) erase from front

`rpc.cpp:147`:
```cpp
accum.erase(accum.begin(), accum.begin() + consumed);
```

Called after every parsed message. For large initial screen paints (many `grid_line` events), this is `O(messages × total_bytes)`. On a 200×50 grid initial redraw this can involve tens of thousands of bytes being shifted repeatedly.

**Fix:** Track a read-position index into the vector; only compact (or swap with tail) once the inner while loop finishes. Or replace with a deque.

#### 3. `read_failed_` is never reset

`nvim.h:215`, `rpc.cpp:98,128`:
```cpp
std::atomic<bool> read_failed_{ false };
```

Set on any pipe read or write error; never cleared. After the first transient I/O error the RPC layer is permanently broken for the session lifetime. Either document this as intentionally latching (and add a comment explaining why), or add a reset path.

#### 4. `MpackValue::Ext` in Type enum is dead code

`nvim.h:72`:
```cpp
enum Type { Nil, Bool, Int, UInt, Float, String, Array, Map, Ext };
```

`type()` maps `storage.index()` 0–7 to the first 8 variants. `Ext` is never returned — the variant has no Ext alternative. Neovim uses msgpack ext types for Buffer/Window/Tabpage handles. Either remove `Ext` from the enum (and document that ext types are silently ignored) or add an Ext storage type and decode it.

---

### Significant

#### 5. `GridRenderingPipeline::flush()` early-exits before atlas upload

`grid_rendering_pipeline.cpp:23–24`:
```cpp
auto dirty = grid_->get_dirty_cells();
if (dirty.empty())
    return;
```

This returns before checking `force_full_atlas_upload_`. Scenario: `force_full_atlas_upload()` is called from `on_grid_resize`, but no cells are marked dirty at that point. When `flush()` is eventually called, the atlas upload is silently skipped.

In practice Neovim always follows `grid_resize` with `grid_line` events before `flush`, so dirty cells arrive in time. But the correctness of atlas upload depends on that ordering guarantee — which is not enforced in the code.

**Fix:** Check `force_full_atlas_upload_` before the early-exit, or decouple atlas upload from the cell dirty check.

#### 6. `Cell::codepoint` is redundant

`grid.h:14`:
```cpp
struct Cell {
    std::string text = " ";
    uint32_t codepoint = ' ';  // always derivable from text
    ...
};
```

`codepoint` is stored on every `set_cell()` via `utf8_first_codepoint(text)`. `GridRenderingPipeline` uses only `cell.text` for cluster resolution. `codepoint` is never read downstream for any rendering or layout decision. It's 4 bytes per cell (~200 KB for a large grid) and one more field to keep in sync.

If it exists as a performance cache to avoid repeated UTF-8 decode, document that. Otherwise remove it.

#### 7. `Grid::scroll()` double-marks dirty

`grid.cpp:131–137`:
```cpp
// After the copy loops (which already set dirty = true on each copied cell):
for (int r = top; r < bot; r++)
    for (int c = left; c < right; c++)
        cells_[r * cols_ + c].dirty = true;
```

Every cell touched by the copy loops already has `dirty = true` set inline. The trailing loop re-marks the same region unconditionally and is harmless but redundant. Remove it.

#### 8. `on_cursor_goto` exposed but never wired

`nvim.h:252`:
```cpp
std::function<void(int, int)> on_cursor_goto;
```

App wires `on_flush`, `on_grid_resize`, and `on_option_set` but not `on_cursor_goto`. Cursor position is read from `ui_events_.cursor_col()/cursor_row()` directly in `on_flush()`. Either remove the callback from the public interface or wire it.

#### 9. Notification queue is unbounded

`nvim.h:208`:
```cpp
std::queue<RpcNotification> notifications_;
```

The reader thread pushes to this queue with no size limit. If the main thread stalls (heavy atlas upload, resize, system sleep) while Neovim is active, the queue grows without bound. A bounded queue with drop-oldest or backpressure would cap memory use.

#### 10. `App` has redundant `grid_cols_`/`grid_rows_`

`app.h:49`:
```cpp
int grid_cols_ = 0, grid_rows_ = 0;
```

`grid_.cols()` and `grid_.rows()` are the canonical size. `App::grid_cols_/rows_` are updated separately in `on_grid_resize`. They're always supposed to match but there's no enforcement. `on_flush()` doesn't actually use them for sizing — they're passed to `nvim_ui_try_resize` calls. Could use `grid_.cols()/rows()` directly.

#### 11. `FontManager` still exposes FreeType/HarfBuzz types in public header

`font.h:32–38`:
```cpp
FT_Face face() const { return face_; }
hb_font_t* hb_font() const { return hb_font_; }
```

`TextService` uses these internally for `GlyphCache::get_cluster(text, face, shaper)`. Any code including `<spectre/font.h>` pulls in `<ft2build.h>` and `<hb.h>`. Not a problem today since `font.h` stays within `spectre-font`, but swapping the font backend would still require touching these types in the shared header.

---

### Minor / Quick wins

| # | Location | Issue | Fix |
|---|----------|-------|-----|
| 1 | `events.h:22` | `TextInputEvent::text` is `const char*` — lifetime depends on SDL event buffer, unclear to callers | Document lifetime or change to `std::string` |
| 2 | `grid.cpp:17–23` | `clear_continuation()` results in `text = ""` while normal blank cells have `text = " "` | Set `cell.text = " "` in `clear_continuation` for consistency |
| 3 | `input.cpp:34–129` | Key translation missing: numeric keypad (KP_0–KP_9, KP_ENTER, KP_Plus, etc.), F13–F24 | Add missing keycodes |
| 4 | `ui_events.cpp:86` | `UiOptions{}` default-constructed on every `handle_grid_line` call when `options_` is null — `options_` should always be set by App | Assert `options_` non-null or remove the null path |
| 5 | `nvim.h:317` | `NvimInput::mouse_button_` stores the active button for drag, but rapid click sequences could leave stale button state | Tie stored button to per-button tracking |

---

## Testing Gaps

The test suite has grown substantially. Current coverage:

| Module | Coverage |
|--------|----------|
| `spectre-types` (unicode, log, highlight) | ✅ Good |
| `spectre-grid` | ✅ Good |
| `spectre-nvim` (ui_events, input, rpc codec, rpc integration) | ✅ Good |
| `spectre-renderer` (RendererState) | ✅ Basic coverage |
| `spectre-font` | ✅ Basic coverage |
| `spectre-window` | ❌ Zero coverage |
| `app/` | ⚠️ Smoke test only (`run_smoke_test`) |

Remaining gaps worth filling:

1. **`GridRenderingPipeline`** — no unit tests. Test: dirty cell → correct `CellUpdate` values; atlas upload triggered on force/dirty; early-exit behavior on empty dirty with `force_full_atlas_upload_` set.
2. **`Grid::scroll()` edge cases** — partial-column scrolls, double-width cell across a scroll boundary, scroll by region exactly equal to height.
3. **Font fallback path** — `TextService::resolve_font_for_text()` and fallback selection have no test exercising the fallback chain.
4. **`MpackValue::Ext` type** — unhandled in code, untested, not documented.
5. **Renderer backend parity** — `RendererState` is tested but there is still no test verifying that the Vulkan and Metal backends produce identical `GpuCell` layouts for the same input sequence.

---

## What Is Good

- **App** is now a genuine thin orchestrator — ~320 lines of init, loop, resize, and font-size change. No policy logic.
- **`IGridSink`** cleanly decouples `spectre-nvim` from `spectre-grid`. Both sides independently testable.
- **`MpackValue`** with `std::variant` — safe, idiomatic, throws on bad access.
- **`RpcResult`** propagates Neovim errors to callers. `attach` and `resize` both check `ok()`.
- **`TextService`** owns the full font pipeline. App has no FreeType or HarfBuzz types.
- **`RendererState`** eliminates CPU-side duplication between Vulkan and Metal backends.
- **Unified logging** — consistent categories and levels across all modules.
- **Mouse modifiers** now threaded end-to-end.
- **`rpc_integration_tests.cpp`** with real fake-server round-trip — high confidence for the RPC layer.
- **`run_smoke_test()`** — CI-friendly startup verification that a flush and a frame both complete.
- **`replay_fixture.h`** — still the best piece of test infrastructure in the repo.

---

## Recommended Priority Order

**Do first (correctness/performance):**
1. Main loop dirty-frame gate — stops CPU/GPU burn while idle
2. RPC accumulator O(n) erase — replace with index-based consumption

**Do next (robustness):**
3. Fix `GridRenderingPipeline::flush()` early-exit before atlas upload
4. Remove or document `Cell::codepoint`
5. Remove `Grid::scroll()` redundant dirty loop

**Do later (cleanup):**
6. Remove or implement `MpackValue::Ext`
7. Remove `on_cursor_goto` from `UiEventHandler` if it won't be wired
8. Consolidate `App::grid_cols_/rows_` with `grid_.cols()/rows()`
9. Add `GridRenderingPipeline` unit tests
10. Add `Grid::scroll()` double-width edge case tests

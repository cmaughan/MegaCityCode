# Code Review — Spectre
**Reviewer:** gpt-5  
**Scope:** Current tree review across app, libs, tests, scripts, and CI

---

## Overall Read

The repo is in much better shape than a typical small GUI frontend. The module split is real, the recent seam extractions are meaningful, and the test surface is no longer token. The architecture is now good enough that most future work can be done in isolated slices instead of always colliding in `app.cpp`.

That said, a few hot-path and long-session issues are still visible in the current code. The biggest remaining problems are not “the design is wrong,” but “the hot paths still have avoidable whole-buffer / whole-grid / whole-vector work.”

---

## Findings

### 1. High: The main loop still busy-spins and submits frames while idle

Relevant code:

- `app/app.cpp:152`
- `app/app.cpp:179`
- `app/app.cpp:209`
- `app/app.cpp:212`

`App::run()` is still an unconditional `while (running_) { pump_once(); }` loop. `pump_once()` still calls `renderer_->begin_frame()` / `renderer_->end_frame()` every iteration, even when:

- no SDL events arrived
- no RPC notifications arrived
- no grid cells are dirty

That means the app can keep burning CPU and GPU while the editor is visually idle.

**Why it matters for maintainability and agent work**

- Performance bugs stay noisy and cross-cutting because “idle” is not a first-class state.
- Renderer changes are harder to reason about because the app is always driving frames.
- This increases the chance that future agents misdiagnose renderer cost when the real problem is loop policy.

**Recommendation**

Add a frame-dirty gate in `App`:

- set it on redraw flush, cursor blink, resize, and atlas upload
- skip `begin_frame` / `end_frame` when clean
- optionally sleep or wait on events when both window and RPC sides are idle

---

### 2. High: The RPC accumulator still does O(n) front-erases

Relevant code:

- `libs/spectre-nvim/src/rpc.cpp:115`
- `libs/spectre-nvim/src/rpc.cpp:147`

The reader thread accumulates bytes in a `std::vector<uint8_t> accum`, then does:

- `accum.erase(accum.begin(), accum.begin() + consumed);`

for each parsed message.

That is still a classic front-erase hot path. Large initial redraws or redraw bursts can repeatedly shift the remaining bytes.

**Why it matters**

- It adds avoidable work exactly where startup and redraw throughput matter.
- It makes future profiling noisy because the decode loop cost scales with message burst shape, not just payload size.

**Recommendation**

Track a read index into the buffer and compact only when needed, or swap to a deque/ring-buffer style reader. This is a contained change with very good payoff.

---

### 3. Medium: Dirty-grid flush still scans and clears the whole grid every flush

Relevant code:

- `app/grid_rendering_pipeline.cpp:17`
- `app/grid_rendering_pipeline.cpp:22`
- `app/grid_rendering_pipeline.cpp:60`
- `libs/spectre-grid/src/grid.cpp:160`
- `libs/spectre-grid/src/grid.cpp:166`

The renderer-side cell-buffer upload path is now incremental, which is good. But the grid dirty path still works like this:

1. `GridRenderingPipeline::flush()` asks `grid_->get_dirty_cells()`
2. `Grid::get_dirty_cells()` scans the entire grid
3. after upload, `grid_->clear_dirty()` clears the entire grid

So the main flush path is still fundamentally `O(grid size)` per flush even when only a handful of cells changed.

**Why it matters**

- The recent renderer dirty-range optimization does not get full value.
- The app/pipeline hot path still depends on whole-grid iteration.
- This is a scaling pain point for large windows or plugin-heavy UIs.

**Recommendation**

Move `Grid` to a real dirty-set or dirty-span model instead of whole-grid scans. Even a row-based dirty structure would materially reduce flush overhead and make the pipeline more modular.

---

### 4. Medium: Font fallback choice caching is unbounded and string-keyed

Relevant code:

- `libs/spectre-font/include/spectre/text_service.h:52`
- `libs/spectre-font/src/text_service.cpp:230`
- `libs/spectre-font/src/text_service.cpp:243`
- `libs/spectre-font/src/text_service.cpp:251`
- `libs/spectre-font/src/text_service.cpp:256`

`TextService` caches font choice per raw cluster string in:

- `std::unordered_map<std::string, int> font_choice_cache_`

and never evicts entries until font-size or fallback reinit clears it.

In normal editing, especially with diagnostics, LSP text, plugin UIs, long logs, or very mixed Unicode, this cache can grow for the entire session.

**Why it matters**

- It creates a long-session memory growth vector.
- It couples font policy to arbitrary text diversity rather than to glyph/script classes.
- It is hard for future agents to reason about because the cache policy is “store every observed string forever.”

**Recommendation**

Replace it with a bounded cache or a cheaper key:

- codepoint/script-level fallback hints
- small LRU for cluster strings
- explicit metrics/logging so cache growth is visible

---

### 5. Medium: Glyph atlas exhaustion still degrades by silently dropping glyphs

Relevant code:

- `libs/spectre-types/include/spectre/types.h:8`
- `libs/spectre-font/src/glyph_cache.cpp:164`
- `libs/spectre-font/src/glyph_cache.cpp:178`
- `libs/spectre-font/src/glyph_cache.cpp:214`
- `libs/spectre-font/src/glyph_cache.cpp:305`

The atlas is still fixed-size (`GLYPH_ATLAS_SIZE = 2048`). When `reserve_region()` cannot fit a glyph or cluster, it logs:

- `Atlas full, cannot fit ...`

and the caller falls back to an empty region.

That means the current runtime behavior for atlas exhaustion is “text/icons start disappearing.”

**Why it matters**

- It is a hard-to-debug long-session failure mode.
- It creates user-visible degradation without recovery.
- It blocks future feature work that increases glyph diversity.

**Recommendation**

Introduce one of:

- atlas reset and relayout
- atlas growth
- simple eviction policy
- explicit “recoverable atlas overflow” path instead of empty-glyph fallback

---

### 6. Medium: Input translation is still a monolithic hardcoded switch

Relevant code:

- `libs/spectre-nvim/src/input.cpp:22`
- `libs/spectre-nvim/src/input.cpp:34`
- `libs/spectre-nvim/src/input.cpp:38`
- `libs/spectre-nvim/src/input.cpp:174`

`NvimInput::translate_key()` is still a long hand-maintained switch with modifier handling split between:

- `translate_key()`
- `mouse_modifiers()`

It works for the current supported set, but it is still awkward to extend and easy for future changes to become inconsistent.

**Why it matters**

- Agents touching input will still conflict in one file.
- It is not obvious which missing keys are intentional vs unimplemented.
- It does not scale well to IME, keypad, richer mouse, or config-driven mappings.

**Recommendation**

Move toward a data-driven key table:

- core named-key map
- modifier formatter shared across keyboard and mouse
- explicit unsupported-key test cases

---

## Testing Gaps

The test surface is solid now, but the main gaps are still around seam behavior that matters in real sessions:

1. `GridRenderingPipeline` has no direct tests.
   It is now an important seam and should have unit coverage for:
   - dirty cell projection
   - highlight resolution
   - atlas upload triggering
   - empty/space/double-width behavior

2. There is no real scripted editing flow beyond startup smoke.
   `spectre-app-smoke` proves startup, flush, and first frame. It does not prove:
   - insert mode round-trip
   - redraw after typing
   - cursor movement correctness
   - quit path correctness under normal UI use

3. Atlas exhaustion and recovery are untested.
   The current behavior is user-visible and should have an explicit regression test once recovery policy exists.

4. Font fallback/cache behavior is lightly covered, not deeply covered.
   There is no direct test for:
   - cache growth policy
   - fallback ordering
   - font choice reuse
   - failure behavior when both primary and fallback miss

5. Backend upload behavior is only indirectly tested.
   `RendererState` is covered, but there is no backend-level test that proves the right dirty slice is uploaded to the mapped buffer in Vulkan/Metal integration terms.

---

## Modularity Opportunities

1. Split `App` into bootstrap vs runtime loop responsibilities.

- `App` is much thinner than before, but it still owns window, RPC, text, grid, pipeline, and loop policy together.
- A small `RuntimeController` or `SessionController` would reduce future merge pressure.

2. Make grid dirtiness a first-class abstraction.

- Right now `Grid` is still the owner of “what changed,” but the interface is a whole-grid scan.
- A dirty tracker abstraction would let the pipeline stay small and keep performance policy out of app code.

3. Separate font discovery policy from text service mechanics.

- `TextService` still mixes:
  - font discovery heuristics
  - fallback policy
  - caching
  - atlas ownership
- That makes it a future conflict zone for both feature and perf work.

4. Move key translation data out of imperative code.

- A declarative map would make extension safer and reduce coordination cost across agents.

---

## Recommended Plan

1. Add idle-frame gating in `App`.
2. Replace RPC front-erases with indexed consumption.
3. Replace whole-grid dirty scans with dirty spans/sets in `Grid`.
4. Add bounded/observable font-choice caching.
5. Introduce recoverable atlas overflow behavior.
6. Add direct `GridRenderingPipeline` tests before more rendering features land.

---

## Top 5 Good Things

1. The library split is now real, not decorative. `spectre-types`, `spectre-grid`, `spectre-font`, `spectre-nvim`, `spectre-renderer`, and `spectre-window` each own recognizable concerns.
2. `RendererState` was the right extraction. It removed duplicated CPU-side renderer logic and made backend parity more plausible.
3. The test infrastructure is strong for a project of this size: replay fixtures, fake RPC server, real RPC integration tests, Unicode conformance corpus, renderer-state tests, and app smoke coverage.
4. Local tooling and CI are aligned. The root wrappers, repo-local scripts, README, and GitHub Actions all point at the same build/test flow.
5. The app is now a thin orchestrator rather than a bag of backend details. That is a meaningful architecture improvement.

## Top 5 Bad Things

1. The main loop still burns frames while idle.
2. The RPC reader still does front-erases on a vector in the hot path.
3. Dirty-grid collection still scans and clears the whole grid every flush.
4. Font-choice caching can grow without bound across a session.
5. Atlas overflow still degrades by dropping glyphs instead of recovering.

# Review Concensus

## Scope

This document combines the latest review outputs in `plans/reviews/review-latest.gpt.md` and `plans/reviews/review-latest.claude.md`.

- The GPT review was based on the bundled `all_code.md` snapshot only and explicitly did not build or run the app.
- The Claude review was also snapshot-based. Both reviews are useful, but runtime-sensitive claims should be treated as inferred unless already confirmed in the current tree.

## Where The Reviews Agree

### 1. `App` is still a merge hotspot

Both reviews independently call out `app/app.cpp` as too central. The agreement is not about folder structure, which is mostly good, but about orchestration logic accumulating in one class.

Shared conclusion:
- keep the top-level module split
- continue moving policy out of `App`
- make lifecycle, resize policy, cursor policy, and redraw projection easier to change without forcing all agents into the same file

### 2. Atlas handling needs a real recovery story

Both reviews identify atlas exhaustion as a correctness problem, not just a polish item.

Shared conclusion:
- silent empty glyphs are not acceptable
- the font/render path needs either eviction, rebuild, paging, or another explicit recovery mechanism
- atlas pressure needs direct regression coverage

### 3. Vulkan needs hardening and a less stall-prone upload path

Both reviews highlight Vulkan-side fragility.

Shared conclusion:
- check low-level API return paths consistently
- collapse duplicated swapchain rebuild logic
- reduce or remove synchronous GPU-idle waits in the glyph upload path

### 4. Public boundaries are still wider than they should be

Both reviews call out broad or leaky interfaces:
- `app/app.cpp` owns too much policy
- `spectre-types` is absorbing utilities as well as shared types
- the public font surface leaks engine internals and raw handles

Shared conclusion:
- keep narrowing interfaces that multiple modules depend on
- prefer hiding implementation detail over exporting more shared headers

### 5. The test base is good, but the missing tests are mostly lifecycle and failure tests

Both reviews are positive about the current test direction and both focus missing coverage on failure behavior:
- atlas pressure
- lifecycle rollback
- font fallback and highlight fidelity
- edge-case redraw semantics

Shared conclusion:
- the next test work should concentrate on failure modes and regression seams, not more generic happy-path smoke tests

## Where One Review Added Useful Detail

### GPT added useful emphasis on:

- blocking RPC round-trips on UI paths such as resize and font zoom
- possible horizontal-scroll incompleteness in `grid_scroll`
- long-session cache growth and dirty-tracking cost
- shutdown ordering and request/read-thread robustness

### Claude added useful emphasis on:

- duplicated swapchain rebuild code in Vulkan
- synchronous `vkQueueWaitIdle` behavior on atlas uploads
- raw-pointer/lifetime fragility in `GridRenderingPipeline`
- concrete missing end-user features: clipboard, config, IME, title updates, underline fidelity

## Where Claims Need Verification Before Code Changes

These should not be treated as settled facts without a quick confirmation pass:

1. Horizontal `grid_scroll` handling
The GPT review is likely directionally right, but this should be verified against current Neovim event semantics and actual traces before a refactor starts.

2. Dirty tracking cost in `Grid`
The review may be right that the scan is coarse, but this should be measured before turning it into a high-priority performance rewrite.

3. Shutdown-order fragility
Recent RPC and wake-path work has already reduced some of the earlier shutdown risk. Re-check the current tree before assuming a large redesign is needed here.

## Consensus Priority Order

The combined view is:

1. Fix correctness risks in atlas handling and Vulkan robustness first.
2. Reduce architecture friction in `App` and public API boundaries next.
3. Remove obvious UI-path blocking and redraw completeness gaps.
4. Finish missing rendering fidelity and core platform integration features.
5. Add the focused regression coverage needed to keep these changes stable.

## Work Item Set

The current work-item set derived from this consensus is:

1. `00 atlas recovery -bug`
2. `01 vulkan robustness and upload path -bug`
3. `02 app lifecycle split and rollback -feature`
4. `03 ui path rpc decoupling -bug`
5. `04 redraw completeness and dirty tracking -bug`
6. `05 font and shared api boundary cleanup -feature`
7. `06 highlight fidelity -feature`
8. `07 user configuration and font selection -feature`
9. `08 clipboard ime and title integration -feature`
10. `09 lifecycle and rendering regressions -test`

See `plans/work-items/index.md` for the per-item plans.

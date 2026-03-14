# Review Consensus

This note is the current synthesis of the review set under `plans/reviews`, reconciled against the current tree.

## Read Of The Repo

The repo is no longer in the earlier "major architecture debt" state. The large cleanup items that used to dominate the reviews have already landed:

- `App` is thinner than before and no longer owns every subsystem policy directly.
- renderer CPU-side duplication has been extracted into shared state
- the RPC contract is explicit and test-covered
- `spectre-nvim` is no longer hard-wired to the concrete grid module
- Unicode/grapheme handling and `ambiwidth` are materially better than the earlier baseline
- the repo now has native tests, smoke coverage, CI coverage, logging, and agent-facing docs

So the consensus is not "rewrite the architecture again." The consensus is that the remaining work is now mostly about hot-path efficiency, resilience, and a better product surface.

## Where The Reviews Converge

The reviews align on four broad themes:

1. The next meaningful code work is mostly in performance-sensitive paths, not gross module separation.
2. The most valuable new tests are seam-level tests around real editing flows and renderer/pipeline behavior.
3. The next user-facing wins are quality-of-life features, not another round of foundational refactors.
4. The remaining maintainability risks are concentrated in a few hotspot paths rather than the overall layout.

## Consensus Findings

### 1. Idle/render pacing is the strongest remaining code-quality issue

The architecture is cleaner, but the app still looks likely to do too much work while idle. If the main loop continues polling, draining, and rendering aggressively without meaningful state changes, that burns CPU and makes later performance work harder to reason about.

Recommendation:

- gate frame submission on actual dirtiness, animation, resize, or startup state
- avoid full work on every idle iteration
- add one focused measurement path so this is driven by observed behavior instead of guesswork

### 2. A few hot-path data structures still look heavier than they should

The current tree is much better structured, but some review comments still point at algorithmic pressure points:

- RPC buffering / accumulation
- grid dirty detection and flush behavior
- font fallback / choice caching
- atlas growth / exhaustion behavior

Some of these may already be "good enough" in practice, but they are the right places to profile and tighten before reopening broader architecture work.

Recommendation:

- profile first
- only optimize the paths that show up under real editor interaction
- prefer narrow local fixes over new abstractions

### 3. Testing should move from component confidence to behavior confidence

The repo now has good component and seam coverage, but the next gap is more integrated behavior:

- real editing-flow smoke
- renderer/pipeline correctness under realistic redraws
- atlas exhaustion / recovery behavior
- fallback-font and cache-policy behavior under repeated mixed text

Recommendation:

- keep the current unit/seam tests
- add a small number of higher-value scripted interaction tests rather than broad fragile UI automation

### 4. User-facing ergonomics are now a better investment than more structural cleanup

The reviews point toward a useful shift in priorities. The repo is now modular enough that the next valuable work is mostly feature and UX work:

- configurable font and fallback behavior
- clipboard integration
- IME / composition support
- session/window persistence
- shell/file-open integration

These are now more important than additional code-motion refactors.

## Things Already Resolved And Not Worth Re-Litigating

These were legitimate issues in older reviews, but they should not be treated as open architectural debt now unless regressions appear:

- `App` as a monolithic owner of text/render/font policy
- duplicated Vulkan/Metal CPU-side cell logic
- opaque RPC success/error handling
- `option_set` being parsed but unused
- lack of smoke coverage
- lack of repo-local agent guidance

Any future review should start from the current code, not from the older pre-refactor baseline.

## Suggested Plan

### Phase 1: Measure And Tighten Runtime Hot Paths

- check idle-loop and frame pacing under no-op editing conditions
- profile redraw-heavy interaction and look at RPC buffering, dirty-grid scans, and upload paths
- harden atlas exhaustion behavior if it still degrades by dropping glyphs without recovery

### Phase 2: Add A Small Number Of Higher-Value Integration Tests

- scripted real-editing flow against embedded Neovim
- direct `GridRenderingPipeline` behavior tests if they still do not exist
- atlas exhaustion / recovery regression coverage
- fallback-font and cache-behavior tests

### Phase 3: Prioritize QoL Features

- clipboard integration
- user-configurable font/fallback settings
- IME / compose support
- window/session persistence

### Phase 4: Revisit Smaller Maintainability Cleanup Only If Needed

- simplify monolithic translation tables if they start to slow down feature work
- tighten cache bounds if real workloads prove growth risk
- keep logging and docs aligned with runtime behavior

## Practical Guidance For Future Agents

- treat performance work as measurement-led, not speculative
- avoid reopening solved architecture seams just because they were painful historically
- prefer adding tests at the pipeline or workflow boundary instead of splitting modules further without need
- bias new effort toward product behavior and editor UX unless profiling shows a real hotspot

## Bottom Line

The current consensus is that Spectre has crossed from "needs core architecture cleanup" into "needs targeted performance, stronger workflow tests, and better end-user features." The remaining work should be incremental and evidence-driven.

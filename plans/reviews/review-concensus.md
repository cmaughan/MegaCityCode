# Review Concensus

## Scope

This document synthesizes:

- [[review-latest.gpt]]
- [[review-latest.claude]]

Both reviews were static snapshot reviews, not fresh runtime investigations. The consensus below reconciles those reviews against the current tree so already-landed work is not blindly re-opened.

## What Is Already Done And Should Not Be Reopened

The current tree already has several areas that older review cycles used to flag:

- render snapshots and blessed UI regression images
- startup smoke coverage in CTest/CI
- logging infrastructure
- user config persistence
- Unicode width conformance checks against headless Neovim
- title updates, clipboard shortcuts, and basic IME plumbing
- shared CPU-side renderer state across Vulkan and Metal

Those are not current backlog items unless a new concrete bug appears.

## Where The Reviews Strongly Agree

### 1. RPC is still the main correctness hotspot

GPT is strongest here, but Claude’s review supports the same direction indirectly through the blocking-UI and broad-surface observations.

Current verified issues:

- `NvimRpc` still writes to the pipe without a write mutex, even though requests and notifications can come from different threads.
- the UI thread still performs blocking RPC for clipboard copy/paste paths
- worker-thread shutdown can still wait on slow RPC activity

Consensus:

- harden the transport before adding more UI features on top of it
- make request ownership and shutdown behavior explicit
- remove avoidable blocking RPC from UI-facing paths

### 2. `App` is still too thick

Both reviews independently land on the same conclusion: the repo-level architecture is good, but `app/app.cpp` is still the main merge hotspot.

Current verified issues:

- cursor blink state machine
- startup and resize policy
- clipboard policy
- render-test/smoke-mode execution
- config persistence hooks
- top-level event routing

Consensus:

- keep `App` as orchestration only
- move policy-heavy subflows into smaller helpers
- make the run loop, UI request handling, and test harness paths easier to evolve independently

### 3. Test harness code is still too entangled with production code

Both reviews call this out from different angles.

Current verified issues:

- `app/render_test.cpp` is compiled into `spectre.exe`
- the test target also compiles `../app/render_test.cpp` directly
- tests still include private `src` directories from multiple libraries
- `app_config.cpp` and `render_test.cpp` still duplicate TOML-like parsing helpers

Consensus:

- extract reusable parsing/reporting helpers into a small shared implementation layer
- keep render-test mechanics available to tests and tools without making them part of the shipping app surface
- shift tests toward public seams and narrower internal test utilities

### 4. Public boundaries still leak implementation detail

The reviews agree here even when they emphasize different examples.

Current verified issues:

- `IWindow` still exposes `SDL_Window*`
- `font.h` is still only a passthrough to `text_service.h`
- `GLYPH_ATLAS_SIZE` still lives in shared `spectre-types`
- `nvim.h` still exports process, RPC, msgpack, redraw, and input concerns together

Consensus:

- reduce rebuild and merge blast radius by narrowing public surfaces
- move implementation constants and raw backend handles downward
- split broad headers when the split improves ownership, not just file count

### 5. Vulkan and redraw edge cases still deserve targeted hardening

GPT is stronger on lifecycle and grid correctness; Claude is stronger on specific Vulkan resource-management smells. Together they point at the same thing: the remaining risk is now in edge-path robustness, not the happy path.

Current verified issues or likely hotspots:

- unchecked or awkward Vulkan resource recreation paths
- synchronous atlas upload stalls
- wide-cell overwrite/repair edge behavior in `Grid`
- redraw/parser fuzz coverage still focused more on valid traffic than hostile or malformed traffic

Consensus:

- do not start with broad rewrites
- target the remaining correctness/perf hotspots surgically and back them with regression tests

## Where The Reviews Differ

### GPT review bias

GPT is more useful on:

- RPC correctness
- shutdown and threading risk
- blocking UI paths
- concrete public-boundary leaks

### Claude review bias

Claude is more useful on:

- refactoring seams
- render-test and parser duplication
- code smell cleanup
- medium-priority GUI ergonomics ideas

### Consensus resolution

The codebase is no longer in “big missing architecture” territory. The right ordering now is:

1. correctness and blocking-risk fixes
2. interface and ownership cleanup
3. test harness extraction and seam cleanup
4. lower-priority GUI/platform polish

That means the GPT concerns set the order, and the Claude concerns shape the extraction plan.

## Current Recommended Work Order

1. Serialize RPC writes and harden RPC shutdown semantics.
2. Remove blocking UI-thread RPC where it still exists.
3. Split `App` responsibilities so loop policy, request execution, and test harness logic stop colliding.
4. Extract render-test/config parsing helpers and stop compiling ad hoc test plumbing directly into both production and tests.
5. Narrow `IWindow`, `nvim.h`, and font/shared-type boundaries.
6. Add targeted redraw/grid/Vulkan failure coverage while fixing the known edge cases.
7. Only then spend time on QoL features like richer clipboard integration, IME composition UI, overlays, or tab/file chrome.

## Work Item Set

The current backlog extracted from this consensus is:

1. [[00 rpc transport serialization and shutdown hardening -bug]]
2. [[01 app loop and ui request decomposition -feature]]
3. [[02 render test extraction and parser unification -feature]]
4. [[03 public api boundary cleanup -feature]]
5. [[04 grid wide-cell repair and redraw robustness -bug]]
6. [[05 vulkan robustness and atlas upload rework -bug]]
7. [[06 test seam cleanup and lifecycle coverage -test]]
8. [[07 platform text and display integration -feature]]
9. [[08 gui ergonomics and observability -feature]]

See [[../work-items/index]] for the per-item execution notes.

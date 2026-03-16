# Review Concensus

## Scope

This document synthesizes:

- [[review-latest.gpt]]
- [[review-latest.claude]]

Both source reviews were static snapshot reviews. This refresh reconciles those notes against the current repository state so finished work is not reopened as if it were still pending.

## Current Tree Reconciliation

The main correctness and architecture concerns from the last review cycle have already been executed as concrete work items and should stay closed:

- [[../work-items-complete/00 rpc transport serialization and shutdown hardening -bug]]
- [[../work-items-complete/01 app loop and ui request decomposition -feature]]
- [[../work-items-complete/02 render test extraction and parser unification -feature]]
- [[../work-items-complete/03 public api boundary cleanup -feature]]
- [[../work-items-complete/04 grid wide-cell repair and redraw robustness -bug]]
- [[../work-items-complete/05 vulkan robustness and atlas upload rework -bug]]
- [[../work-items-complete/06 test seam cleanup and lifecycle coverage -test]]

That means the old consensus priorities around RPC hardening, `App` decomposition, render-test extraction, boundary cleanup, grid correctness, Vulkan robustness, and lifecycle coverage are no longer the active planning problem.

## Where The Reviews Still Agree

### 1. Platform-facing polish is the main remaining product gap

GPT is strongest on the concrete user-facing items here: clipboard behavior, IME composition visibility, display-following DPI behavior, and default presentation polish. Claude is directionally aligned that the remaining work is less about core architecture and more about quality of integration.

Consensus:

- keep the remaining platform work grouped as one follow-on item
- do not treat it as a correctness blocker
- leave it parked until platform UX becomes the next deliberate push

Tracked item:

- [[../work-items-icebox/07 platform text and display integration -feature]]

### 2. GUI ergonomics and observability are still useful, but optional

Claude emphasizes observability, config UX, and keeping medium-priority GUI work separate from correctness. GPT’s QoL ideas overlap on discoverability, diagnostics, and richer user affordances, even if the specific feature list differs.

Consensus:

- keep GUI ergonomics separate from platform integration
- preserve the existing split between low-risk ergonomics and larger affordances
- leave this work in the icebox until there is explicit appetite for UX investment

Tracked item:

- [[../work-items-icebox/08 gui ergonomics and observability -feature]]

## Where The Reviews Differ

### GPT review bias

GPT pushes harder on larger product-facing features:

- IME composition as a real UI feature
- configurable keybindings
- `ext_multigrid`
- dynamic atlas growth
- broader GUI convenience features

### Claude review bias

Claude pushes harder on structural cleanup:

- keeping `App` thin
- extracting reusable seams
- reducing duplication
- improving testability around subsystem boundaries

### Consensus resolution

The structural items Claude pushed are mostly the things that became work items `00` through `06`, and those are now complete. GPT still identifies real user-facing gaps, but they are no longer urgent enough to pull back into the active backlog automatically.

The current shared position is:

1. keep the active backlog empty rather than inventing new urgency
2. thaw `07` first when platform integration becomes the goal
3. thaw `08` first when ergonomics or observability becomes the goal
4. keep bigger GPT-only ideas like `ext_multigrid` or dynamic atlas growth in reserve until a concrete need justifies their scope

## Recommended Path Forward

1. Leave the active backlog empty for now.
2. If the next push is platform quality, thaw [[../work-items-icebox/07 platform text and display integration -feature]].
3. If the next push is daily UX/debuggability, thaw [[../work-items-icebox/08 gui ergonomics and observability -feature]].
4. Revisit larger feature ideas only after one of those tracks becomes active or a new bug forces reprioritization.

## Work Item Set

Active:

- none

Icebox:

- [[../work-items-icebox/07 platform text and display integration -feature]]
- [[../work-items-icebox/08 gui ergonomics and observability -feature]]

Completed from this review cycle:

- [[../work-items-complete/00 rpc transport serialization and shutdown hardening -bug]]
- [[../work-items-complete/01 app loop and ui request decomposition -feature]]
- [[../work-items-complete/02 render test extraction and parser unification -feature]]
- [[../work-items-complete/03 public api boundary cleanup -feature]]
- [[../work-items-complete/04 grid wide-cell repair and redraw robustness -bug]]
- [[../work-items-complete/05 vulkan robustness and atlas upload rework -bug]]
- [[../work-items-complete/06 test seam cleanup and lifecycle coverage -test]]

Model: Codex (GPT-5)

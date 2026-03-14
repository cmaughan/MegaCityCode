# Review Consensus Execution Plan

## Goal

Execute the agreed consensus refactor in small, buildable slices that reduce coupling first and defer broad churn until seam tests exist.

## Phase Order

1. Add seam tests first.
2. Cut `spectre-nvim -> spectre-grid`.
3. Introduce `RpcResult` and safer RPC handling.
4. Replace `MpackValue` with a safer representation.
5. Extract `TextService` from `App`.
6. Extract the shared CPU renderer front-end.
7. Wire editor UI options and input fidelity fixes.
8. Finish the cleanup and performance pass.

## First Slice

This change set starts with the highest-leverage small seam fixes:

- add a grid sink interface so `spectre-nvim` no longer depends directly on `spectre-grid`
- move shared highlight types to `spectre-types`
- update tests/build wiring to prove `UiEventHandler` can work without the concrete grid type
- if churn stays contained, follow immediately with `RpcResult`

## Status

Completed in the first landing slice:

- stored this execution note
- added `IGridSink` in `spectre-types`
- moved shared highlight types out of `spectre-grid`
- cut the direct `spectre-nvim -> spectre-grid` build dependency
- introduced `RpcResult` and updated app request callsites
- added a `UiEventHandler` test that uses a fake grid sink

Completed in the follow-up execution slices:

- migrated `MpackValue` onto a `std::variant`-backed representation
- extracted a testable MsgPack codec seam and replaced fixed-size RPC write buffers
- extracted `TextService` from `App` for font/fallback/shaping/cache ownership
- extracted `RendererState` and moved both Vulkan and Metal onto the shared CPU-side grid/cursor path
- extracted `GridRenderingPipeline` from `App` for grid/highlight/text-to-renderer projection
- wired `option_set` into shared `UiOptions`, including `ambiwidth`-aware width decisions
- implemented atlas dirty-rect uploads so the renderer partial-update path is now exercised
- added seam tests for renderer state, MsgPack/RPC codec behavior, `ambiwidth`, and double-width grid edge cases
- revalidated the tree with `t.bat`, a short `r.bat --console` smoke run, and a repo-style format pass

## Guardrails

- every slice leaves `t.bat` green
- renderer-adjacent slices also get a short `r.bat --console` smoke run
- avoid combining `App`, renderer, and RPC refactors in one uncontrolled patch

# 05 Vulkan Robustness And Atlas Upload Rework

## Why This Exists

The Vulkan path is usable, but both reviews still found fragile allocation/recreation behavior and unnecessary synchronous waits.

Current reviewed issues:

- awkward or unchecked resource recreation paths in Vulkan buffer/context code
- atlas upload path still too stall-prone
- swapchain/resource failure handling can be cleaner and easier to reason about

## Goal

Make the Vulkan backend less fragile under resize/resource pressure and reduce unnecessary GPU/CPU stalls on glyph uploads.

## Implementation Plan

- [x] Audit recreation/error paths.
  - [x] check allocation return values consistently
  - [x] avoid destroying the old resource before the replacement is known-good where possible
- [x] Simplify swapchain/resource rebuild flow.
  - [x] reduce duplicated rebuild sequencing
  - [x] keep failure cleanup explicit
- [x] Rework atlas upload scheduling.
  - [x] move away from fully synchronous per-upload waits by batching atlas uploads into the frame path
  - [x] keep pending full-atlas snapshots coherent when later dirty-region uploads arrive before the next frame
- [x] Add focused regression coverage for safe buffer replacement and atlas upload queue behavior.
- [ ] Reconcile the remaining local render snapshot drift in `spectre-render-basic-view`, `spectre-render-cmdline-view`, and `spectre-render-unicode-view`.

## Tests

- [x] add regression tests around safe failure returns
- [x] `spectre-app-smoke`
- [x] `spectre-tests` after building `spectre-rpc-fake`
- [ ] `ctest --test-dir build --build-config Release --output-on-failure` still reports render snapshot drift in the three `spectre-render-*` scenarios

## Suggested Slice Order

1. return-value and replacement-safety audit
2. rebuild-flow cleanup
3. atlas upload scheduling improvement

## Sub-Agent Split

- best handled by one renderer-focused agent to avoid overlapping Vulkan churn

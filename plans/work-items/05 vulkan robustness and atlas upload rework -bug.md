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

1. Audit recreation/error paths.
   - check allocation return values consistently
   - avoid destroying the old resource before the replacement is known-good where possible
2. Simplify swapchain/resource rebuild flow.
   - reduce duplicated rebuild sequencing
   - keep failure cleanup explicit
3. Rework atlas upload scheduling.
   - move away from fully synchronous per-upload waits if possible
   - integrate uploads more cleanly into the frame path

## Tests

- add failure-injection or mock-path tests where feasible
- at minimum add regression tests around safe failure returns
- keep render snapshots and smoke green after the changes

## Suggested Slice Order

1. return-value and replacement-safety audit
2. rebuild-flow cleanup
3. atlas upload scheduling improvement

## Sub-Agent Split

- best handled by one renderer-focused agent to avoid overlapping Vulkan churn

# 04 Grid Wide-Cell Repair And Redraw Robustness

## Why This Exists

The redraw path is much better than it used to be, but the remaining risk is now in edge semantics, especially around wide cells and hostile redraw input.

Current likely or reviewed issues:

- overwriting a continuation half may not fully repair the leader/continuation state
- redraw robustness coverage is still mostly happy-path fixture based
- malformed or partial event handling can be pushed further

## Goal

Make grid mutation rules and redraw parsing more hostile-input tolerant, especially around wide-cell edges.

## Implementation Plan

1. Verify and fix the wide-cell overwrite path.
   - specifically test overwriting the continuation cell first
   - repair both sides of the old wide-cell state
2. Expand redraw robustness fixtures.
   - malformed arrays
   - truncated `grid_line`
   - odd repeat/empty combinations
3. Keep fixes local.
   - prefer `Grid` or `UiEventHandler` fixes over broader pipeline churn

## Tests

- direct `Grid` tests for continuation overwrite and scroll boundaries
- redraw replay cases for malformed/truncated line batches
- keep render snapshots green to catch visible fallout

## Suggested Slice Order

1. reproduce/lock down the wide-cell bug
2. patch `Grid`
3. add malformed redraw fixtures

## Sub-Agent Split

- one agent on `Grid`
- one agent on redraw fixture expansion

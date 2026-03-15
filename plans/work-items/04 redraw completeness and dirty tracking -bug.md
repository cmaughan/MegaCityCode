# 04 Redraw Completeness And Dirty Tracking -bug

## Problem

The combined reviews point at two related risks:
- redraw coverage may still be incomplete in places such as `grid_scroll`
- some dirty-tracking work may still be coarser than necessary

## Goal

Confirm redraw correctness first, then tighten dirty propagation where measurement shows it matters.

## Main Files

- `libs/spectre-nvim/src/ui_events.cpp`
- `libs/spectre-grid/include/spectre/grid.h`
- `libs/spectre-grid/src/grid.cpp`
- `libs/spectre-renderer/src/renderer_state.cpp`

## Implementation Plan

1. Verify current `grid_scroll` handling against Neovim event semantics and captured traces.
2. Fix any horizontal or partial-region gaps before changing performance behavior.
3. Measure current dirty tracking to determine whether full-grid scanning is a real bottleneck.
4. If warranted, move to more local dirty-range bookkeeping with regression tests.

## Suggested Split

- Agent 1: redraw/event conformance
- Agent 2: dirty-tracking measurement and optimization
- Agent 3: replay fixtures and regression tests

## Exit Criteria

- redraw semantics are verified, not inferred
- dirty-tracking changes are measurement-driven
- targeted tests cover partial scroll and overwrite edge cases

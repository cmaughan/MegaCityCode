# 06 Highlight Fidelity -feature

## Problem

The code carries more highlight/style information than the renderer visibly honors today. Underline, undercurl, and related fidelity gaps are now one of the clearest feature deficits.

## Goal

Render the highlight information already present in the pipeline with behavior that matches user expectations from Neovim.

## Main Files

- `libs/spectre-types/include/spectre/highlight.h`
- `libs/spectre-renderer/src/*`
- shader sources
- `app/grid_rendering_pipeline.cpp`

## Implementation Plan

1. Confirm the exact style information already available end to end.
2. Add underline and undercurl rendering first, then any special-color handling needed for diagnostics.
3. Avoid sentinel/value collisions in highlight resolution where future alpha support would matter.
4. Add visual and seam-level tests for reverse/highlight combinations.

## Suggested Split

- Agent 1: highlight/state plumbing audit
- Agent 2: renderer and shader implementation
- Agent 3: style fidelity tests

## Exit Criteria

- underline and undercurl are visibly rendered
- reverse/special-color behavior is tested

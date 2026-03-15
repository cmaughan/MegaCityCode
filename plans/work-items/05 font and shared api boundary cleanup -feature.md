# 05 Font And Shared API Boundary Cleanup -feature

## Problem

The public font surface still leaks engine details, and `spectre-types` is drifting beyond plain shared types.

## Goal

Narrow the shared dependency surface so module boundaries stay clean as the project grows.

## Main Files

- `libs/spectre-font/include/spectre/font.h`
- `libs/spectre-font/src/*`
- `libs/spectre-types/include/spectre/*`

## Implementation Plan

1. Hide raw `FT_Face`, HarfBuzz handles, and other engine detail behind internal headers or narrower facades.
2. Re-evaluate what belongs in `spectre-types` versus a more specific module.
3. Replace public raw-handle exposure with higher-level data types where ownership matters.
4. Keep downstream changes incremental so tests and renderer code can migrate without a flag day.

## Suggested Split

- Agent 1: public API audit and target design
- Agent 2: font API narrowing
- Agent 3: `spectre-types` scope cleanup

## Exit Criteria

- public headers expose less engine detail
- ownership/lifetime is clearer at the type level
- shared modules stop accumulating unrelated policy by default

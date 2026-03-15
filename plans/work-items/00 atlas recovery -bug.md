# 00 Atlas Recovery -bug

## Problem

The current glyph atlas has no safe recovery path when it fills. The agreed failure mode is too severe: new glyphs can disappear instead of failing explicitly or recovering gracefully.

## Goal

Make atlas pressure survivable in long sessions without silently dropping text.

## Main Files

- `libs/spectre-font/src/glyph_cache.cpp`
- `libs/spectre-font/include/spectre/font.h`
- `libs/spectre-font/src/text_service.cpp`
- `app/grid_rendering_pipeline.cpp`
- renderer atlas upload paths

## Implementation Plan

1. Make atlas-full state explicit instead of returning an indistinguishable empty region.
2. Choose one recovery strategy and implement it end to end:
   - full atlas rebuild
   - atlas paging
   - eviction with regeneration
3. Ensure the pipeline can trigger a full atlas re-upload cleanly after recovery.
4. Add logging and a visible diagnostic path for repeated atlas pressure.

## Suggested Split

- Agent 1: glyph-cache design and recovery implementation
- Agent 2: renderer/pipeline integration and full-upload path
- Agent 3: tests and pressure fixtures

## Exit Criteria

- no silent invisible glyphs on atlas-full
- recovery path is deterministic
- regression test covers exhaustion and post-recovery rendering

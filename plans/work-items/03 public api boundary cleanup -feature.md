
# 03 Public API Boundary Cleanup

## Why This Exists

The repo-level module split is good, but several public surfaces still leak implementation detail or collapse unrelated concerns into one header.

Current verified issues:

- `IWindow` exposes `SDL_Window*`
- `nvim.h` still bundles process, RPC, value model, redraw, and input concerns
- `font.h` is just a passthrough
- `GLYPH_ATLAS_SIZE` still lives in shared `spectre-types`

## Goal

Reduce rebuild/merge blast radius and make module ownership clearer to humans and agents.

## Implementation Plan

1. Clean the window boundary.
   - replace raw `SDL_Window*` exposure with a narrower native-handle or backend-specific extension path
   - only expose what the renderer factory actually needs
2. Split `nvim.h`.
   - separate process/RPC/value-model types from redraw/input helpers
   - keep public includes smaller and more intention-revealing
3. Remove misleading passthroughs.
   - either remove `font.h` or turn it into a real curated public header
4. Move implementation constants down.
   - atlas sizing/config should live in font/renderer-owned headers or a deliberate config surface

## Tests

- build-level validation: no private include regressions
- keep existing seam tests green after header moves

## Suggested Slice Order

1. window boundary cleanup
2. font/shared-constant cleanup
3. `nvim` header split

## Sub-Agent Split

- one agent on `spectre-window`
- one agent on font/shared-types cleanup
- one agent on `spectre-nvim` header partitioning

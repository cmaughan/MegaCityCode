# 03 UI Path RPC Decoupling -bug

## Problem

Blocking RPC round-trips on resize and font-change paths can turn Neovim responsiveness into GUI latency.

## Goal

Keep the UI responsive even when Neovim is slow, stalled, or temporarily busy.

## Main Files

- `app/app.cpp`
- `libs/spectre-nvim/src/rpc.cpp`
- `libs/spectre-nvim/include/spectre/nvim.h`

## Implementation Plan

1. Identify all UI-thread request paths that still wait on synchronous RPC results.
2. Convert resize and font-change negotiation to non-blocking or deferred request flows where practical.
3. Add clear fallback behavior when Neovim does not respond quickly enough.
4. Make latency visible in logs/tests rather than only in user experience.

## Suggested Split

- Agent 1: request-path audit and contract changes
- Agent 2: app-side deferred resize/font workflow
- Agent 3: timeout and stall tests

## Exit Criteria

- resize and font zoom no longer block the UI thread on long RPC waits
- failure behavior is explicit and logged

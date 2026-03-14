# Review Consensus Execution

## Status

The main consensus refactor plan is complete. The architecture, seam extraction, RPC safety work, shared renderer state, text service extraction, UI options wiring, native tests, smoke coverage, and shared logging layer are all landed and validated.

This file now tracks only the remaining optional follow-up work.

## Remaining Work

Optional follow-up:

- Idle scheduler validation and diagnostics
  Check CPU/GPU idle behavior properly after the event-driven main-loop change. If we need better visibility, add a lightweight diagnostic such as a brief flash/overlay/log pulse on frame submission so unnecessary redraws are obvious during manual testing.

The original consensus work is complete. Any further work from here is new optimization or product-direction work, not unfinished consensus cleanup.

## Guardrails

- Keep `t.bat` green after each slice.
- Renderer- or startup-adjacent changes should still get a short `r.bat --console --smoke-test` check.
- Prefer narrow, test-backed slices over broad cross-module rewrites.

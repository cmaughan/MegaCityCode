# Review Consensus Execution

## Status

The main consensus refactor plan is complete. The architecture, seam extraction, RPC safety work, shared renderer state, text service extraction, UI options wiring, native tests, smoke coverage, and shared logging layer are all landed and validated.

This file now tracks only the remaining optional follow-up work.

## Remaining Work

1. Unicode conformance corpus

- Compare Spectre width decisions against headless Neovim `strdisplaywidth()` across a broader fixture set.
- Focus on emoji sequences, ambiguous-width characters, combining-heavy scripts, and plugin UI glyphs.

2. Cell-buffer upload performance

- Atlas uploads are incremental, but cell-buffer uploads are still coarse.
- Add dirty-range tracking for renderer cell buffers if profiling shows it matters.

3. Notification queue hardening

- The notification path is correct but still simple.
- Add queue bounds or backpressure behavior if long-running sessions show unbounded growth risk.

4. Logging documentation and polish

- Keep README logging docs aligned with the shared logger behavior.
- Add more targeted failure-path assertions if startup/RPC logging needs stronger regression coverage.

5. CI/runtime observation

- Keep an eye on hosted-runner smoke stability over time.
- If GitHub-hosted runners become flaky for windowed GPU startup, split headless logic from full renderer smoke.

## Guardrails

- Keep `t.bat` green after each slice.
- Renderer- or startup-adjacent changes should still get a short `r.bat --console --smoke-test` check.
- Prefer narrow, test-backed slices over broad cross-module rewrites.

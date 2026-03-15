# 01 App Loop And UI Request Decomposition

## Why This Exists

`app/app.cpp` is still the main merge hotspot. The module split is good, but too much policy still terminates in one orchestration class.

Current hot areas:

- cursor blink state machine
- event-driven run loop policy
- startup resize deferral
- clipboard action routing
- render/smoke mode execution
- config-save hooks

## Goal

Keep `App` as the top-level coordinator, not the home for every policy decision.

## Implementation Plan

1. Extract loop/cursor timing policy.
   - move blink timing and render-wakeup decisions into a small scheduler/helper
2. Separate UI request execution from app orchestration.
   - let `UiRequestWorker` or a successor own more of the “request and retry/debounce” policy
3. Isolate startup/shutdown staging.
   - keep initialize/rollback steps explicit and local
   - push side concerns like config-save triggers and resize bookkeeping downward
4. Keep `App` owning composition, not details.
   - window + renderer + text + grid + rpc wiring stays in `App`
   - timing/state machine helpers move out

## Tests

- add focused tests around the extracted scheduler or loop-policy helper
- add startup resize deferral coverage at the helper level
- keep app smoke and render snapshot scenarios green

## Suggested Slice Order

1. extract cursor blink/timing helper
2. extract request coordination helper
3. trim `App` callsites
4. add seam tests

## Sub-Agent Split

- one agent on loop/timing extraction
- one agent on request worker decomposition
- avoid parallel edits in `app/app.cpp` until helper interfaces are agreed

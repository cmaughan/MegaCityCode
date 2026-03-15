# 06 Test Seam Cleanup And Lifecycle Coverage

## Why This Exists

The test suite is already strong for a GUI app, but the next step is better ownership and more failure/lifecycle coverage.

Current verified issues:

- tests still pull in private `src` directories
- tests compile `app/render_test.cpp` directly
- missing lifecycle/failure cases include shutdown latency, font resize chains, startup resize deferral, and config/parser round trips

## Goal

Move tests closer to supported seams and fill the remaining lifecycle/failure holes.

## Implementation Plan

1. Reduce private-source reach-through.
   - expose narrow testable seams or internal test libs where justified
   - stop compiling ad hoc production `.cpp` files into tests when avoidable
2. Add lifecycle-focused coverage.
   - shutdown while worker/request is active
   - startup resize deferral
   - font-size change -> relayout -> resize request chain
3. Add parser/config round-trip coverage.
   - config save/load
   - scenario parsing edge cases

## Tests

- this item is itself test work
- acceptance is the new tests existing and providing useful failure localization

## Suggested Slice Order

1. parser/config round-trip tests
2. lifecycle tests
3. seam cleanup around render-test and private-source reach-through

## Sub-Agent Split

- one agent can add tests while another extracts seams, but only after the seam direction is agreed

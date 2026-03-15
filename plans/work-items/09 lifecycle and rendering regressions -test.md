# 09 Lifecycle And Rendering Regressions -test

## Problem

The current test suite is good, but the remaining gaps are concentrated around failure paths and rendering/lifecycle regressions that are easy to reintroduce.

## Goal

Add focused regression tests that protect the highest-risk behavior called out in the reviews.

## Test Targets

- init rollback after a late failure
- atlas-full and recovery behavior
- font fallback selection
- reverse/highlight fidelity
- resize/font-change latency behavior
- cursor blink/controller state edges
- partial scroll and double-width overwrite edges

## Implementation Plan

1. Add tests only after each corresponding behavior has a stable seam.
2. Prefer replay fixtures, fake servers, and seam tests over fragile GUI automation.
3. Keep app-smoke coverage for full-stack startup/shutdown, but use narrower tests for most correctness checks.

## Suggested Split

- Agent 1: lifecycle and RPC tests
- Agent 2: font/atlas/highlight tests
- Agent 3: redraw edge-case tests

## Exit Criteria

- each high-risk change area has at least one narrow regression test
- the suite can catch future rollback, atlas, and highlight regressions without relying on manual smoke checks

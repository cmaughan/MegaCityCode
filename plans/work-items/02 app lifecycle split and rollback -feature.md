# 02 App Lifecycle Split And Rollback -feature

## Problem

`App` remains the highest-conflict file in the repo. It still owns too much initialization, lifecycle policy, and runtime orchestration.

## Goal

Keep `App` as an orchestrator, not a policy bucket, and make partial-init rollback explicit and safe.

## Main Files

- `app/app.cpp`
- `app/app.h`
- `app/grid_rendering_pipeline.cpp`
- `libs/spectre-nvim/*`
- `libs/spectre-window/*`

## Implementation Plan

1. Split initialization into explicit stages with scoped rollback or RAII-owned subcomponents.
2. Extract lifecycle-sensitive concerns into focused helpers:
   - startup/attach sequence
   - resize policy
   - cursor controller if still mixed into `App`
3. Remove separate `initialize()` plus raw-pointer setup patterns where ownership can be made explicit.
4. Keep the public app surface small while moving policy behind smaller units.

## Suggested Split

- Agent 1: staged init/rollback design
- Agent 2: `App` extraction work
- Agent 3: lifecycle tests

## Exit Criteria

- late init failure does not leak process, renderer, or window state
- `App` sheds meaningful policy weight
- startup/shutdown paths become easier to unit-test

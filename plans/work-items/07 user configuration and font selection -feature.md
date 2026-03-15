# 07 User Configuration And Font Selection -feature

## Problem

Important user-facing settings are still effectively hardcoded, especially around fonts and startup defaults.

## Goal

Give users a stable configuration path for the settings they should not need to recompile to change.

## Main Files

- `app/*`
- `libs/spectre-font/src/text_service.cpp`
- `README.md`

## Implementation Plan

1. Introduce a small config format for core GUI settings:
   - font path or family
   - fallback list
   - font size
   - optional window defaults
2. Make bundled/system font fallback behavior explicit rather than silent.
3. Persist the settings most users adjust interactively, starting with font size if appropriate.
4. Document the config and fallback rules clearly.

## Suggested Split

- Agent 1: config format and load path
- Agent 2: font resolution integration
- Agent 3: docs and migration behavior

## Exit Criteria

- users can choose fonts without recompiling
- startup behavior is deterministic and documented

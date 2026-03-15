# 02 Render Test Extraction And Parser Unification

## Why This Exists

The current render-test path works, but the ownership is wrong.

Current verified issues:

- `app/render_test.cpp` is compiled into `spectre.exe`
- the tests compile that same file directly
- `app_config.cpp` and `render_test.cpp` each carry their own partial TOML-like parser helpers
- render-test reporting still uses hand-built JSON text

## Goal

Keep render-test tooling available without making it a production-orchestration concern, and remove duplicated parsing/reporting logic.

## Implementation Plan

1. Extract shared parsing helpers.
   - create a small parser utility for the simple config/scenario format, or adopt one lightweight library if justified
   - remove duplicated `trim`, `unquote`, and string-array parsing code
2. Move render-test logic behind a narrower library seam.
   - scenario load
   - frame diff/compare
   - report generation
   - image export helpers
3. Keep the CLI mode thin.
   - `main.cpp` should only dispatch to the render-test service
   - `App` should not own report-writing policy
4. Make report output structurally safe.
   - stop concatenating JSON strings manually

## Tests

- parser round-trip/edge-case tests
- report escaping tests
- keep current render snapshot scenarios green

## Suggested Slice Order

1. parser utility extraction
2. report/output cleanup
3. render-test service extraction
4. CLI/app wiring cleanup

## Sub-Agent Split

- one agent on parser/report utility
- one agent on render-test service extraction
- merge after agreeing on the new shared utility API

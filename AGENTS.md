# MegaCityCode Agent Guide

## Scope

This repository is a cross-platform native 3D viewer with:

- Vulkan rendering on Windows
- Metal rendering on macOS
- SDL3 windowing/input
- a fixed-camera visualization scene today, with font/grid foundations retained for later use

The codebase is intentionally split into small libraries. Keep app code thin and push platform or subsystem logic downward into `libs/`.

## Build And Test

### Windows

Requirements:

- CMake 3.25+
- Visual Studio 2022
- Vulkan SDK with `glslc`
Commands:

```powershell
cmake --preset default
cmake --build build --config Release --parallel
ctest --test-dir build --build-config Release --output-on-failure
```

Run:

```powershell
.\build\Release\megacitycode.exe
.\build\Release\megacitycode.exe --console
```

### macOS

Requirements:

- CMake 3.25+
- Xcode Command Line Tools
Commands:

```bash
cmake --preset mac-debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Run:

```bash
./build/megacitycode
```

## Architecture

- `libs/megacitycode-types`: shared POD types and event structs
- `libs/megacitycode-window`: `IWindow` abstraction and SDL implementation
- `libs/megacitycode-renderer`: `IRenderer` abstraction, backend factory, Vulkan and Metal backends
- `libs/megacitycode-font`: FreeType/HarfBuzz font loading, shaping, glyph atlas management
- `libs/megacitycode-grid`: thin array-backed cell storage reserved for future text/layout work
- `app/`: orchestration only

Preferred dependency direction:

- `app` depends on public headers only
- renderer backends stay private to `megacitycode-renderer`
- pure logic stays testable without opening a window

## Validation Expectations

- If you touch renderer code, build the platform-specific app target and verify startup at least once.
- After implementing a user-facing feature or rendering-affecting change, run the render smoke/snapshot suite with `t.bat` or `ctest` and confirm the relevant `megacitycode-render-*` scenario still passes.
- When blessing render references, use `py do.py blessplane` or `py do.py blessall` from the repo root instead of calling `megacitycode.exe --render-test` manually. The helper passes repo-rooted scenario paths and avoids working-directory mistakes.
- If you change build wiring, keep both Windows and macOS paths valid in CI.
- After every completed work item, run one final `clang-format` pass across all touched source files in a single shot instead of formatting piecemeal during the work.
- When you complete a work item or a concrete subtask from `plans/work-items/*.md`, update that markdown file in the same turn and mark the completed entries with Markdown task ticks (`- [x]`). Leave incomplete follow-ups as unchecked items so progress stays visible in the file itself.

## Review Consensus

When the user asks to "come to consensus" on reviews, do not just concatenate or summarize review files.

Treat it as a synthesis task:

- read the review notes from the relevant agent folders under `plans/reviews/`
- identify where the agents agree, where one review adds useful detail, and where there is real disagreement or just a sequencing difference
- reconcile the review notes against the current tree so already-fixed or stale issues are called out instead of repeated blindly
- produce a planning-oriented consensus note with suggested fix order, not just a findings list
- where helpful, explicitly attribute points to the agent models that raised them

The result should read like a conversation and planning review for fixes, with a current recommended path forward.

## Known Pitfalls

- Do not include backend-private renderer headers from `app/`.
- The grid is intentionally storage-only right now; changing it must not affect rendering until the text layer is reintroduced deliberately.
- Font-size changes still matter for the retained font/grid foundation even though the current plane scene does not consume that data visually.
- Do not include backend-private renderer headers from `app/`.
## Consensus Shortcut

When the user says `come to consensus`, treat that as a direct instruction to execute the saved consensus prompt in `plans/prompts/consensus_review.md`.

## Prompt History

When the user asks to store prompts from the current thread, write them to a dated markdown file under `plans/prompts/history/` in chronological order and mark interrupted or partial prompts inline.

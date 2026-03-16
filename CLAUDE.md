# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### Windows
Requires CMake 3.25+, Visual Studio 2022, and Vulkan SDK (with glslc).

```bash
cmake --preset default                               # Configure (Debug, VS 2022 x64)
cmake --preset release                               # Configure (Release)
cmake --build build --config Release --target megacitycode # Build
```

Run: `.\build\Release\megacitycode.exe`. Pass `--console` to allocate a debug console window.

### macOS
Requires CMake 3.25+, Xcode Command Line Tools (for Metal compiler).

```bash
cmake --preset mac-debug                             # Configure (Debug)
cmake --preset mac-release                           # Configure (Release)
cmake --build build --target megacitycode           # Build
```

Run: `./build/megacitycode`.

## Project Structure

```text
megacitycode/
├── CMakeLists.txt                  # Top-level: wires libraries + app
├── CLAUDE.md
├── libs/
│   ├── megacitycode-types/         # Shared POD types, events, logging
│   ├── megacitycode-window/        # Window abstraction + SDL implementation
│   ├── megacitycode-renderer/      # Renderer interface + Vulkan/Metal backends
│   ├── megacitycode-font/          # Font loading, shaping, glyph cache
│   └── megacitycode-grid/          # Thin retained cell storage for future text work
├── app/                            # Startup, scene loop, test entry points
├── shaders/                        # Vulkan and Metal shader assets
└── fonts/
```

## Architecture

MegaCityCode is a cross-platform native viewer. The current runtime centers on a fixed-camera 3D plane scene in Vulkan, while the font stack and a simple retained grid remain in place for later text-driven work.

### Dependency Graph

```text
                 megacitycode-types
              /      |       |      \
        window   renderer   font    grid
              \      |       |      /
                     app (executable)
```

### Data Flow

```text
SDL events
  -> App::run() / App::pump_once()
  -> window resize or font-size changes update retained grid dimensions
  -> renderer scene draw
  -> optional frame capture for smoke/snapshot testing
```

### Key Abstractions

- `IRenderer` (`libs/megacitycode-renderer/include/megacitycode/renderer.h`) and `IWindow` (`libs/megacitycode-window/include/megacitycode/window.h`) are the main platform seams.
- `App` (`app/app.h/cpp`) owns startup, the main loop, retained state sizing, and render/smoke test paths.
- `Grid` (`libs/megacitycode-grid/include/megacitycode/grid.h`) is now a simple array-backed cell store. It does not currently affect rendering.

### Rendering

- Vulkan currently renders a single plane with shader-generated geometry and a simple material.
- The default camera looks toward the origin from an elevated 45-degree view.
- Render snapshots capture pixels directly from the renderer output.
- The text stack remains initialized so font metrics and atlas code stay available for later scene/text integration.

#### Vulkan-specific (Windows)
- Descriptor sets, render passes, and swapchain management via vk-bootstrap
- Plane shaders compiled from GLSL to SPIR-V via glslc

#### Metal-specific (macOS)
- Metal backend remains behind the same renderer interface
- Shader assets compile with `xcrun metal` and `xcrun metallib`

### Threading

- The current viewer runs its window events, retained state updates, and GPU submission on the main thread.

### Font Pipeline

- FreeType loads faces
- HarfBuzz shapes codepoints to glyph IDs
- `GlyphCache` rasterizes on demand and maintains the atlas

## Dependencies

All fetched automatically via CMake FetchContent (in `cmake/FetchDependencies.cmake`): SDL3, FreeType, HarfBuzz. On Windows: vk-bootstrap, VMA. Shaders compile from GLSL to SPIR-V via glslc (Windows, `cmake/CompileShaders.cmake`) or from Metal to metallib via xcrun (macOS, `cmake/CompileShaders_Metal.cmake`).

## Validation Expectations

- If you touch renderer code, build the platform-specific app target and verify startup at least once.
- After implementing a rendering-affecting change, run the render smoke/snapshot suite with `t.bat` or `ctest` and confirm the relevant `megacitycode-render-*` scenario still passes.
- When blessing render references, use `py do.py blessplane` or `py do.py blessall` from the repo root instead of calling `megacitycode.exe --render-test` manually.
- If you change build wiring, keep both Windows and macOS paths valid in CI.
- After every completed work item, run one final `clang-format` pass across all touched source files in a single shot instead of formatting piecemeal during the work.
- When you complete a work item or a concrete subtask from `plans/work-items/*.md`, update that markdown file in the same turn and mark completed entries with Markdown task ticks (`- [x]`).

## Platform

- Windows: MSVC/Visual Studio 2022, built as a Windows GUI app (`WIN32_EXECUTABLE`) with the active Vulkan scene path
- macOS: Clang/Xcode, rendering via Metal

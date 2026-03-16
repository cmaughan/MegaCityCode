# GEMINI.md - MegaCityCode Project Context

## Project Overview
MegaCityCode is a cross-platform native viewer built with C++20. It uses native GPU rendering (Vulkan on Windows, Metal on macOS) and currently centers on a fixed-camera visualization scene, while retaining the font and grid subsystems for later text-driven work.

- **Architecture:** Highly modular, organized into internal libraries (`libs/`) covering types, windowing (SDL3), rendering (Vulkan/Metal), font management (FreeType/HarfBuzz), and retained grid state.
- **Key Technologies:**
  - **Windowing:** SDL3
  - **GPU APIs:** Vulkan (Windows), Metal (macOS)
  - **Text Stack:** FreeType, HarfBuzz, dynamic glyph atlas
- **Main Goal:** Build out a polished native visualization path without throwing away the existing font and retained-cell groundwork.

## Building and Running

### Prerequisites
- **Windows:** CMake 3.25+, Visual Studio 2022, Vulkan SDK (with `glslc`)
- **macOS:** CMake 3.25+, Xcode CLT

### Build Commands
The project uses CMake Presets for configuration.

- **Windows (Debug):**
  ```powershell
  cmake --preset default
  cmake --build build --config Debug --parallel
  ```
- **macOS (Debug):**
  ```bash
  cmake --preset mac-debug
  cmake --build build --parallel
  ```

### Running the App
- **Windows:** `.\build\Debug\megacitycode.exe` (or `.\r.bat` for a quick build and run)
- **macOS:** `./build/megacitycode` (or `./r.sh`)
- **Flags:** `--console` (Windows only, opens log console), `--smoke-test` (runs a brief initialization check)

### Convenience Scripts
- `r.bat` / `r.sh`: Build and run the application
- `t.bat` / `t.sh`: Build and run the test suite

## Testing
MegaCityCode includes a suite of native tests covering retained grid logic, app config/render-test parsing, renderer helpers, and scene snapshot behavior.

- **Run Tests:** Use `t.bat` (Windows) or `./t.sh` (macOS)
- **Underlying Tool:** CTest
- **Integration Test:** `megacitycode-app-smoke` verifies the app reaches a first rendered frame within a timeout

## Development Conventions

### Architecture and Modularity
- **Encapsulation:** The renderer backend (Vulkan/Metal) is hidden behind the `IRenderer` interface. App-level code should only interact with `IRenderer.h`.
- **Libraries:**
  - `megacitycode-types`: Shared PODs, events, and logging
  - `megacitycode-window`: SDL3 window abstraction
  - `megacitycode-renderer`: Public API and platform-specific backends
  - `megacitycode-font`: Font loading, shaping, and glyph caching
  - `megacitycode-grid`: Thin array-backed retained cell storage reserved for later use

### Coding Style
- **Standard:** C++20
- **Naming:** `snake_case` for variables and functions, `PascalCase` for classes and enums
- **Logging:** Use the `MEGACITYCODE_LOG_<LEVEL>(category, ...)` macros
  - **Categories:** `App`, `Rpc`, `Window`, `Font`, `Renderer`, `Input`, `Test`
  - **Levels:** `Error`, `Warn`, `Info`, `Debug`, `Trace`
- **Assets:** Shaders and fonts are copied to the build output directory during the post-build step

### Git Workflow
- Always verify changes against the app smoke test and any relevant render snapshot after renderer or scene changes
- Do not commit generated build artifacts or large log files
- Adhere to the `.clang-format` configuration for code formatting

## Key Files
- `app/main.cpp`: Entry point, handles platform-specific initialization
- `app/app.cpp`: Main application orchestrator
- `libs/megacitycode-renderer/include/megacitycode/renderer.h`: Public rendering interface
- `libs/megacitycode-renderer/src/vulkan/vk_renderer.cpp`: Vulkan scene camera/projection setup
- `libs/megacitycode-grid/include/megacitycode/grid.h`: Retained cell container reserved for future scene/text work
- `CMakeLists.txt`: Root build configuration
- `cmake/FetchDependencies.cmake`: External library management

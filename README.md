# Spectre

A cross-platform Neovim GUI frontend with Vulkan (Windows) and Metal (macOS) rendering.

![Spectre on Windows](screenshots/spectre.png)
![Spectre on macOS](screenshots/spectre-mac.png)

## Features

- Vulkan renderer (Windows) and Metal renderer (macOS) with instanced drawing
- FreeType + HarfBuzz font rendering with dynamic glyph atlas
- Msgpack-RPC communication with embedded neovim (`nvim --embed`)
- Full ext_linegrid protocol support
- Runtime font size control (Ctrl+=/-, Ctrl+0 to reset)
- Mouse support (click, drag, scroll)
- Window resize with automatic grid recalculation
- Retina/HiDPI display support on macOS

## Building

### Windows

**Requirements:** CMake 3.25+, Vulkan SDK, Visual Studio 2022

```
cmake --preset default
cmake --build build --config Release --target spectre
```

### macOS

**Requirements:** CMake 3.25+, Xcode Command Line Tools

```
cmake --preset mac-debug
cmake --build build --target spectre
```

All other dependencies (SDL3, FreeType, HarfBuzz, MPack, and vk-bootstrap/VMA on Windows) are fetched automatically via CMake FetchContent.

## Running

**Windows:**
```
build\Release\spectre.exe
```

**macOS:**
```
./build/spectre
```

Requires `nvim` on PATH.

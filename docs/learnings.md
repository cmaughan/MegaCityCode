# Learnings

Things discovered while working on this project that are worth remembering for future sessions or other projects.

---

## Tooling

### Claude can generate and manipulate code dependency graphs

Claude successfully:
- Generated a `scripts/gen_deps.py` script that calls `cmake --graphviz` to produce a dot file of CMake target dependencies
- Wrote Python to post-process the dot file: strip the CMake legend subgraph (brace-depth tracking), filter noise targets (ZERO_CHECK, ALL_BUILD, etc.), and render to SVG via `dot`
- Extended it with `--exclude` (remove a node and all its edges entirely) and `--prune` (keep a named node as a leaf but remove everything it depends on — useful for collapsing third-party library subtrees like SDL3-static or freetype without losing the node itself)

The prune logic works by parsing the dot node/edge lines, building an adjacency list, BFS-ing from the prune root to find all descendants, then dropping those nodes and any edges that reference them.

**Useful for:** visualising module boundaries, spotting unexpected dependency edges, onboarding new contributors.

```sh
python scripts/gen_deps.py --prune SDL3-static --prune freetype --prune harfbuzz
```

---

### clang-uml for class-level dependency diagrams

For deeper than CMake target graphs, `clang-uml` generates UML class diagrams directly from C++ source using `compile_commands.json` (already emitted by the CMake config via `CMAKE_EXPORT_COMPILE_COMMANDS ON`).

Setup:
- `.clang-uml` YAML config in the repo root defines the `spectre_classes` diagram
- `scripts/gen_uml.py` runs `clang-uml` then `plantuml` to render to SVG
- Install: `brew install clang-uml plantuml` (Mac) / `winget install bkryza.clang-uml` + `choco install plantuml` (Windows)
- `compile_commands.json` requires a **Ninja** build (`cmake --preset clang-tools` → `build-tools/`); the default VS generator does not emit it

```sh
python scripts/gen_uml.py                        # all diagrams → docs/uml/
python scripts/gen_uml.py --diagram spectre_classes
python scripts/gen_uml.py --puml-only            # emit .puml without rendering
```

Key config fields in `.clang-uml`:
- `compilation_database_dir`: points at `./build-tools` (Ninja build, has `compile_commands.json`)
- `glob`: must match **`.cpp` translation units**, not headers — clang-uml follows includes from there
- `using_namespace`: strips `spectre::` prefix from all type names in the diagram
- `generate_method_arguments: none`: keeps the diagram readable (no parameter lists)
- `exclude: namespaces: [std, mpack, ...]`: essential — without this the diagram is swamped with STL and third-party types
- **SVG not PNG**: PlantUML's PNG output is pixel-limited and clips large diagrams; always use `-tsvg`

#### clang-uml 0.6.2 known crashes (Windows)
Two source files trigger `STATUS_ILLEGAL_INSTRUCTION` (exit 3221225725) in clang-uml 0.6.2 on Windows:
- `libs/spectre-grid/src/grid.cpp`
- `libs/spectre-nvim/src/ui_events.cpp`

Workaround: exclude them from `glob`. Their classes still appear in the diagram via headers included by other translation units. Running diagrams in parallel (default) also triggers a crash — run each with `-n <name>` sequentially.

#### Package diagrams don't work for single-namespace codebases
`type: package` requires sub-namespaces to group. Since all spectre code is in one `spectre` namespace, the package diagram is always empty. Use the CMake graphviz graph (`docs/deps/deps.svg`) for library-level dependency visualisation instead.

---

### Visual regression tests should capture renderer output, not the desktop

For deterministic screenshot-style testing in Spectre, capture pixels directly from the renderer output instead of taking a desktop screenshot.

What worked well:
- Read back the actual presented frame from the swapchain/drawable
- Keep scenarios deterministic: fixed window size, fixed bundled font, clean Neovim startup, blink disabled, scripted startup commands
- Store platform-specific references because Vulkan and Metal can differ slightly in raster output

Why this matters:
- Desktop screenshots pick up compositor noise, z-order/focus issues, DPI scaling, and other window-manager artifacts
- Swapchain/drawable readback gives a stable pixel buffer that can be diffed mechanically and blessed when expected changes occur

Current repo pattern:
- scenario files under `tests/render/`
- references under `tests/render/reference/`
- actual/diff/report artifacts under `tests/render/out/`

---

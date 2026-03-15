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

For deeper than CMake target graphs, `clang-uml` generates UML class and package diagrams directly from C++ source using `compile_commands.json` (already emitted by the CMake config via `CMAKE_EXPORT_COMPILE_COMMANDS ON`).

Setup:
- `.clang-uml` YAML config in the repo root defines diagrams (`spectre_classes`, `spectre_packages`)
- `scripts/gen_uml.py` runs `clang-uml` then `plantuml` to render to PNG/SVG
- Install: `brew install clang-uml plantuml` (Mac) / `winget install bkryza.clang-uml` + `choco install plantuml` (Windows)

```sh
python scripts/gen_uml.py                        # all diagrams → docs/uml/
python scripts/gen_uml.py --diagram spectre_classes --format svg
python scripts/gen_uml.py --puml-only            # emit .puml without rendering
```

Key config fields in `.clang-uml`:
- `compilation_database_dir`: points at `./build` (where `compile_commands.json` lives)
- `glob`: header files to analyse (public API headers + selected internal ones)
- `using_namespace`: strips `spectre::` prefix from all type names in the diagram
- `generate_method_arguments: none`: keeps the diagram readable (no parameter lists)
- `package_type: directory`: groups classes by filesystem directory for the package diagram

---

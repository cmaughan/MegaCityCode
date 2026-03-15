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

### Hero screenshots need a separate path from deterministic render tests

The README/marketing screenshot should not use the exact same startup model as the deterministic regression scenarios.

What we learned:
- Deterministic render tests should stay clean and fixed: bundled font, `-u NONE --noplugin`, fixed commands, blink off, stable compare/bless flow.
- The README hero screenshot looks much better when it uses the user's real Neovim config, theme, statusline, and plugin UI.
- The current split works well:
  - deterministic scenarios live under `tests/render/*.toml`
  - the hero screenshot uses `tests/render/readme-hero.toml`
  - `scripts/update_screenshot.py` defaults to the hero scenario

Important operational detail:
- `update_screenshot.py` does **not** launch a normal visible desktop window. It runs Spectre in render-test/export mode and grabs pixels from the renderer backbuffer. That is why "the app is not popping up" during screenshot generation is expected behavior, not a startup failure.

---

### Multi-line array parsing in render scenarios was a real bug

The render scenario loader originally only handled single-line arrays like:

```toml
commands = ["set number", "edit file.txt"]
```

That broke the hero screenshot scenario because `readme-hero.toml` used a multi-line `commands = [ ... ]` block. The failure mode was confusing:
- the screenshot updater appeared to run
- the output PNG did not change
- Spectre's render-test export path was actually failing before capture because it loaded zero startup commands

Fix:
- teach `load_render_test_scenario()` in `app/render_test.cpp` to accumulate multi-line array literals
- add a regression test for multi-line `commands` parsing

Lesson:
- config/test-scenario formats that look TOML-like need to support the syntax we actually write in the repo, or they become silent tooling traps

---

### Plugin-driven screenshot actions should call APIs directly, not replay mappings

Trying to open Mini Files in the hero screenshot by simulating the `-` key was unreliable.

What we found:
- `-` was a Lazy-managed normal-mode mapping with an expr callback
- feeding the key too early did nothing
- even when the mapping existed, replaying the mapped key path was less deterministic than just calling the plugin directly

What worked:
- defer the action briefly inside Neovim
- `pcall(require, 'mini.files')` to force the plugin to load
- call `MiniFiles.open(vim.api.nvim_buf_get_name(0), false)` directly

Lesson:
- for scripted screenshot/setup flows, prefer explicit plugin APIs over simulated keypresses whenever possible

---

### Python downsampling was fun, but the native high-resolution image won

We tried a supersampled hero screenshot path:
- capture at `2560x1600`
- keep the font size the same
- downsample in Python by averaging `2x2` RGBA blocks into a `1280x800` PNG

That worked technically and was straightforward to implement in `scripts/update_screenshot.py`, but the user preferred the native full-resolution result instead.

Current outcome:
- hero screenshots are captured and written directly at `2560x1600`
- no downsample step is used now

Still useful to remember:
- pure-Python RGBA downsampling is easy to add for doc assets if we ever want a smaller supersampled output path later

---

## Fonts, Emoji, and Fallbacks

### Windows color emoji needs an end-to-end color glyph path

Monochrome emoji was not just a font-selection problem. Full color on Windows required the whole pipeline to support color glyphs.

What had to be true end to end:
- FreeType color glyph loading had to preserve `BGRA` bitmap data
- the glyph cache had to keep RGBA pixels instead of collapsing everything to alpha
- atlas uploads had to support RGBA, not just monochrome glyph masks
- shaders had to treat color glyphs differently and avoid tinting them with the cell foreground color

Font selection also mattered:
- on Windows, `seguiemj.ttf` should be preferred ahead of `seguisym.ttf` for emoji-like clusters
- if the fallback picker resolves to a monochrome symbol font first, you still get black-and-white emoji even if the renderer can handle color glyphs

Lesson:
- color emoji is a pipeline feature, not a single switch

---

### CJK tofu on Windows was mostly a fallback-font coverage problem

The "Wide" line rendering as tofu was fixed by broadening the default Windows fallback list, not by changing shaping logic.

Useful fallback candidates:
- `YuGoth*`
- `meiryo`
- `msgothic`
- `msyh`
- `simsun`

Lesson:
- if wide/CJK text is tofu while Latin and emoji work, the first thing to check is fallback coverage, not HarfBuzz

---

### Bundled fonts and installed fonts can diverge in important ways

We hit this earlier with newer Nerd Font icons: the repo-bundled font lacked glyphs that were present in the installed system version.

Practical pattern that worked:
- prefer a current installed JetBrains Nerd Font if present
- keep a known-good bundled fallback in the repo
- update the bundled copy when the system-installed version proves materially better

Lesson:
- a stale bundled font can make the app look broken even when the user's machine actually has the right glyphs

---

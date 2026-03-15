# Module Map

This page is the human-friendly entry point for understanding how Spectre is put together.

Use it when you want to answer:

- where a change probably belongs
- how the main libraries relate to each other
- which generated diagrams to look at first
- which validation flows are most relevant after a change

## Start Here

If you only want the shortest path to orientation:

1. Read the top-level layout in [README.md](../README.md).
2. Look at the target dependency graph in [deps.svg](deps/deps.svg).
3. Look at the class diagram in [spectre_classes.svg](uml/spectre_classes.svg).
4. Generate local API docs with `python scripts/build_docs.py --api-only`, then open `docs/api/index.html`.
5. Check active follow-up items in [index.md](../plans/work-items/index.md).
6. Use the snapshot and smoke flows through [do.py](../do.py) when touching UI or rendering.

## Main Libraries

### `app/`

Top-level orchestration only.

Owns:
- process startup/shutdown
- wiring between window, renderer, text service, grid, and Neovim RPC
- smoke/render-test entry points

Good place for:
- app lifecycle
- top-level CLI/test harness behavior
- integration glue

Bad place for:
- backend-specific renderer logic
- low-level font logic
- grid mutation rules

### `libs/spectre-types/`

Shared low-level data types and cross-module contracts.

Owns:
- shared structs
- event types
- highlight/logging/support types

Good place for:
- narrow shared contracts
- POD-like data passed between modules

### `libs/spectre-window/`

SDL windowing and platform-facing input/display behavior.

Owns:
- window creation
- DPI/display queries
- event pump / wake behavior
- clipboard / IME / title / focus plumbing

Good place for:
- platform window behavior
- SDL event translation

### `libs/spectre-renderer/`

Public renderer API plus Vulkan/Metal backends.

Owns:
- renderer interface
- shared renderer CPU-side state
- GPU upload/submission code
- frame capture for render snapshots

Good place for:
- draw/update behavior
- backend-specific GPU work
- readback/capture paths

### `libs/spectre-font/`

Text pipeline and atlas management.

Owns:
- primary/fallback font loading
- shaping
- glyph cache / atlas population
- text service API

Good place for:
- fallback/font selection
- glyph rasterization
- color emoji support

### `libs/spectre-grid/`

The terminal cell model.

Owns:
- cell storage
- dirty tracking
- scroll/copy/clear behavior

Good place for:
- redraw correctness
- cell mutation rules

### `libs/spectre-nvim/`

Embedded Neovim process, RPC, redraw parsing, and input encoding.

Owns:
- child process lifecycle
- msgpack-RPC transport
- UI event parsing
- keyboard/mouse/text input encoding

Good place for:
- Neovim API/event handling
- transport behavior
- input fidelity

## Generated Views

### Target graph

[deps.svg](deps/deps.svg)

Use this when:
- checking module boundaries
- spotting unexpected library dependencies
- deciding where a new abstraction belongs

### Class diagram

[spectre_classes.svg](uml/spectre_classes.svg)

Use this when:
- exploring object relationships inside a subsystem
- understanding which class owns a responsibility

### Local API docs

Generated locally at `docs/api/index.html` via Doxygen.

Use this when:
- you want browsable symbol and header docs
- you want include/call/reference graphs tied to public headers
- you want a more traditional API-reference view than the diagrams provide

## Validation Map

### Fast confidence

- `python do.py smoke`
- `python do.py test`

### Deterministic UI confidence

- `python do.py basic`
- `python do.py cmdline`
- `python do.py unicode`
- `python do.py blessall`

### Documentation / hero image

- `python do.py shot`

## Planning Links

- Active work items: [plans/work-items/index.md](../plans/work-items/index.md)
- Design notes: [plans/design](../plans/design)
- Review notes: [plans/reviews](../plans/reviews)
- Learnings: [docs/learnings.md](learnings.md)

## Practical Heuristics

- If the issue is about what Neovim sent or how input is encoded, start in `spectre-nvim`.
- If the issue is about what the screen should contain, start in `spectre-grid`.
- If the issue is about how the screen is drawn, start in `spectre-renderer`.
- If the issue is about glyph choice, shaping, emoji, tofu, or atlas behavior, start in `spectre-font`.
- If the issue is about DPI, focus, clipboard, IME, or visible window behavior, start in `spectre-window`.
- If the issue crosses several modules, start in `app/` and work downward.

## Why This Exists

As the repo grows, humans need a small number of reliable views into its structure:

- a module map
- generated dependency/class diagrams
- clear validation entry points
- current planning documents

Without that, the architecture becomes harder to hold in your head, even if the code itself stays clean.

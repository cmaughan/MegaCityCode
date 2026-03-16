# Module Map

This page is the human-friendly entry point for understanding how MegaCityCode is put together.

Use it when you want to answer:

- where a change probably belongs
- how the main libraries relate to each other
- which generated diagrams to look at first
- which validation flows are most relevant after a change

## Start Here

If you only want the shortest path to orientation:

1. Read the top-level layout in [README.md](../README.md).
2. Look at the target dependency graph in [deps.svg](deps/deps.svg).
3. Look at the class diagram in [megacitycode_classes.svg](uml/megacitycode_classes.svg).
4. Generate local API docs with `python scripts/build_docs.py --api-only`, then open `docs/api/index.html`.
5. Check active follow-up items in [index.md](../plans/work-items/index.md).
6. Use the smoke and snapshot flows through [do.py](../do.py) when touching rendering.

## Main Libraries

### app/

Top-level orchestration only.

Owns:
- app startup/shutdown
- wiring between window, renderer, text service, and retained grid state
- smoke/render-test entry points

Good place for:
- app lifecycle
- top-level CLI/test harness behavior
- integration glue

Bad place for:
- backend-specific renderer logic
- low-level font logic
- grid storage details

### libs/megacitycode-types/

Shared low-level data types and cross-module contracts.

Owns:
- shared structs
- event types
- logging/support types

Good place for:
- narrow shared contracts
- POD-like data passed between modules

### libs/megacitycode-window/

SDL windowing and platform-facing input/display behavior.

Owns:
- window creation
- DPI/display queries
- event pump / wake behavior
- clipboard / IME / title / focus plumbing

Good place for:
- platform window behavior
- SDL event translation

### libs/megacitycode-renderer/

Public renderer API plus Vulkan/Metal backends.

Owns:
- renderer interface
- GPU upload/submission code
- scene drawing
- frame capture for render snapshots

Good place for:
- camera/material behavior
- backend-specific GPU work
- readback/capture paths

### libs/megacitycode-font/

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

### libs/megacitycode-grid/

Thin retained cell storage reserved for later scene/text integration.

Owns:
- cell storage
- basic resize/clear/set/get behavior

Good place for:
- retained cell semantics
- future cell-backed scene/text integration points

## Generated Views

### Target graph

[deps.svg](deps/deps.svg)

Use this when:
- checking module boundaries
- spotting unexpected library dependencies
- deciding where a new abstraction belongs

### Class diagram

[megacitycode_classes.svg](uml/megacitycode_classes.svg)

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

### Deterministic render confidence

- `python do.py plane`
- `python do.py blessplane`
- `python do.py blessall`

### Documentation / hero image

- `python do.py shot`

## Planning Links

- Active work items: [plans/work-items/index.md](../plans/work-items/index.md)
- Design notes: [plans/design](../plans/design)
- Review notes: [plans/reviews](../plans/reviews)
- Learnings: [docs/learnings.md](learnings.md)

## Practical Heuristics

- If the issue is about retained cell contents or future text-state scaffolding, start in `megacitycode-grid`.
- If the issue is about how the scene is drawn, start in `megacitycode-renderer`.
- If the issue is about glyph choice, shaping, emoji, tofu, or atlas behavior, start in `megacitycode-font`.
- If the issue is about DPI, focus, clipboard, IME, or visible window behavior, start in `megacitycode-window`.
- If the issue crosses several modules, start in `app/` and work downward.

## Why This Exists

As the repo grows, humans need a small number of reliable views into its structure:

- a module map
- generated dependency/class diagrams
- clear validation entry points
- current planning documents

Without that, the architecture becomes harder to hold in your head, even if the code itself stays clean.

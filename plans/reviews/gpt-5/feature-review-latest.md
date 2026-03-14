# Feature Review — Spectre
**Reviewer:** gpt-5  
**Scope:** Current product gaps and stability opportunities based on the latest tree

---

## Best 5 Features To Add For Quality Of Life

### 1. User-configurable font and fallback settings

Why this is high value:

- font selection is currently heuristic and platform-path-driven
- users will want control over primary font, fallback order, size, and maybe line spacing/padding
- this would remove a whole class of “works on my machine” font/icon issues

Good shape:

- config file or CLI/env override
- primary font path/name
- fallback list override
- optional padding / line-height tuning

---

### 2. Clipboard integration

Why this matters:

- a Neovim GUI without reliable OS clipboard behavior feels incomplete immediately
- it is one of the first “frontend vs terminal” quality-of-life wins users expect

Good shape:

- support `+` / `*` clipboard registers cleanly
- document platform behavior
- test copy/paste round-trips with headless Neovim where possible

---

### 3. IME / compose / dead-key input support

Why this matters:

- this is the biggest remaining usability gap for non-Latin text entry
- the current input path is still basically “simple text and key events”
- it matters for real multilingual editing, not just edge cases

Good shape:

- SDL text-editing / composition support
- clear cursor/preedit handling
- tests or scripted scenarios for composition round-trips

---

### 4. Window/session persistence

Why this matters:

- users expect the app to remember size, position, and maybe font size
- even minimal persistence makes the frontend feel more deliberate and less disposable

Good shape:

- restore window geometry
- remember last font size
- optionally restore the last working directory or open-file arguments

---

### 5. Shell / file-open / drag-and-drop integration

Why this matters:

- “open this file in Spectre” is a natural frontend workflow
- drag-and-drop is a simple GUI affordance with high payoff

Good shape:

- support file arguments cleanly
- handle drag-and-drop open
- make multiple-file behavior explicit

---

## Best 5 Tests To Add For Stability

### 1. Direct `GridRenderingPipeline` unit tests

This is the most obvious missing seam-level test now.

Cover:

- dirty cell projection into `CellUpdate`
- highlight resolution
- space / empty / continuation cell behavior
- force-full atlas upload vs dirty-rect atlas upload

---

### 2. Scripted real-Neovim editing flow

Current smoke proves startup. It does not prove a real editing round-trip.

Cover:

- open buffer
- enter insert mode
- type text
- move cursor
- quit cleanly

---

### 3. Atlas exhaustion and recovery tests

Once atlas recovery exists, it needs explicit coverage.

Cover:

- atlas fills up
- recovery path triggers
- previously visible text is still rendered after recovery

---

### 4. Font fallback and cache policy tests

The current font path has real policy complexity and little direct coverage.

Cover:

- primary font hit
- fallback font hit
- both miss
- repeated cluster cache reuse
- bounded/eviction behavior if cache policy changes

---

### 5. Resize and font-size regression smoke

This repo has already had regressions here, so it deserves direct coverage.

Cover:

- window resize
- font-size change
- relayout correctness
- cursor still aligned afterward

---

## Worst 5 Current Features

These are the weakest current product traits, not necessarily code defects.

### 1. Font behavior is still too heuristic

- works better now than before
- still driven by platform path guessing instead of user-controlled settings

### 2. Input coverage is still narrow

- common keys work
- richer keyboard cases and multilingual input do not feel frontend-complete yet

### 3. Clipboard story is absent

- for many users, this is the first missing GUI feature they will notice

### 4. Atlas overflow has no graceful user-facing recovery

- the current fallback is effectively “glyphs disappear and a warning is logged”

### 5. There is still no real user config surface

- the repo is well-instrumented for developers
- it is still fairly thin on end-user controls and preferences

---

## Suggested Feature Order

1. Clipboard integration
2. User-configurable font/fallback settings
3. Resize/font-size regression tests
4. Real editing-flow smoke test
5. IME / composition support

That order gives the fastest user-visible payoff while still improving stability in parallel.

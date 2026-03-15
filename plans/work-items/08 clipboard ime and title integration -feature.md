# 08 Clipboard IME And Title Integration -feature

## Problem

Several basic GUI affordances are still missing:
- system clipboard integration
- IME/composition handling
- dynamic window title updates

## Goal

Close the biggest day-to-day integration gaps for normal desktop editing.

## Main Files

- `libs/spectre-window/src/sdl_window.cpp`
- `libs/spectre-types/include/spectre/events.h`
- `libs/spectre-nvim/src/input.cpp`
- app/title handling

## Implementation Plan

1. Implement clipboard bridge behavior first, because it has the broadest day-to-day impact.
2. Add IME composition event handling, not just committed text input.
3. Wire Neovim title updates into the native window title.
4. Keep platform differences contained behind the window/input layer.

## Suggested Split

- Agent 1: clipboard
- Agent 2: IME/composition
- Agent 3: title updates and docs

## Exit Criteria

- copy/paste works through the system clipboard
- composition text is supported
- window title tracks Neovim state

# Clipboard Provider Integration

Status: pending follow-up

## Problem

Spectre currently provides practical clipboard shortcuts at the app layer:

- `Ctrl+Shift+C` copies Neovim's unnamed register to the OS clipboard
- `Ctrl+Shift+V` pastes OS clipboard text into Neovim

That is useful, but it is not a real Neovim clipboard provider. It does not make `"+` and `"*` behave like first-class system clipboard registers, and it does not integrate with normal yank/delete/paste flows the way users expect from a GUI frontend.

## Goal

Make Spectre act as a proper system clipboard bridge for embedded Neovim, so clipboard access works through normal Neovim mechanisms rather than only through GUI-specific shortcuts.

## Suggested Scope

1. Support `"+` and `"*` reads from the system clipboard.
2. Support `"+` and `"*` writes to the system clipboard.
3. Keep unnamed-register behavior predictable when `clipboard` includes `unnamed` or `unnamedplus`.
4. Preserve the existing shortcut path as a fallback or convenience layer.
5. Cover Windows and macOS with the same high-level behavior.

## Likely Implementation Shape

- Add an explicit clipboard bridge in the app/RPC layer rather than burying it in ad hoc key handling.
- Use SDL clipboard APIs as the platform boundary.
- Expose provider operations to Neovim through the embedded-session path, likely by setting up provider hooks or a frontend-owned clipboard RPC bridge.
- Keep the implementation text-only unless there is a clear need for richer clipboard formats later.

## Validation

- Yanking to `"+y` updates the OS clipboard.
- Pasting from `"+p` reads the OS clipboard.
- `set clipboard=unnamedplus` behaves as expected in normal editing flow.
- Existing `Ctrl+Shift+C` / `Ctrl+Shift+V` shortcuts still work.
- Add tests where practical around the provider/bridge layer, plus a manual smoke checklist for real clipboard behavior.

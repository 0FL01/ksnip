# MVP scope

## Must have

- Clean Qt 6 build on the target host.
- Runtime probe for `org.kde.KWin.ScreenShot2`.
- Native KDE capture for:
  - full workspace;
  - active screen;
  - active window;
  - user-selected window;
  - selected rectangular area.
- Existing generic Screenshot Portal remains available as fallback.
- Wayland global shortcuts through XDG GlobalShortcuts Portal.
- Built-in capture shortcuts only are sufficient for MVP.
- No polling while idle.
- No blocking work on the GUI thread.
- Correct cancel behavior.
- No empty-image success result.
- User-local installation that satisfies KWin/portal desktop-file identity.
- Final binary and verification artifacts in `dist/`.

## Low-cost additions allowed after all must-haves are green

- Last rectangular area.
- Cursor inclusion.
- Window decoration setting.
- A small optional latency trace controlled by an environment variable.
- Fixing a verified incorrect desktop action.

## Explicitly out of scope

Do not implement these during this iteration:

- PipeWire screenshot sessions.
- ScreenCast-based snapshot capture.
- Screenshot Portal v3 target selection.
- GNOME private screenshot APIs.
- `ext-image-copy-capture`.
- `wlr-screencopy`.
- Hyprland/Sway-specific command execution.
- A generic plugin registry, backend scoring engine, or dependency injection
  rewrite.
- New capture-mode enums merely to rename “window under cursor”.
- A broad settings UI redesign.
- Custom user action hotkeys on Wayland.
- RPM, AppImage, Flatpak, Snap, or container packaging.
- Native-resolution mixed-DPI composition comparable to Spectacle.
- Refactoring unrelated legacy code.
- Disabling KWin permission checks or editing system portal files.

## Meaning of Pareto for this task

The first 80% of value is:

1. native KWin capture;
2. area overlay;
3. global shortcut activation;
4. a correctly installed binary.

Everything else waits until those four items are proven working.

# Definition of done

The task is done only when all mandatory checks below pass on the target KDE
Wayland session.

## Build

- `cmake` configure succeeds with Qt 6.
- Release or RelWithDebInfo build succeeds.
- `file dist/ksnip` reports an x86-64 ELF executable.
- `ldd dist/ksnip` has no “not found” entries.
- `dist/ksnip --version` succeeds.
- Existing automated tests pass, or every pre-existing unrelated failure is
  identified with evidence.

## Capture

From the installed user-local build:

- Full workspace returns a non-empty image.
- Active screen returns a non-empty image.
- Active window returns the expected window.
- Selected-window mode allows the user to click/select a window and returns it.
- Rectangular area uses the KSnip overlay and returns the selected pixels.
- Escape cancels area and interactive-window selection without an error dialog.
- No KDE Screenshot Portal picker appears on the native ScreenShot2 path.
- Cursor setting does not crash even if cursor inclusion is not fully polished.
- At least 30 consecutive area captures complete without a hang, descriptor
  leak, or empty image.
- At least 30 consecutive full-screen captures complete without a hang or empty
  image.

## Global shortcuts

With another application focused:

- RectArea shortcut opens the selector.
- FullScreen shortcut captures.
- ActiveWindow shortcut captures.
- SelectedWindow shortcut starts interactive window selection.
- Restarting KSnip does not create duplicate activations.
- Idle CPU usage does not show a shortcut polling loop.
- Changing shortcut settings either rebinds through a recreated session or
  produces a clear supported limitation; it must not silently leave stale
  bindings.

## Responsiveness

Measure on the target host, not in a VM:

- Shortcut activation to area-overlay visibility:
  - median under 200 ms over 20 runs;
  - no multi-second outlier caused by KSnip.
- No `sleep`, busy-wait, or blocking read runs on the GUI thread.
- Resident startup is not repeated for each shortcut activation.

If the portal adds a one-time authorization/configuration dialog, exclude that
first-time user interaction from latency measurements.

## Installation and security

- `dist/install-user.sh` uses no `sudo`.
- It installs a desktop entry whose absolute `Exec` path matches the binary.
- The desktop entry declares `org.kde.KWin.ScreenShot2`.
- Native capture works when launched from the installed desktop entry.
- `dist/uninstall-user.sh` removes only files created by the installer.
- No system portal or KWin files were modified.
- No security check was disabled.

## Delivery

`dist/` contains:

```text
ksnip
install-user.sh
uninstall-user.sh
org.ksnip.ksnip.desktop
SHA256SUMS
build-info.txt
test-report.md
source.patch
```

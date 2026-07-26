# Execution order

Execute strictly in this order. Do not start a later phase while the current
one is red.

## Phase 0 — baseline

- Record git SHA and dirty state.
- Initialize submodules.
- Build upstream unchanged with Qt 6.
- Run `ksnip --version`.
- Probe ScreenShot2 and GlobalShortcuts D-Bus interfaces.
- Record the exact build command and existing failures in `STATE.md`.

## Phase 1 — ScreenShot2 core

Implement and manually verify:

- full workspace;
- active screen;
- active window;
- interactive selected window;
- cancellation and errors.

Do not touch area selection or hotkeys yet.

## Phase 2 — rectangular area

Connect the existing KSnip Wayland selector to `CaptureArea`.

Verify:

- overlay appears immediately;
- Escape cancels;
- selected image matches the rectangle;
- no portal screenshot dialog appears;
- repeated captures do not hang.

## Phase 3 — Wayland global shortcuts

Implement GlobalShortcuts Portal session and bind built-in capture actions.

Verify activation while another application has focus.

## Phase 4 — user-local installation

Generate installer/uninstaller and test the installed desktop entry, not just
the build-tree executable.

## Phase 5 — regression, latency, delivery

Run the acceptance checklist, create `dist/`, compute checksum, and write the
test report.

Suggested commit boundaries:

```text
build: record Qt6 Fedora baseline
feat(wayland): use KWin ScreenShot2 for KDE captures
feat(wayland): capture rectangular areas through KWin
feat(wayland): add portal global shortcuts
build: add user-local install and delivery artifacts
```

Do not combine unrelated cleanup with these commits.

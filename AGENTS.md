# Project: ksnip

KSnip is a cross-platform Qt screenshot and annotation application. The current
Linux objective is a fast, locally installable Qt 6 build for KDE Plasma on
Wayland, with native KWin capture and portal-backed global shortcuts.

Tech stack: C++17, Qt 5/6 Widgets and QtDBus, CMake, GoogleTest, kImageAnnotator,
and kColorPicker.

## Source of Truth

* Treat the checked-out code as authoritative; verify signatures and runtime
  paths before relying on task notes.
* `ksnip-wayland-agent-context-pack/` is the execution contract for the current
  Wayland objective.
* Read `00_PRIMARY_GOAL.md`, `02_MVP_SCOPE.md`, `03_MINIMAL_DESIGN.md`, and
  `05_ACCEPTANCE_CRITERIA.md` before changing Wayland capture or shortcuts.
* Follow the task files in `ksnip-wayland-agent-context-pack/tasks/` in numeric
  order and keep `ksnip-wayland-agent-context-pack/STATE.md` current after each
  phase.

## Workspace Overview

* `src/backend/` - capture backends, configuration, CLI handling, persistence,
  and upload/save services.
* `src/gui/` - main capture flow, selector overlay, global shortcuts, settings,
  tray workflow, and editor integration.
* `src/dependencyInjector/` - runtime composition and platform-specific service
  selection.
* `src/common/` - shared enums, DTOs, platform checks, and helpers.
* `tests/` - GoogleTest executables built from the production source list.
* `desktop/` - desktop entry, application metadata, and Linux icon installation.
* `libraries/` - kImageAnnotator and kColorPicker git submodules.
* `ksnip-wayland-agent-context-pack/` - objective, design constraints, phase
  tasks, probes, acceptance criteria, and delivery contract.

## Runtime Architecture

* `DependencyInjectorBootstrapper::injectImageGrabber()` is the active image
  backend composition point. Do not assume the legacy `ImageGrabberFactory.cpp`
  participates in the built runtime; it is not currently in `KSNIP_SRCS`.
* `IImageGrabber` emits `finished(CaptureDto)` or `canceled()`; `MainWindow`
  forwards captures into the existing editor and action pipeline.
* `AbstractImageGrabber` owns common mode, delay, and cursor state.
  `AbstractRectAreaImageGrabber` owns the reusable selector lifecycle for
  backends that capture by rectangle.
* `KdeWaylandImageGrabber` is the KDE-native backend. `WaylandImageGrabber` is
  the existing `org.freedesktop.portal.Screenshot` fallback and must remain
  available when ScreenShot2 is unavailable or generic Wayland is forced.
* `WaylandSnippingArea` is the existing live Wayland selector; reuse it rather
  than adding a second overlay.
* `GlobalHotKeyHandler` maps configured shortcuts to the existing
  `captureTriggered(CaptureModes)` and `actionTriggered(Action)` signals.
  X11 and Windows currently use native key handlers; Wayland must use the
  GlobalShortcuts portal at this high-level action boundary.

## Wayland Contracts

* Select the KDE backend only after a runtime probe of:
  `org.kde.KWin.ScreenShot2`, `/org/kde/KWin/ScreenShot2`, interface
  `org.kde.KWin.ScreenShot2`, property `Version`.
* Map capture modes without adding new enums: `FullScreen` to
  `CaptureWorkspace`, `CurrentScreen` to `CaptureActiveScreen`, `ActiveWindow`
  to `CaptureActiveWindow`, `WindowUnderCursor` to `CaptureInteractive(0)`, and
  `RectArea` to the KSnip overlay followed by `CaptureArea`.
* ScreenShot2 calls must be asynchronous. Read its blocking pipe on a worker,
  use finite timeouts, handle partial reads and early EOF, validate all raw
  image metadata, and close every descriptor exactly once.
* Never block or poll on the GUI thread. Do not use sleeps, busy-waits, idle
  polling, recursive pipe reads, or an `O_NONBLOCK` retry loop.
* Cancellation is not failure. Never emit a null, partial, or empty image as a
  successful capture, and prevent stale async completions after destruction.
* Rectangular capture uses global logical coordinates. Close the transparent
  overlay before `CaptureArea`; do not invoke the portal picker, capture the
  whole workspace first, or crop a full-workspace image.
* Use one `org.freedesktop.portal.GlobalShortcuts` session per resident process.
  Stable built-in IDs map directly to existing capture signals. Recreate the
  session after hotkey settings change and close it on shutdown; do not emulate
  raw native key events or add custom-action shortcuts for this MVP.

## Architectural Invariants

* Preserve the generic Screenshot Portal fallback and existing X11, Windows,
  macOS, and non-KDE behavior.
* Prefer one small ScreenShot2 helper and one small portal shortcut manager only
  when they remove duplication or make async ownership testable.
* Do not add a backend registry, plugin/scoring framework, new capture-mode enum,
  dependency-injection rewrite, or new dependency unless the required behavior
  cannot be implemented with existing Qt 6 and QtDBus facilities.
* PipeWire, ScreenCast snapshots, GNOME private APIs, wlroots protocols,
  compositor-specific commands, packaging frameworks, and broad settings or
  mixed-DPI redesigns are out of scope.
* KWin ScreenShot2 is restricted. Keep desktop ID `org.ksnip.ksnip`, install a
  desktop entry whose absolute `Exec` matches the installed binary, and retain
  `X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2`.
* Never disable compositor permission checks, edit system KWin/portal files,
  write to `/usr` during development, or use `sudo` in delivery scripts.
* Preserve license attribution when adapting code from KDE Spectacle or KWin.

## Development Practices

* Follow `CODINGSTYLE.md`: KDELibs-derived style, tabs for indentation,
  `mCamelCase` members, mixed-case class filenames, and cumulative headers.
* A `.cpp` normally includes only its matching header; required dependencies
  belong in the cumulative header.
* Add production sources to `KSNIP_SRCS` in `src/CMakeLists.txt`; test sources
  are listed explicitly in `tests/CMakeLists.txt`.
* Unit-test names use
  `<MethodUnderTest>_Should_<ExpectedBehavior>_When_<OptionalCondition>`.
* Keep changes phase-local. Do not combine Wayland work with unrelated cleanup,
  formatting, translations, or legacy refactors.

## Build and Verification

Initialize dependencies once:

```bash
git submodule update --init --recursive
```

Configure and build the required Qt 6 baseline:

```bash
cmake -S . -B build-agent -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBUILD_WITH_QT6=ON \
  -DUSE_SUBMODULE_KCOLORPICKER=ON \
  -DUSE_SUBMODULE_KIMAGEANNOTATOR=ON
cmake --build build-agent --parallel
```

Configure and run automated tests when GoogleTest is available:

```bash
cmake -S . -B build-tests -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_WITH_QT6=ON -DBUILD_TESTS=ON \
  -DUSE_SUBMODULE_KCOLORPICKER=ON \
  -DUSE_SUBMODULE_KIMAGEANNOTATOR=ON
cmake --build build-tests --parallel
ctest --test-dir build-tests --output-on-failure
```

Focused test pattern:

```bash
ctest --test-dir build-tests -R '<TestName>' --output-on-failure
```

Probe the target session without modifying it:

```bash
bash ksnip-wayland-agent-context-pack/scripts/probe-wayland.sh
```

Build after each meaningful phase and test each capture mode as soon as it is
available. Final capture, authorization, global-shortcut, latency, repetition,
and descriptor-leak checks require the target KDE Wayland session and the
installed user-local desktop entry.

## Execution and Delivery

* Keep phases green in order: baseline, ScreenShot2 core, rectangular area,
  GlobalShortcuts, user-local installation, then regression and delivery.
* Record exact commands, outputs, failures, and the next action in `STATE.md`.
  A blocker must identify a concrete failed command and missing prerequisite.
* User-local installation is confined to `~/.local`, backs up a pre-existing
  user desktop entry, and removes only installer-created files.
* The final `dist/` contract is: `ksnip`, `install-user.sh`,
  `uninstall-user.sh`, `org.ksnip.ksnip.desktop`, `SHA256SUMS`,
  `build-info.txt`, `test-report.md`, and `source.patch`.

## Detailed References

* `README.md` - general features, dependencies, and upstream build guidance.
* `CODINGSTYLE.md` - C++ formatting and test naming conventions.
* `ksnip-wayland-agent-context-pack/03_MINIMAL_DESIGN.md` - D-Bus transport,
  selector, shortcut session, and installation design.
* `ksnip-wayland-agent-context-pack/05_ACCEPTANCE_CRITERIA.md` - mandatory
  functional, responsiveness, installation, and delivery checks.
* `ksnip-wayland-agent-context-pack/06_BUILD_AND_DELIVERY.md` - exact build,
  probe, metadata, and patch requirements.

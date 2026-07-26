# KSnip Wayland Agent Context Pack


---

<!-- README_RU.md -->

# Контекст-пак: KSnip для KDE Plasma Wayland

Дата фиксации контекста: **2026-07-26**.

Этот каталог предназначен для помещения в корень рабочего checkout `ksnip` как `AGENT_CONTEXT/`.

## Запуск агента

1. Поместить каталог в репозиторий:
   ```bash
   cp -a ksnip-wayland-agent-context-pack ./AGENT_CONTEXT
   ```
2. Передать агенту единственную команду:
   ```text
   Прочитай AGENT_CONTEXT/PROMPT_CODING_AGENT.md и выполни задачу до получения
   проверенного бинарника и комплекта dist/. Не останавливайся на плане.
   ```
3. Агент обязан вести `AGENT_CONTEXT/STATE.md`, выполнять задачи по порядку и
   оставлять рабочий результат после каждого этапа.

## Что означает «универсальное решение» в этом MVP

Это **не** обещание одинакового нативного захвата на каждом Wayland-композиторе.

В рамках MVP универсальность означает:

- быстрый нативный путь на KDE Plasma через `org.kde.KWin.ScreenShot2`;
- сохранение существующего Screenshot Portal как fallback;
- глобальные хоткеи Wayland через стандартный
  `org.freedesktop.portal.GlobalShortcuts`;
- отсутствие поломок существующих X11/Windows/macOS путей;
- небольшая архитектура, которую позже можно расширить без переписывания MVP.

## Главный принцип

Сначала получить надёжный локально устанавливаемый бинарник для целевой машины.
Не строить заранее framework для GNOME, wlroots, PipeWire и будущих протоколов.


---

<!-- 00_PRIMARY_GOAL.md -->

# Primary objective

Modify the current upstream KSnip checkout and deliver a **working, locally
installable, Qt 6, x86_64 Linux binary** for this target:

- Fedora Linux
- KDE Plasma 6.7.3
- Wayland session
- AMD Ryzen 7 7840HS
- resident/tray workflow
- low-latency global shortcuts

The installed build must provide these capture actions:

1. Rectangular area selected with the existing KSnip overlay.
2. Full workspace / all screens.
3. Active window.
4. User-selected window.
5. Current/active screen when it is cheap to retain.

Required implementation paths:

- KDE Plasma capture: `org.kde.KWin.ScreenShot2`.
- Other Wayland capture: preserve the existing Screenshot Portal fallback.
- Wayland global shortcuts: `org.freedesktop.portal.GlobalShortcuts`.
- X11 and Windows global shortcuts: preserve the existing implementations.

The final result is not an analysis or patch proposal. The final result is:

- a built ELF executable;
- a safe user-local installer;
- a safe uninstaller;
- build metadata;
- SHA-256 checksum;
- test report;
- source diff or commit list.

Success is measured on the target KDE Wayland session, not by theoretical
support for every compositor.


---

<!-- 01_BASELINE_AND_FACTS.md -->

# Baseline and verified facts

Treat the actual checkout as the source of truth. Re-check every item before
editing because upstream may move.

At context-pack creation time, upstream had the following relevant properties:

- C++17 and CMake.
- Optional Qt 6 build via `BUILD_WITH_QT6=ON`.
- `KdeWaylandImageGrabber` still used the legacy:
  - service `org.kde.KWin`
  - object `/Screenshot`
  - interface `org.kde.kwin.Screenshot`
  - serialized `QImage` transport through `QDataStream`
- The legacy KDE grabber advertised several modes but routed unmatched modes to
  the old `interactive` method. In particular, advertised RectArea support was
  not a correct KSnip-overlay plus `CaptureArea` implementation.
- `KeyHandlerFactory` returned `DummyKeyHandler` on Wayland.
- The existing high-level `GlobalHotKeyHandler` already maps configured
  shortcuts to capture modes and actions.
- The generic Wayland grabber already calls
  `org.freedesktop.portal.Screenshot`; keep it as fallback.
- The desktop entry already mentioned
  `X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2`, but its `Exec`
  value was hard-coded and must match the actually installed binary for a
  reliable development installation.
- The existing “FullScreen” desktop action used `ksnip -m`; inspect CLI
  semantics and correct it only if verified wrong.

Likely relevant files:

```text
CMakeLists.txt
src/CMakeLists.txt
desktop/org.ksnip.ksnip.desktop

src/backend/imageGrabber/KdeWaylandImageGrabber.*
src/backend/imageGrabber/WaylandImageGrabber.*
src/backend/imageGrabber/ImageGrabberFactory.*
src/backend/imageGrabber/AbstractImageGrabber.*
src/backend/imageGrabber/AbstractRectAreaImageGrabber.*

src/gui/snippingArea/WaylandSnippingArea.*
src/gui/snippingArea/AbstractSnippingArea.*

src/gui/globalHotKeys/GlobalHotKeyHandler.*
src/gui/globalHotKeys/GlobalHotKey.*
src/gui/globalHotKeys/keyHandler/KeyHandlerFactory.*

src/dependencyInjector/*
src/common/platform/PlatformChecker.*
```

Reference implementation for a minimal subset of ScreenShot2 behavior:

```text
KDE Spectacle:
src/Platforms/ImagePlatformKWin.cpp
src/Platforms/ImagePlatformKWin.h

KWin protocol definition:
src/plugins/screenshot/org.kde.KWin.ScreenShot2.xml
```

Do not copy a large Spectacle subsystem. Reuse only the small transport pattern
needed by KSnip and preserve copyright/license attribution for copied code.


---

<!-- 02_MVP_SCOPE.md -->

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


---

<!-- 03_MINIMAL_DESIGN.md -->

# Minimal design

## 1. Backend selection

Do not build a generalized backend framework.

Use a small runtime probe:

```text
Wayland + ScreenShot2 service/path/interface available
    -> KdeWaylandImageGrabber using ScreenShot2
otherwise
    -> existing WaylandImageGrabber portal fallback
```

Prefer probing the D-Bus interface/property over trusting only
`XDG_CURRENT_DESKTOP`.

## 2. KWin ScreenShot2 endpoint

```text
service:   org.kde.KWin.ScreenShot2
path:      /org/kde/KWin/ScreenShot2
interface: org.kde.KWin.ScreenShot2
property:  Version
```

Minimal mode mapping:

```text
FullScreen        -> CaptureWorkspace
CurrentScreen     -> CaptureActiveScreen
ActiveWindow      -> CaptureActiveWindow
WindowUnderCursor -> CaptureInteractive(kind = 0)
RectArea          -> existing KSnip overlay, then CaptureArea
```

For KDE Wayland, the existing `WindowUnderCursor` action may represent
“click/select a window” through `CaptureInteractive(0)`. Do not add a new enum
or translate the whole UI in this MVP.

## 3. ScreenShot2 transport

Use the proven minimal pattern from Spectacle:

1. Create a blocking pipe with `pipe2(..., O_CLOEXEC)`.
2. Do **not** retain the old `O_NONBLOCK` plus recursive polling loop.
3. Build an asynchronous D-Bus call and pass the write descriptor as
   `QDBusUnixFileDescriptor`.
4. Close the local write descriptor after dispatch.
5. Use a finite timeout:
   - normal capture: approximately 4 seconds;
   - interactive window selection: approximately 60 seconds.
6. On a successful `QVariantMap` reply:
   - require `type == "raw"`;
   - validate `width`, `height`, `format`, `stride`, and calculated byte sizes;
   - allocate a `QImage`;
   - read the pipe in a worker/future, never on the GUI thread;
   - handle partial reads and early EOF;
   - set `devicePixelRatio` from `scale` when valid;
   - emit success only for a non-null complete image.
7. Treat `org.kde.KWin.ScreenShot2.Error.Cancelled` as cancellation, not failure.
8. Close every descriptor exactly once on every path.
9. Prevent stale async completions from acting on a destroyed object.

Keep the implementation small. One focused helper such as
`KWinScreenShot2Client` is acceptable only if it reduces duplication and makes
metadata parsing testable.

## 4. Area selection flow

Reuse `WaylandSnippingArea` / the existing KSnip selector.

Preferred MVP flow:

```text
shortcut/action
  -> show transparent KSnip overlay without frozen screenshot
  -> user selects logical QRect
  -> overlay closes
  -> event loop resumes
  -> CaptureArea(x, y, width, height)
  -> image enters the existing editor pipeline
```

Rules:

- Escape emits cancellation.
- Reject empty or negative-size rectangles.
- Use global logical coordinates expected by KWin.
- Avoid arbitrary sleep delays.
- Do not first capture the whole desktop and crop it.
- Do not use the portal picker on the native KDE path.
- Save last-rect only if it is already a trivial integration.

## 5. Global shortcuts

Keep the existing `GlobalHotKeyHandler` for non-Wayland platforms.

On Wayland, add one small manager around:

```text
org.freedesktop.portal.Desktop
/org/freedesktop/portal/desktop
org.freedesktop.portal.GlobalShortcuts
```

Session lifecycle:

1. Probe the portal interface/version.
2. Create one session for the resident KSnip process.
3. Use stable shortcut IDs:
   - `capture.rect_area`
   - `capture.full_screen`
   - `capture.current_screen`
   - `capture.active_window`
   - `capture.select_window`
4. List existing shortcuts for the application.
5. Bind missing shortcuts once, with descriptions and preferred triggers.
6. Map `Activated` signals directly to existing
   `captureTriggered(CaptureModes)` behavior.
7. On hotkey settings changes, close and recreate the portal session rather
   than trying to mutate a session that only permits one bind operation.
8. Close the session on shutdown.
9. Do not poll.

Do not force the portal into the native event-filter/IKeyHandler abstraction.
Portal activation is an action signal, not a raw key event.

If and only if the target host is proven not to expose a usable
GlobalShortcuts Portal, a small optional `KF6::GlobalAccel` fallback may be
considered. Do not implement both preemptively.

## 6. Desktop identity and installation

KWin ScreenShot2 is a restricted D-Bus interface. The development build must
be launched through an installed desktop entry whose identity and `Exec` path
match the actual binary.

The final package must install only into the user account, for example:

```text
~/.local/libexec/ksnip-wayland/ksnip
~/.local/share/applications/org.ksnip.ksnip.desktop
~/.local/bin/ksnip-wayland -> actual binary
```

The generated desktop entry must:

- use the absolute actual executable path;
- declare `org.kde.KWin.ScreenShot2`;
- retain normal KSnip metadata/actions;
- use the desktop ID expected by the application.

Never disable compositor security checks. Never patch system portal files.


---

<!-- 04_EXECUTION_ORDER.md -->

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


---

<!-- 05_ACCEPTANCE_CRITERIA.md -->

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


---

<!-- 06_BUILD_AND_DELIVERY.md -->

# Build and delivery contract

## Baseline build

Prefer Ninja and submodule dependencies to minimize host-version mismatch:

```bash
git submodule update --init --recursive

cmake -S . -B build-agent -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBUILD_WITH_QT6=ON \
  -DUSE_SUBMODULE_KCOLORPICKER=ON \
  -DUSE_SUBMODULE_KIMAGEANNOTATOR=ON

cmake --build build-agent --parallel
```

If system libraries are intentionally used instead, record their exact package
versions in `dist/build-info.txt`.

Do not install missing packages silently. Print the exact Fedora `dnf install`
or `dnf builddep` command required, then continue after dependencies exist.

## Test build

When the existing test configuration is viable:

```bash
cmake -S . -B build-tests -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_WITH_QT6=ON \
  -DBUILD_TESTS=ON \
  -DUSE_SUBMODULE_KCOLORPICKER=ON \
  -DUSE_SUBMODULE_KIMAGEANNOTATOR=ON

cmake --build build-tests --parallel
ctest --test-dir build-tests --output-on-failure
```

Do not spend the iteration repairing unrelated ancient tests.

## D-Bus probes

Useful non-mutating checks:

```bash
busctl --user get-property \
  org.kde.KWin.ScreenShot2 \
  /org/kde/KWin/ScreenShot2 \
  org.kde.KWin.ScreenShot2 \
  Version

busctl --user introspect \
  org.kde.KWin.ScreenShot2 \
  /org/kde/KWin/ScreenShot2

busctl --user get-property \
  org.freedesktop.portal.Desktop \
  /org/freedesktop/portal/desktop \
  org.freedesktop.portal.GlobalShortcuts \
  version
```

## Build metadata

`dist/build-info.txt` must include:

- UTC build date;
- repository URL;
- source commit SHA;
- dirty/clean status;
- compiler and version;
- CMake version;
- Qt version;
- kernel;
- Fedora release;
- Plasma version;
- session type;
- exact configure and build commands;
- binary path;
- `file` output;
- relevant `ldd` output.

## Patch

Produce:

```bash
git diff --binary <baseline-sha>...HEAD > dist/source.patch
```

If commits are not created, use the baseline working tree diff and state that
clearly.


---

<!-- 07_AGENT_OPERATING_RULES.md -->

# Agent operating rules

## Behavior

- Do the work; do not stop after producing a plan.
- Verify the actual checkout before trusting this context pack.
- Keep `STATE.md` current after every phase.
- Prefer the smallest coherent patch.
- Compile after each meaningful change.
- Test each capture mode as soon as it exists.
- Keep the user-facing behavior unchanged outside the target features.
- Preserve existing code style unless a touched block needs local cleanup.
- Keep commits small and bisectable.

## KISS

- At most one small ScreenShot2 helper and one small portal shortcut manager.
- Reuse existing KSnip signals, capture DTOs, settings, selector, and editor
  pipeline.
- Do not introduce a backend registry or abstract factory hierarchy for future
  compositors.
- Do not add dependencies unless the required behavior cannot be achieved with
  Qt 6 and QtDBus.

## YAGNI

- Do not implement future protocols.
- Do not add settings without an immediate acceptance criterion.
- Do not add a new capture mode just for naming.
- Do not make packaging portable across distributions in this iteration.
- Do not support custom action shortcuts on Wayland in the MVP.

## Pareto

When choosing between:

- a general solution that delays the binary; and
- a small solution that cleanly handles KDE plus a generic fallback;

choose the second.

## Safety

- Never edit `/usr/share`, `/usr/lib`, KWin config, or portal config as part of
  development.
- Never disable permission checks.
- Never use `sudo` in generated installer scripts.
- Do not overwrite an unrelated user file without backup and confirmation.
- Close D-Bus requests, sessions, watchers, and file descriptors.
- Avoid destructive git operations.
- Preserve license notices for adapted upstream code.

## Blocker policy

A blocker is valid only when it prevents code, build, or runtime verification.

When blocked:

1. Capture the exact command and complete error.
2. Identify the smallest missing prerequisite.
3. Record it in `STATE.md`.
4. Continue every task that does not depend on it.

Do not classify uncertainty or unfamiliar code as a blocker.


---

<!-- PROMPT_CODING_AGENT.md -->

# Prompt for the primary coding agent

You are the primary implementation agent working directly in a KSnip checkout.

Read these files in order:

1. `AGENT_CONTEXT/00_PRIMARY_GOAL.md`
2. `AGENT_CONTEXT/01_BASELINE_AND_FACTS.md`
3. `AGENT_CONTEXT/02_MVP_SCOPE.md`
4. `AGENT_CONTEXT/03_MINIMAL_DESIGN.md`
5. `AGENT_CONTEXT/04_EXECUTION_ORDER.md`
6. `AGENT_CONTEXT/05_ACCEPTANCE_CRITERIA.md`
7. `AGENT_CONTEXT/06_BUILD_AND_DELIVERY.md`
8. `AGENT_CONTEXT/07_AGENT_OPERATING_RULES.md`
9. every file in `AGENT_CONTEXT/tasks/`

Then execute the work to completion.

## Mission

Produce a fast, reliable, locally installable Qt 6 KSnip binary for Fedora,
KDE Plasma 6.7.3, Wayland.

The mandatory user workflow is:

- keep KSnip resident;
- press a global shortcut;
- immediately capture a rectangular area, full workspace, active window, or
  user-selected window;
- receive the image in the normal KSnip editor.

Use:

- KWin `org.kde.KWin.ScreenShot2` for native KDE capture;
- the existing Screenshot Portal as non-KDE/unavailable fallback;
- XDG `org.freedesktop.portal.GlobalShortcuts` for Wayland shortcuts.

## Non-negotiable delivery

Do not finish with only explanations, pseudocode, or an unbuilt patch.

Finish only after creating `dist/` with:

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

## Execution contract

- Start by updating `AGENT_CONTEXT/STATE.md`.
- Establish an unchanged upstream Qt 6 baseline before editing.
- Work through task files in numeric order.
- Build after every task.
- Make a small commit after every green task when the checkout permits it.
- Use the actual checkout as source of truth and adapt file names if upstream
  moved.
- Do not broaden scope to PipeWire, GNOME-native capture, wlroots protocols,
  packaging frameworks, or unrelated refactors.
- Do not disable KWin security or edit system portal files.
- Do not use `sudo` in the final installer.
- Do not stop merely because a part is unfamiliar; inspect the repository and
  primary protocol implementations.

## Decision gates

- Add a small helper only when it removes duplication or makes async ownership
  testable.
- Use the GlobalShortcuts Portal first.
- Consider an optional KF6 GlobalAccel fallback only after proving that the
  target portal is absent or unusable.
- Implement LastRectArea only after every mandatory acceptance check is green.
- Fix mixed-DPI edge cases only when reproduced on the target setup or when a
  very small correction is obvious.

Begin with Phase 0 now.

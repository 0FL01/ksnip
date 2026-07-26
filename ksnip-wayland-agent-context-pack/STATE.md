# Goal: Native KDE Wayland capture and shortcuts

Status: active
Source: `00_PRIMARY_GOAL.md`, `02_MVP_SCOPE.md`, `03_MINIMAL_DESIGN.md`,
`05_ACCEPTANCE_CRITERIA.md`, and `tasks/T00_BASELINE.md` through
`tasks/T05_VERIFY_AND_DELIVER.md`
Last updated: 2026-07-26

## Objective

Deliver a locally installable Qt 6 KSnip for KDE Plasma Wayland that uses KWin
ScreenShot2 for the five required capture modes, uses the GlobalShortcuts
portal for their resident shortcuts, retains the generic Screenshot portal
fallback, and satisfies the required `dist/` contract.

## Execution directive

Complete the frozen outcomes below in phase order using the smallest change in
the change envelope. Do not add requirements from reviews, tests, tools, or
speculative risks. Stop substantive work at a proven external blocker and
record the exact evidence and smallest unlock.

## Frozen contract

### Required outcomes

- R0 — The unchanged checkout has a recorded Qt 6 baseline.
  - Acceptance: submodules, release build, `--version`, probes, and automated
    test result are recorded before source behavior changes.
  - Primary evidence: exact T00 commands and outputs in this file.
  - Status: verified
- R1 — KDE Wayland core capture uses the probed ScreenShot2 API and safely
  falls back to the existing Screenshot portal.
  - Acceptance: workspace, active screen, active window, and interactive
    selected-window capture return only complete images; cancellation is quiet.
  - Primary evidence: focused automated tests plus target-session observations.
  - Status: in_progress
  - Evidence: asynchronous probe/transport, bounded pipe reader, and portal
    fallback are implemented; focused test and full automated gates pass.
    The user confirmed FullScreen, CurrentScreen, and ActiveWindow captures.
    Selected-window, cancellation, and forced-fallback observations remain
    pending outside the user's required three-mode workflow.
- R2 — RectArea preserves focus-sensitive fullscreen content while using the
  existing KSnip overlay and keyboard cancellation.
  - Primary evidence: focused coordinate/lifecycle tests and target-session
    capture observations.
  - Status: verified
  - Evidence: the required keyboard focus retracted fullscreen Yakuake and
    Minecraft with the transparent selector. The user-approved KDE-only frozen
    ScreenShot2 workspace background preserves both applications' content,
    retains the existing selector and Escape cancellation, and was confirmed
    live by the user as correct and substantially faster than Spectacle.
- R3 — One resident GlobalShortcuts portal session activates the five required
  built-in capture actions exactly once and is recreated after settings change.
  - Primary evidence: focused state/mapping tests and target-session activation
    counts.
  - Status: in_progress
  - Evidence: the serial Create/List/Bind/Close manager, stable five-ID mapping,
    safe preferred-trigger conversion, and enabled/dirty recreation lifecycle
    are implemented. Runtime tracing proved KGlobalAccel -> portal Activated ->
    KSnip -> ScreenShot2. The candidate registers all five IDs and exposes
    portal v2 `ConfigureShortcuts`; after assignment in KDE, the user's three
    required bindings work globally and persist across a clean KSnip restart.
- R4 — The final candidate installs safely below `~/.local` with stable desktop
  identity, absolute `Exec`, restricted-interface metadata, and reversible
  uninstall behavior.
  - Primary evidence: installer sentinel round-trip and installed launch.
  - Status: verified
  - Evidence: shell/desktop/checksum gates, a temporary-HOME replacement and
    backup/restore sentinel round-trip, and installation of the byte-identical
    `dist/ksnip` candidate under `/home/stfu/.local` pass. Desktop launch,
    process identity, clean restart, native capture, and shortcut persistence
    were verified in the target session.
- R5 — The immutable installed candidate passes mandatory build, capture,
  shortcut, responsiveness, repetition, and fallback checks, and `dist/`
  contains every required artifact.
  - Primary evidence: `test-report.md`, artifact inventory, and SHA-256 check.
  - Status: pending

### Constraints

- Keep `forceGenericWayland` authoritative and preserve X11, Windows, macOS,
  GNOME, generic Wayland, and existing editor/action flows.
- No GUI-thread blocking, sleeps, busy-waits, raw shortcut emulation, security
  bypass, `sudo`, or system-file changes.
- ScreenShot2 pipe ownership, timeouts, metadata validation, cancellation, and
  destruction safety must follow `03_MINIMAL_DESIGN.md`.
- Update this state only after a completed checkpoint, material decision, or
  concrete blocker.

### Non-goals

- New capture enums, backend registry, DI rewrite, plugin framework, PipeWire,
  ScreenCast, compositor commands, custom-action shortcuts, packaging formats,
  or broad mixed-DPI/settings redesign.
- Optional LastRectArea and unrelated cleanup.

## Change envelope

- Target: KDE Wayland image-backend composition, ScreenShot2 transport,
  existing Wayland selector integration, high-level global-hotkey handling,
  user-local installation, and delivery evidence.
- Expected paths: `src/backend/imageGrabber/`,
  `src/dependencyInjector/DependencyInjectorBootstrapper.cpp`,
  `src/gui/snippingArea/`, `src/gui/globalHotKeys/`, Wayland config/settings,
  `src/main.cpp`, CMake source/test lists, focused tests, `desktop/`, `dist/`,
  and this context pack.
- Direct consumers: `MainWindow`, `AbstractImageGrabber`,
  `AbstractRectAreaImageGrabber`, `GlobalHotKeyHandler`, and installed desktop
  launchers.
- Allowed artifacts: one small ScreenShot2 helper, one small Wayland portal
  shortcut manager, focused tests, mandatory delivery files, and one local
  checksum-only installer ownership marker required for reversible uninstall.
- Allowed RectArea exception: KDE native RectArea may take one ScreenShot2
  workspace image before activating the existing selector and crop that frozen
  image locally. This exception is limited to preserving focus-sensitive
  fullscreen content and does not change generic Wayland or GNOME behavior.
- Forbidden expansion: new dependency, service, daemon, persistent state,
  generic transport framework, public API redesign, or unrelated refactor.

## Baseline

- Repository: `/home/stfu/ai/trash-can/ksnip`, branch `master` ahead of
  `origin/master` by one commit.
- Baseline SHA: `62fa0ff6ec888125ce6dd592b5fb346658160ac5`
- Initial dirty state: untracked `ksnip-wayland-agent-context-pack/` only;
  both library submodules were uninitialized and are now at their pinned SHAs.
- Target host: Fedora 44 x86_64, KDE Plasma 6.7.3, Wayland (`wayland-0`).
- Qt version: 6.11.1 runtime and development packages.
- ScreenShot2 API version: 5.
- GlobalShortcuts portal version: 2.

## Current checkpoint

- Phase: T05 — Residual acceptance evidence
- Closes: R1, R3, and R5
- Smallest next action: retain the user-approved installed candidate; run the
  remaining selected-window, forced-fallback, repetition, and quantitative
  latency checks only if full context-pack closure resumes.
- Expected evidence: the remaining mandatory observations recorded in
  `test-report.md`, without changing the accepted capture implementation.
- Stop or replan if: a remaining check reproduces a failure in an explicit
  required outcome; do not add speculative hardening or unrelated cleanup.

## Completed

- [x] Baseline build
- [x] D-Bus probes
- [x] ScreenShot2 core (automated evidence; live verification pending)
- [x] Rectangular area (automated and live evidence)
- [x] Wayland global shortcuts (automated and three user-required live bindings)
- [x] User-local installer (automated and live launch/restart evidence)
- [ ] Acceptance tests
- [x] `dist/` delivery (live report pending)

## Current evidence

```text
git rev-parse HEAD
62fa0ff6ec888125ce6dd592b5fb346658160ac5

git status --short --branch
## master...origin/master [ahead 1]
?? ksnip-wayland-agent-context-pack/

git submodule update --init --recursive
libraries/kColorPicker: 2781a262b6ae76ec2e9a2d86b3ab8892a90f06aa
libraries/kImageAnnotator: 7eedc8975fe447761fee7c2ec591e7024ee57520

bash ksnip-wayland-agent-context-pack/scripts/probe-wayland.sh
XDG_SESSION_TYPE=wayland; XDG_CURRENT_DESKTOP=KDE; WAYLAND_DISPLAY=wayland-0
org.kde.KWin.ScreenShot2 Version: u 5
org.freedesktop.portal.GlobalShortcuts version: u 2

rpm -q extra-cmake-modules qt6-qtbase-devel qt6-qtbase-private-devel \
  qt6-qtsvg-devel qt6-qttools-devel gtest-devel
All six packages installed at Fedora 44 versions.

cmake -S . -B build-agent -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBUILD_WITH_QT6=ON -DUSE_SUBMODULE_KCOLORPICKER=ON \
  -DUSE_SUBMODULE_KIMAGEANNOTATOR=ON
FAILED at find_package(ECM): ECMConfig.cmake was not found.

After installing the packages and requesting Qt6 GuiPrivate explicitly:
cmake --build build-agent --parallel && build-agent/src/ksnip --version
PASS; binary: build-agent/src/ksnip; Version: 1.11.0.

CONDA_PREFIX= cmake ... -DGTest_DIR=/usr/lib64/cmake/GTest
System GTest selected; test build now fails because `gmock/gmock.h` is absent.
`dnf provides '*/gmock/gmock.h'` => gmock-devel-1.17.0-2.fc44.x86_64.

After installing gmock-devel:
CONDA_PREFIX= cmake -S . -B build-tests ... \
  -DGTest_DIR=/usr/lib64/cmake/GTest
cmake --build build-tests --parallel
ctest --test-dir build-tests --output-on-failure
PASS: 11/11 tests, 0 failures.

cmake --build build-tests --target KWinScreenShot2ClientTests --parallel
ctest --test-dir build-tests -R '^KWinScreenShot2ClientTests$' --output-on-failure
PASS: 1/1 focused transport test executable.

cmake --build build-agent --parallel
cmake --build build-tests --parallel
ctest --test-dir build-tests --output-on-failure
PASS: Qt 6 application build; 12/12 tests, 0 failures.

After T02 RectArea integration:
cmake --build build-agent --parallel
cmake --build build-tests --parallel
ctest --test-dir build-tests --output-on-failure
git diff --check
PASS: Qt 6 application build; 12/12 tests; clean diff check.

After T03 GlobalShortcuts integration:
cmake --build build-tests --target WaylandGlobalShortcutManagerTests --parallel
ctest --test-dir build-tests -R '^WaylandGlobalShortcutManagerTests$' --output-on-failure
cmake --build build-agent --parallel
cmake --build build-tests --parallel
ctest --test-dir build-tests --output-on-failure
git diff --check
PASS: focused trigger/D-Bus signature tests; Qt 6 application build; 13/13
tests; clean diff check.

T04/T05 delivery staging:
bash ksnip-wayland-agent-context-pack/scripts/build-release.sh
sha256sum --check dist/SHA256SUMS
desktop-file-validate dist/org.ksnip.ksnip.desktop
bash -n dist/install-user.sh dist/uninstall-user.sh
PASS: exactly eight required dist files; RelWithDebInfo Qt 6 x86-64 candidate;
all hashes and metadata checks pass.

Temporary-HOME sentinel round-trip:
PASS: unconfirmed desktop replacement refused; confirmed install and reinstall;
uninstall restored the pre-existing desktop and preserved an unrelated file.

dist/install-user.sh
cmp dist/ksnip ~/.local/libexec/ksnip-wayland/ksnip
desktop-file-validate ~/.local/share/applications/org.ksnip.ksnip.desktop
PASS: exact candidate installed with absolute Exec and matching CLI symlink.
```

## Blockers

R3 requires user authorization: the live KSnip portal session and downstream
activation path work, but KDE persisted the three existing actions with active
binding `none` and only default suggestions. The standard portal cannot assign
physical keys silently. The newly installed candidate must be restarted and its
explicit Configure Global Shortcuts action completed by the user.

## Material decisions

- 2026-07-26: This existing context pack and `STATE.md` are the durable goal;
  do not create a competing `docs/goals/` document.
- 2026-07-26: Runtime capability probing is folded into the ScreenShot2-capable
  backend so bootstrap remains non-blocking and fallback is atomic with R1.
- 2026-07-26: Preserve `WaylandSnippingArea::selectedRectArea()` for GNOME;
  expose global logical selection only to the KDE native path.
- 2026-07-26: The reversible installer may keep one checksum-only ownership
  state file inside its dedicated `~/.local/libexec/ksnip-wayland` directory;
  this is the minimum state needed to avoid deleting or overwriting modified
  user files.
- 2026-07-26: Recover empty KDE shortcut assignments only through an explicit
  portal v2 `ConfigureShortcuts` user action. Do not parse localized trigger
  descriptions, auto-open configuration, or mutate KGlobalAccel directly.
- 2026-07-26: User chose a frozen ScreenShot2 workspace image for KDE RectArea
  after live evidence showed that keyboard focus necessarily retracts Yakuake
  and minimizes fullscreen Minecraft. This narrowly supersedes the original
  no-workspace-crop constraint for KDE RectArea only; generic/GNOME paths and
  the existing selector remain unchanged.

## Checkpoint history

- 2026-07-26: RECON recorded exact git/session/API state. T00 remains in
  progress because required development packages are absent.
- 2026-07-26: Initialized both pinned submodules. Unchanged configure failed at
  `find_package(ECM)`; no approved in-scope build action remains before package
  installation.
- 2026-07-26: Release build and `--version` passed after a one-line build-only
  fix requesting the already-linked Qt6 GuiPrivate component. Test build first
  exposed Conda GTest contamination, then with system GTest exposed the missing
  Fedora `gmock-devel` package.
- 2026-07-26: R0 verified. Release build produced `build-agent/src/ksnip`
  (`Version: 1.11.0`); system-GTest debug build passed all 11 registered tests.
  Root `enable_testing()` was required for the documented CTest command.
- 2026-07-26: T01 implementation replaced the legacy KWin endpoint and polling
  reader with an asynchronous ScreenShot2 v3+ probe/client, finite D-Bus and
  worker read deadlines, strict raw metadata handling, and the existing portal
  fallback. Focused transport tests and the full 12-test suite pass; R1 awaits
  target-session observations. Advanced to T02 implementation.
- 2026-07-26: T02 reuses `WaylandSnippingArea` without changing its GNOME-facing
  scaled rectangle, adds a KDE-only global logical rectangle, rejects empty
  selections, and calls ScreenShot2 `CaptureArea` after the existing queued
  overlay-close turn. Build and all 12 tests pass; R2 awaits live observation.
- 2026-07-26: T03 adds one serial GlobalShortcuts portal manager at the existing
  high-level handler boundary, binds the complete desired five-ID set at most
  once per session, and closes/recreates it across settings and shutdown. The
  exact `a(sa{sv})` type, safe trigger conversion, release build, and all 13
  tests pass; R3 awaits live activation counts. Advanced to T04.
- 2026-07-26: T04 staged all eight delivery artifacts, passed shell, desktop,
  checksum, replacement-refusal, reinstall, backup/restore, and unrelated-file
  sentinel checks, then installed the exact candidate under `~/.local`. R4
  awaits desktop launch and post-restart native capture. Advanced to T05 user
  live verification.
- 2026-07-26: User live check confirmed FullScreen, RectArea, and ActiveWindow.
  Global shortcuts do not activate and the portal exposes no KSnip session;
  diagnosis of the first failed setup transition is the current checkpoint.
- 2026-07-26: R3 diagnosis found one live portal session and an active KDE
  component. Programmatic `capture.current_screen` produced one portal
  `Activated` and one ScreenShot2 `CaptureActiveScreen`; physical keys failed
  because KGlobalAccel stored active bindings as `none`. Added an explicit
  portal v2 configuration action and retained all five mandatory IDs even when
  no preferred trigger is configured. The first unconstrained concurrent build
  was OOM-killed; sequential `--parallel 2` application/test builds, all 13
  tests, diff check, delivery checksum, and byte-identical user install pass.
- 2026-07-26: After KDE assigned the three user-required physical shortcuts,
  RectArea, CurrentScreen, and ActiveWindow worked globally and survived a clean
  KSnip restart. Fullscreen Yakuake and Minecraft then proved that activating
  the transparent RectArea selector removes focus-sensitive source content.
- 2026-07-26: Implemented the user-approved KDE-only frozen RectArea path: one
  ScreenShot2 workspace image is shown by the existing selector and cropped
  locally after selection. Sequential `--parallel 2` application/test builds
  and `ctest -j1` pass all 13 tests; installed fullscreen retest is next.
- 2026-07-26: User confirmed the installed frozen-background RectArea under
  fullscreen Yakuake and Minecraft, including the requested selector behavior;
  all required workflows now behave correctly and feel substantially faster
  than Spectacle. R2 and R4 are verified; broader T05 stress/fallback evidence
  remains pending.

## Completion

- Resolved outcomes: none.
- Commands and artifacts: automated build/test/install evidence and complete
  `dist/` are present; live evidence is pending.
- Constraint and diff-scope check: automated check passed; final check pending
  live evidence.
- Final status: active at T05; R1-R5 await their target-session observations.

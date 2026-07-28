# Goal: Native KDE Wayland capture, shortcuts, and offline OCR

Status: active
Source: `00_PRIMARY_GOAL.md`, `02_MVP_SCOPE.md`, `03_MINIMAL_DESIGN.md`,
`05_ACCEPTANCE_CRITERIA.md`, and `tasks/T00_BASELINE.md` through
`tasks/T06_OFFLINE_OCR.md`
Last updated: 2026-07-28

## Objective

Deliver a locally installable Qt 6 KSnip for KDE Plasma Wayland that uses KWin
ScreenShot2 for the five required capture modes, uses the GlobalShortcuts
portal for their resident shortcuts, retains the generic Screenshot portal
fallback, and adds the separately approved offline RU/EN RectArea-to-clipboard
OCR workflow.

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
  - Status: in_progress
  - Evidence: the required keyboard focus retracted fullscreen Yakuake and
    Minecraft with the transparent selector. The user-approved KDE-only frozen
    ScreenShot2 workspace background preserves both applications' content,
    retains the existing selector and Escape cancellation, and was confirmed
    live by the user as correct and substantially faster than Spectacle. A
    focused latest-wins lifecycle suite now covers startup bursts, overlapping
    background requests, selector replacement, Escape, and a fresh frozen crop;
    installed-candidate burst verification remains pending.
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
- R6 — One selected in-process recognizer handles fixed RU/EN screenshot text
  offline without runtime downloads or system OCR/inference packages.
  - Acceptance: pinned RU, EN, and mixed fixtures pass the frozen output and
    resource gates in a network-disabled installed environment.
  - Primary evidence: conversion/parity report, focused fixtures, ELF/RPM
    dependency inspection, and isolated runtime observation.
  - Status: in_progress
  - Evidence: conversion, fixture parity, static closure, and production-class
    smoke checks pass without a dynamic OCR dependency or external model file.
    The user-local candidate recognized a live mixed Russian/English screenshot;
    the result also exposed expected model-level homoglyph and monospace-code
    errors that remain part of the quality assessment. The local RPM now
    contains only embedded model data and adds no OCR runtime requirement or
    shared-library dependency. The user confirmed RU/EN OCR from the installed
    RPM initially appeared to succeed with networking fully disabled, but a
    subsequent repeat exposed that the packaged workflow can finish without a
    clipboard write. The corrected RCC build embeds non-zero model resources and
    the user confirmed live recognition from the exact rebuilt RPM; a repeat
    with networking disabled remains pending. A ten-line live RU/EN benchmark
    preserved every line and its order with 6.89% raw CER and 5.75% CER after
    excluding the repeated line-number separator; ordinary prose measured 1.21%
    CER, while mixed-script homoglyphs and escaped code punctuation dominated
    the remaining errors.
- R7 — RectArea OCR writes only non-empty recognized text to the clipboard and
  bypasses the editor, auto-save, and image-clipboard paths.
  - Acceptance: one accepted OCR request produces at most one text write;
    Escape, error, empty output, and repeated activation preserve prior state.
  - Primary evidence: focused workflow tests and target-session observation.
  - Status: verified
  - Evidence: focused routing/concurrency tests cover successful, Escape, empty,
    error, and repeated activation paths. The user installed the exact RPM and
    initially confirmed live OCR, Escape, and repeated-hotkey behavior, but then
    reproduced an installed-RPM run where no recognized text reached the
    clipboard. The root cause was zero-length Qt big resources produced by the
    LTO-compiled two-pass RCC object. With LTO disabled only for that object, the
    user confirmed the exact rebuilt RPM again writes recognized text to the
    clipboard; focused tests retain cancellation, empty, error, and repeat
    coverage.
- R8 — OCR is activated as one built-in shortcut in the existing resident
  GlobalShortcuts session without changing the five capture mappings.
  - Acceptance: the sixth stable ID activates OCR exactly once and settings
    recreation preserves the existing capture actions.
  - Primary evidence: focused mapping/session tests and target activation.
  - Status: verified
  - Evidence: the portal manager now emits stable IDs; the existing five IDs
    retain their exact capture mappings and `ocr.rect_area` emits the dedicated
    OCR workflow signal. One persisted `Alt+Shift+O` preference and settings row
    are included only in built-in OCR builds. Focused mapping, portal, and
    workflow tests pass; the user assigned and successfully activated the OCR
    shortcut in the target session and confirmed it still works after installing
    and restarting the exact RPM candidate.
- R9 — The Qt 6 application and local RPM build include the one selected OCR
  engine and embedded model data and pass affected regression checks.
  - Acceptance: local build/tests pass; installed OCR runs offline with no
    external model files or new OCR shared-library dependency.
  - Primary evidence: build/test commands, package inventory, dependency
    inspection, and installed fixture result.
  - Status: in_progress
  - Evidence: the OCR-enabled Fedora 44 binary RPM and complete SRPM build
    successfully from the immutable source closure. `%check` passes all 18 Qt 6
    tests. Package inventory, `Requires`, `readelf`, and `ldd` show embedded-only
    models and no dynamic OCR dependency. Installation and live recognition
    from this exact RPM pass in the target session. The installed binary is
    byte-identical to the extracted RPM payload and is the running process. The
    user later reproduced a package-only missing clipboard result, invalidating
    final installed acceptance. The resource-target fix was then rebuilt into an
    RPM whose ELF contains each pinned model/dictionary blob exactly once; the
    user confirmed live recognition works. Network-disabled acceptance for this
    corrected package remains pending.

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
- A second production OCR engine, daemon, runtime engine selection/download,
  language UI, document layout reconstruction, GPU acceleration, OCR history,
  or replacement of the existing plugin-based editor OCR.

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
- T06 expansion: one selected static CPU OCR engine and its pinned model data,
  one private recognizer boundary/worker, one built-in shortcut and setting,
  embedded production-only resources, focused tests, and RPM source/license
  inputs are allowed.
- Forbidden expansion: a second production engine, service, daemon, runtime
  download, persistent state, generic transport/action framework, public API
  redesign, or unrelated refactor.

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
- OCR follow-up baseline: clean `master` at
  `9d1c8034` (`feat(saver): add stealth screenshot mode`), matching
  `origin/master`.

## Current checkpoint

- Phase: T06 — corrected installed offline acceptance
- Closes: R6 and R9
- Smallest next action: repeat one successful OCR request from the corrected
  `/usr/bin/ksnip` while networking is disabled.
- Expected evidence: recognized text reaches the clipboard and the runtime log
  contains no resource, inference, subprocess, or download failure.
- Stop or replan if: the corrected package attempts network access or fails only
  when networking is unavailable.

## Completed

- [x] Baseline build
- [x] D-Bus probes
- [x] ScreenShot2 core (automated evidence; live verification pending)
- [x] Rectangular area (automated and live evidence)
- [x] Wayland global shortcuts (automated and three user-required live bindings)
- [x] User-local installer (automated and live launch/restart evidence)
- [ ] Acceptance tests
- [x] `dist/` delivery (live report pending)
- [x] Offline RU/EN OCR engine feasibility gate
- [x] RectArea-to-clipboard vertical slice
- [x] OCR shortcut and settings (automated and live activation evidence)
- [x] OCR-enabled RPM and SRPM immutable source closure
- [ ] Network-disabled installed OCR acceptance
- [ ] Embedded local build and installed acceptance

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
- 2026-07-27: The user approved the audited T06 plan and explicitly authorized
  iterative implementation and local builds. T05 residual live evidence is
  retained honestly but no longer blocks the separately approved OCR follow-up.
  T06 selects one engine through sequential gates, starting with Paddle/ONNX;
  Tesseract is evaluated only after a concrete Paddle blocker.
- 2026-07-27: The reduced static Paddle/ONNX closure is credible enough for
  production integration, so Paddle/ONNX remains the sole selected engine and
  the sequential Tesseract fallback is not activated.

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
- 2026-07-27: Fixed the RectArea burst regression caused by an unbounded KDE
  request queue. The backend now keeps one active request and only the latest
  replacement across delay, ScreenShot2 background capture, and selector
  phases; silent replacement also stops the old selector timeout. The Qt 6
  application build, three focused lifecycle cases, and the full 14-test suite
  pass with `QT_QPA_PLATFORM=offscreen`; live verification of the newly built
  installed candidate remains pending.
- 2026-07-27: T06 Paddle conversion gate passed in ignored
  `build-ocr-spike/`. Python 3.12.12, Paddle 3.2.2, Paddle2ONNX 2.1.0,
  ONNX 1.17.0, and ONNX Runtime 1.27.0 converted both pinned PIR models at
  opset 17 without patches or custom operators. ONNX checker and CPU sessions
  pass. Detector dynamic inputs `128x128` and `256x384`, recognizer widths 160
  and 640 with batch sizes 1 and 2, and the nominal inputs match Paddle with
  `rtol=1e-4, atol=1e-5`; maximum observed absolute difference is
  `3.7252903e-06`. Final ONNX hashes are detector
  `c8d9b07063420ce5365c74e42532de48238feeeedcdb7a330b195708bc38a93f`
  (4,766,440 bytes) and recognizer
  `a966b18ae06292f6df1114183fa69db2fb31ab62d930d73b44b8d1bef0ae77ff`
  (7,882,715 bytes). Advanced to fixture-level parity before any application
  source change.
- 2026-07-27: T06 fixture parity passed on generated English light-theme,
  Russian dark-theme, and mixed multiline screenshot-like PNGs using the pinned
  PaddleOCR preprocessing, DB postprocessing, perspective crops, dictionary,
  and CTC decoder. Paddle and ORT produced identical threshold maps, final
  boxes, CTC argmax IDs, and decoded text; maximum detector-map difference was
  `5.7697296142578125e-05` (`rtol=1e-4`, `atol=1e-4`) and maximum recognizer
  difference was `5.602836608886719e-06`. EN and RU gold text were exact. The
  mixed fixture exposed identical model-level Latin/Cyrillic homoglyph errors
  (`О/O`, `e/е`, `c/с`), which is a quality-corpus input rather than a
  conversion mismatch. Advanced to the standalone static CPU closure.
- 2026-07-27: T06 standalone static CPU closure passed. A C++17 harness linked
  reduced-operator non-minimal ONNX Runtime 1.27.0, OpenCV 4.12.0 core/imgproc,
  and Clipper 6.4.2 statically, loaded both models and the exact dictionary from
  embedded read-only bytes, and reproduced all three frozen fixture outputs.
  Thirty repeats passed with session initialization `189.397 ms`, fixture p50
  `120.489 ms`, p95 `184.153 ms`, and peak RSS `186148 KiB` on the current host.
  The stripped ELF is `25,163,752` bytes. `readelf`/`ldd` show only zlib and the
  normal C/C++ runtime libraries; the link map contains only OpenCV core and
  imgproc and no codec or shared ORT provider. `strace -f` recorded one harness
  `execve`, the three expected PPM opens, and no model/dictionary file access,
  subprocess, or network attempt. Advanced to the fake capture-to-clipboard
  vertical slice before linking the engine into KSnip.
- 2026-07-27: T06 fake capture-to-clipboard vertical slice passed. A private
  `OcrCaptureWorkflow` now owns the OCR purpose at the shared grabber result
  boundary, passes ordinary captures and cancellations to the unchanged editor
  pipeline, deep-copies accepted OCR pixels on the GUI thread, and runs an
  injected recognizer through `QtConcurrent`. It writes normalized non-empty
  text exactly once and suppresses editor/action/image-clipboard handling for
  OCR cancellation, empty output, and errors. `OcrCaptureWorkflowTests` passes
  all eight fake-recognizer routing/concurrency cases under offscreen Qt; the
  real Qt 6 `ksnip` target builds successfully with `--parallel 2`. Advanced to
  the sole selected Paddle recognizer and production-only embedded resources.
- 2026-07-27: T06 production Paddle recognizer integration passed locally. The
  production-only `PaddleOcrRecognizer` ports the proven static pipeline, lazily
  loads pinned detector/recognizer/dictionary bytes from an uncompressed Qt big
  resource, and serializes inference off the GUI thread. CMake verifies exact
  model, dictionary, and Clipper hashes and links the reduced static ORT,
  OpenCV core/imgproc, and Clipper closure only into `ksnip`; large resources
  remain outside `KSNIP_SRCS` and the shared test archive. A focused ignored
  production-class smoke executable reproduced all three frozen fixture texts.
  `cmake --build build-agent --target ksnip --parallel 2` passed, and
  `readelf`/`ldd` show no dynamic OCR, ORT, OpenCV, Paddle, or Tesseract
  dependency. Advanced to the sixth built-in shortcut and persisted setting.
- 2026-07-27: T06 built-in shortcut integration passed automated gates. The
  existing portal manager now reports stable IDs, `GlobalHotKeyHandler` maps
  the five capture IDs unchanged and emits a dedicated signal for
  `ocr.rect_area`, and OCR-enabled builds expose one persisted
  `Alt+Shift+O` preference and settings row. OCR starts only when no ordinary
  capture is pending and reuses the existing single-flight workflow. Focused
  `GlobalHotKeyHandlerTests`, `WaylandGlobalShortcutManagerTests`, and
  `OcrCaptureWorkflowTests` pass 3/3 under offscreen Qt; the OCR-enabled
  `ksnip` target builds successfully with `--parallel 2`, and `git diff
  --check` passes. Advanced to the user-local installed/live gate.
- 2026-07-27: The user installed the stripped OCR-enabled candidate under the
  user-local desktop identity, assigned the built-in OCR shortcut, and confirmed
  the target workflow captures an area and places mixed Russian/English text in
  the clipboard. The live sample showed model-level Latin/Cyrillic homoglyph
  substitutions (`уже` -> `ужe`, `тестов` -> `теcтов`, `OCR` -> `0CR`) plus
  monospace code/punctuation errors (`libexec` -> `iibexec` and lost shell
  punctuation). These match the known mixed-fixture model limitation rather
  than a Paddle-to-ONNX or C++ parity difference. Advanced to immutable RPM
  source and offline package closure; no heuristic text rewriting was added.
- 2026-07-28: T06 immutable RPM closure passed locally. The first full
  `rpmbuild -ba` validation attempt linked test GTest/GMock from the active
  Miniconda environment into the otherwise system-Qt build and failed with a
  PCRE2 ABI mismatch. Repeating the same build with Conda variables removed and
  a system-only `PATH` required no source change and succeeded. `%check` passed
  18/18 tests. The binary RPM is `5,703,052` bytes with SHA-256
  `55ffe84d89934e28c585c1a802090553c39c47c580536a786c56b1354c9bdf2c`;
  its installed size is `34,282,648` bytes. The complete SRPM is `425,131,179`
  bytes with SHA-256
  `68e742de76c474691a6b9b0d6f683a819c8cd571704956973822e1e39684a43e` and
  contains Source0 plus all 13 hash-locked dependency archives. Package payload
  inspection found no external model/dictionary file, RPM `Requires` and ELF
  `DT_NEEDED` contain no ORT/OpenCV/Paddle/Tesseract dependency, no RPATH is
  present, and the packaged executable exposes the OCR settings text. Advanced
  to installation and target-session acceptance of this exact RPM; it remains
  a local artifact named from the uncommitted `9d1c8034` baseline.
- 2026-07-28: The user installed the exact local RPM and confirmed OCR, Escape,
  repeated activation, and restart behavior work in the target KDE Wayland
  session. `rpm -q` reports the expected NEVRA, the running executable resolves
  to `/usr/bin/ksnip`, and no user-local desktop override remains. Installed
  `/usr/bin/ksnip` is byte-identical to the extracted RPM payload with SHA-256
  `08094e19e104bdd5c245efda7cc539999689da576f01f3088a399a6b1cf37c78`.
  R7 and R8 are verified; advanced to the final network-disabled installed OCR
  check for R6 and R9.
- 2026-07-28: The user confirmed the installed RPM continues to recognize text
  with networking fully disabled. Together with the embedded-only package
  payload, dependency audit, exact installed-binary identity, and prior fixture
  parity/runtime traces, this verifies R6 and R9 and closes T06.
- 2026-07-28: The user subsequently reproduced an installed-RPM OCR run that
  completed without writing text to the clipboard while the local candidate
  remained working. This later evidence invalidates the installed acceptance
  above and reopens R6, R7, and R9. Package diagnostics found that RPM LTO flags
  leaked into nested ORT/OpenCV static builds and the final link reported an ORT
  `Ort::Exception` ODR warning; the next package rebuild isolates those
  dependencies from LTO and logs recognizer exceptions without OCR text.
- 2026-07-28: The corrected dependency closure removed the ODR warning, but the
  exact installed RPM logged `Missing embedded OCR resource`. A minimal Qt probe
  reproduced the package flags: `qt6_add_big_resources` pass 1 compiled with
  GCC LTO registered zero-length resources, while adding `-fno-lto` only to
  `rcc_object_OcrResources` exposed the exact detector, recognizer, and dictionary
  sizes. The production CMake now applies that narrow resource-target fix.
- 2026-07-28: The rebuilt RPM retained Fedora LTO for KSnip while compiling only
  `rcc_object_OcrResources` with the trailing `-fno-lto`; all 18 tests passed,
  the ELF contained each exact model/dictionary blob once, and package/ELF
  dependency checks remained clean. The user reinstalled that exact package and
  confirmed RectArea OCR again writes text to the clipboard, verifying R7.
- 2026-07-28: A user-run ten-line benchmark retained 10/10 lines in order. Raw
  character edit distance was 37/537 (6.89% CER); excluding the synthetic `NN |`
  prefix gave 28/487 (5.75%), and the prose subset gave 2/165 (1.21%). Main
  failures were Latin/Cyrillic homoglyphs, `O/0`, `|/I/І`, escaped `\\n`, smart
  quotes, and a trailing underscore; no heuristic substitutions were added.
- 2026-07-28: A bounded terminal-style A/B rendered the same ten-line RU/EN/code
  sample at 16, 20, and 24 px on a 1920x1080 dark Noto Sans Mono fixture. Raising
  detector long-side input from 960 to 1536 or 1920 changed mean CER from 5.22%
  to 5.28% and 5.03%, while mean detector time rose from 259 ms to 484 ms and
  838 ms. At the existing 960 input, recognizer resize filters measured 5.22%
  CER for linear, 5.46% for cubic, and 5.71% for Lanczos. Neither candidate is a
  stable accuracy win, so the production preprocessing remains unchanged and no
  terminal-specific heuristic was added.

## Completion

- Resolved outcomes: R7 and R8 are verified; R6 and R9 await only the corrected
  package's network-disabled runtime check.
- Commands and artifacts: model conversion/parity reports, static and production
  fixture smoke checks, focused and full Qt 6 tests, complete binary RPM/SRPM,
  package/ELF dependency inspection, exact installed-payload identity, and live
  corrected-package recognition are recorded above.
- Constraint and diff-scope check: the prior package closure remains valid, but
  final target-session behavior is not currently accepted; no second OCR engine,
  external model, runtime downloader, service, or OCR shared dependency was
  added.
- Final status: T06 active until the corrected exact RPM repeats successful OCR
  with networking disabled.

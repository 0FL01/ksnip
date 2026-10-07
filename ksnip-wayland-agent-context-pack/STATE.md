# Goal: Native KDE Wayland capture, shortcuts, and offline OCR

Status: active
Source: `00_PRIMARY_GOAL.md`, `02_MVP_SCOPE.md`, `03_MINIMAL_DESIGN.md`,
`05_ACCEPTANCE_CRITERIA.md`, and `tasks/T00_BASELINE.md` through
`tasks/T06_OFFLINE_OCR.md`
Last updated: 2026-10-07

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
    installed-candidate burst verification remains pending. A later live
    `2560x1600` at 175% regression showed that ScreenShot2 returned a
    `1463x914`, DPR 1.0 frozen background while the primary screen DPR was 2.0;
    the old crop doubled the logical selection and retained only its intersection
    with the bottom edge. Mapping the selector canvas directly to the actual
    background extents fixed the normal RectArea result live, and a focused
    regression now covers the mismatched-DPR case. The user also confirmed that
    OCR receives and recognizes the same corrected title/description region.
    Runtime logs later exposed intermittent incomplete `CaptureWorkspace` pipe
    payloads that canceled before the selector appeared. The backend now retains
    frozen capture as the preferred path and deterministically falls back to the
    same live selector followed by queued `CaptureArea`; focused failure,
    completion, Escape, coordinate, and latest-wins tests pass. Runtime evidence
    after display reconfiguration then exposed a black/flickering fallback
    selector when its top-level translucent attribute was changed after a prior
    frozen selection. The Wayland selector now enables its alpha surface before
    first show and retains it across frozen and live modes; automated and RPM
    checks pass, while installed live acceptance remains pending.
- R3 — One resident GlobalShortcuts portal session activates the five required
  built-in capture actions exactly once and is recreated after settings change.
  - Primary evidence: focused state/mapping tests and target-session activation
    counts.
  - Status: in_progress
  - Evidence: the serial Create/Bind/Close manager, stable five-ID mapping,
    safe preferred-trigger conversion, and enabled/dirty recreation lifecycle
    are implemented. Runtime tracing proved KGlobalAccel -> portal Activated ->
    KSnip -> ScreenShot2. The candidate registers all five IDs and exposes
    portal v2 `ConfigureShortcuts`; after assignment in KDE, the user's three
    required bindings previously worked globally across a clean KSnip restart.
    KDE 6.7.4+ exposed the old List-equality path skipping Bind on new sessions.
    Direct full-list binding now passes the private-bus regression; physical
    activation after restart/reboot of the new exact RPM remains pending.
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
  - Acceptance: the sixth stable ID activates OCR exactly once after start,
    clean restart and reboot, and settings recreation preserves capture actions.
  - Primary evidence: focused mapping/session tests and target activation.
  - Status: in_progress
  - Evidence: the portal manager now emits stable IDs; the existing five IDs
    retain their exact capture mappings and `ocr.rect_area` emits the dedicated
    OCR workflow signal. One persisted `Alt+Shift+O` preference and settings row
    are included only in built-in OCR builds. Focused mapping, portal, and
    workflow tests pass; the user assigned and successfully activated the OCR
    shortcut in the target session and confirmed it still works after installing
    and restarting an earlier exact RPM candidate. The subsequent ignored
    Meta+Shift+D report invalidates current restart/reboot acceptance. New
    private-bus tests prove fresh-session binding and OCR-ID delivery, not a
    physical keypress or real inference on the installed candidate.
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

- Phase: shortcut and transport commits pushed; first full OCR RPM passes binary
  gates, but source-closure inspection found an unlisted ORT JSON archive.
- Smallest next action: commit the pinned JSON lock/spec correction, reseal source,
  and rebuild in a separate topdir. Require all 12 ORT population receipts to use
  local SRPM archives; the first candidate is not the final delivery.
- Preserve the committed ScreenShot2 socketpair mitigation and diagnostics. No
  capture-only diagnostic is to be deployed under the main desktop identity.
- Installed baseline from preflight: `1:ksnip-1.11.0-1.2876.gccf15f2a.dirty.fc44.x86_64`;
  no main user-local desktop override was present. The old artifact directory is
  absent; its dated build evidence below remains historical.
- Expected live evidence after user installation: saved Meta+Shift+D produces
  current-session Activated, one selector, and expected real fixture text replacing
  a clipboard sentinel after start, one clean restart and one actual reboot,
  without reassignment, manual backend Bind or clearing configuration.
- This implementation does not establish physical-key delivery, real OCR inference,
  or sleep/display-state transport acceptance. No host installation, resident
  termination, UI opening or user-bus portal mutation was performed.

## Completed

- [x] Baseline build
- [x] D-Bus probes
- [x] ScreenShot2 core (automated evidence; live verification pending)
- [x] Rectangular area (automated and live evidence)
- [x] Wayland global shortcuts (automated; historical target-session evidence)
- [ ] Saved capture/OCR activation after restart and reboot of the new exact RPM
- [x] User-local installer (automated and live launch/restart evidence)
- [ ] Acceptance tests
- [x] `dist/` delivery (live report pending)
- [x] Offline RU/EN OCR engine feasibility gate
- [x] RectArea-to-clipboard vertical slice
- [x] OCR shortcut and settings (automated; historical live activation evidence)
- [ ] OCR-enabled RPM and SRPM complete source closure (JSON omission found)
- [x] Fractional-scale normal RectArea crop (automated and live local evidence)
- [x] Fractional-scale OCR crop live acceptance
- [x] Incomplete-background live-selector fallback (automated evidence)
- [ ] Incomplete-background fallback installed live acceptance
- [x] Wayland fallback alpha-surface lifecycle (automated evidence)
- [ ] Wayland fallback alpha-surface installed live acceptance
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

No implementation blocker is known. The earlier `none`-binding observation is
historical, not the current diagnosis: saved OCR metadata does not prove delivery.
Target-session acceptance requires the user to install the exact new RPM, confirm
physical keypresses, and perform a real reboot; no automated key injection or
manual backend binding substitutes for that evidence.

The only missing RPM BuildRequires in preflight is `python3-flatbuffers`.
The declared requirement stays intact. A documented local `--nodeps` exception is
permitted only after checking every other final-spec requirement and using the
existing Miniconda Python for ORT generation, not the compiler/Qt toolchain.

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
- 2026-08-23: KDE frozen RectArea must map the selector's local logical canvas
  directly to the actual ScreenShot2 background dimensions. It must not apply
  `primaryScreen()->devicePixelRatio()`: live evidence showed primary DPR 2.0,
  widget DPR 1.75, and a logical `1463x914`, DPR 1.0 frozen background.
- 2026-08-24: A failed KDE frozen background is recoverable rather than user
  cancellation. Preserve the focus-safe frozen path normally; on technical
  failure only, use the existing live selector and queued ScreenShot2
  `CaptureArea`. Never accept a partial image, retry the frozen transfer, or
  invoke the portal picker.
- 2026-08-31: The Wayland selector must create its translucent top-level surface
  before first show and retain it across frozen and live modes. The frozen pixmap
  still paints opaque pixels; non-Wayland selectors retain their existing
  attribute behavior. Do not toggle `WA_TranslucentBackground` on a reused shown
  Wayland widget because Qt does not uniformly support that transition.

## Checkpoint history

- 2026-10-07: Committed and pushed `fac6098d` (transport) and `e93f80e5`
  (fresh-session shortcut Bind) to `origin/master`. Full OCR Qt6 RPM `%check`
  passes 18/18, with 18 passing manager QtTest entries and no failures/skips;
  package resources, desktop identity, dependencies and socketpair gates pass.
  Source0 contains the clean source commit `e93f80e5551911ecf2dab6974c8f4a08053de17b`
  (count 2878) and both pinned submodules; the declared 15 SRPM files byte-match.

  A stronger source audit invalidated complete offline source closure:
  ```text
  /usr/bin/python3 -I -B build-ocr-shortcuts-rpm/verify-local-mirror.py
  FAIL, exit 1: 11/12 ORT receipts use locked local archives; nlohmann_json
  uses https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.zip,
  absent from the old lockfile and SRPM.
  Retained archive bytes: 8489998
  SHA256: 04022b05d806eb5ff73023c280b68697d12b93e1b7267a0b22a1a39ec7578069
  SHA1: 5e88795165cc8590138d1f47ce94ee567b85b4d6
  ```
  The SHA1 exactly matches pinned ORT 1.27.0 `cmake/deps.txt:32`. Added the
  SHA256-locked archive and Source14; the existing mirror builder already consumes
  every lock entry. JSON's MIT attribution is already in the installed ORT
  third-party notices. No engine/model/version change or mirror framework is needed.
  Complete rebuilt SRPM must now contain 16 inputs: spec plus Source0 through
  Source14. Preserve the first candidate/logs as failed-closure evidence, not final
  delivery. The current resident process and installed RPM remain unchanged.

- 2026-10-07: Implemented the audited minimal shortcut lifecycle correction:
  every successful new CreateSession dispatches one BindShortcuts containing
  the complete actual desired list. Removed List flow and its unused desired-ID
  set; retained Bind result handling, subset/typed-empty semantics, configuration
  queue, generation guards, cancellation and session/request closing.

  The isolated public-frontend fake uses real QtDBus on `dbus-run-session`, six
  explicit fixture IDs including OCR, saved metadata, controllable responses,
  bound-gated simulated presses and independent raw signals for filtering.
  The fake refuses registration without the CTest private-bus marker/address;
  only this Linux test is wrapped. `dbus-daemon` is a build/test dependency only.
  Manager tests do not exercise physical key delivery or the production recognizer.

  Exact commands and evidence:
  ```text
  env -u CONDA_PREFIX -u CONDA_DEFAULT_ENV -u CMAKE_PREFIX_PATH -u LD_LIBRARY_PATH PATH=/usr/bin:/bin cmake -S . -B build-tests -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_WITH_QT6=ON -DBUILD_TESTS=ON -DENABLE_BUILTIN_OCR=OFF -DUSE_SUBMODULE_KCOLORPICKER=ON -DUSE_SUBMODULE_KIMAGEANNOTATOR=ON -DGTest_DIR=/usr/lib64/cmake/GTest
  PASS. Test-only OCR=OFF build is not installed or delivered.
  cmake --build build-tests --target WaylandGlobalShortcutManagerTests --parallel 2
  PASS after resuming the initial tool-timeout build.
  ctest --test-dir build-tests -R '^WaylandGlobalShortcutManagerTests$' --output-on-failure
  RED before production change: 5 QtTest checks passed, 13 failed at missing Bind;
  no fake registration or wire error. Saved six IDs must not activate a new session.
  env -u CONDA_PREFIX -u CONDA_DEFAULT_ENV -u CMAKE_PREFIX_PATH -u LD_LIBRARY_PATH PATH=/usr/bin:/bin cmake --build build-tests --parallel 2
  PASS (existing Qt deprecation warnings only).
  QT_QPA_PLATFORM=offscreen /usr/bin/ctest --test-dir build-tests --output-on-failure -j2
  GREEN after production change: 18/18 CTest executables, 0 failures, 1.96 s.
  QT_QPA_PLATFORM=offscreen /usr/bin/ctest --test-dir build-tests -R '^(WaylandGlobalShortcutManagerTests|KWinScreenShot2ClientTests|KdeWaylandImageGrabberTests|GlobalHotKeyHandlerTests|OcrCaptureWorkflowTests)$' --output-on-failure
  PASS: 5/5 after adding one-Bind fake assertions and teardown draining, 3.06 s.
  /usr/bin/ctest --test-dir build-tests -R '^WaylandGlobalShortcutManagerTests$' --repeat until-fail:10 --output-on-failure
  PASS: all ten isolated repetitions, 23.08 s.
  git diff --check
  PASS. Independent read-only implementation review found no concrete blocker.
  ```
  Full OCR RPM build and exact-package inspection follow source commits. Real
  physical activation after reboot, installed inference and transport stress
  remain separately pending; neither saved IDs nor green fake tests close them.

- 2026-09-14: Built one Fedora 44 x86_64 RPM/SRPM combining the full static OCR
  closure with the current uncommitted ScreenShot2 socketpair mitigation and
  diagnostics. The new ignored topdir is `build-ocr-socket-rpm`; no prior artifact
  was overwritten. Source0 was sealed before this checkpoint entry from tracked
  `HEAD` `ccf15f2abe48dc2fd5a36c5c7eda7b991ade1f7b` (count 2876), the exact initial
  five-file working-tree diff, and both pinned submodule archives. Builds, caches,
  other untracked files, and Git metadata are excluded. The package release and
  CLI build string carry `.dirty`/`dirty-tracked`, and the RPM description records
  the pre-commit base and included diff.

  Source closure:
  ```text
  Source0 SHA-256:
  e0c1edda1d88193dd9a95dae43b4543b5e9beba52040181e08759d6af7384c3c
  Initial tracked working-tree patch SHA-256:
  1eac26f327139416db3b4a97ef8185bbeda75eabcc0cc45b667a98abd8bde846
  Spec SHA-256:
  2da5fd701cbfa320b396f699806adfc7478a54a6a8b6044f832249187831d675
  bash .../fetch-ocr-sources.sh .../SOURCES --offline
  PASS: all 13 immutable archive hashes match ocr-sources.lock; no network used.
  SRPM extraction and cmp against the topdir:
  PASS: spec plus Source0 through Source13 are byte-identical (15 files total).
  Source0 contains both submodule CMakeLists and the socketpair source/tests.
  ```

  Exact successful build environment and command, with the same command first
  interrupted only by tool limits at 20 and 40 minutes and resumed through the
  private ccache:
  ```text
  env -u CONDA_PREFIX -u CMAKE_PREFIX_PATH -u PYTHONPATH -u PYTHONHOME \
    PATH=/usr/lib64/ccache:/usr/bin:/bin \
    CCACHE_DIR=/home/stfu/ai/trash-can/ksnip/build-ocr-socket-rpm/ccache \
    KSNIP_OCR_PYTHON=/home/stfu/miniconda3/bin/python \
    SOURCE_DATE_EPOCH=1788179999 \
    rpmbuild -ba --nodeps \
      --define "_topdir /home/stfu/ai/trash-can/ksnip/build-ocr-socket-rpm" \
      --define "_smp_build_ncpus 2" \
      build-ocr-socket-rpm/SPECS/ksnip.spec
  PASS. Logs: rpmbuild-attempt1-timeout.log, rpmbuild-attempt2-timeout.log,
  and rpmbuild.log. The final ccache total was 3925/7617 cacheable hits.
  ```
  `--nodeps` was required only because declared `python3-flatbuffers` is not
  installed. As in the prior successful RPM, existing Miniconda Python 3.13.5
  with flatbuffers 25.12.19 was selected only for ORT's generated flatbuffers;
  GCC 16.1.1, CMake 4.3.0, Qt 6.11.1, GTest/GMock, and every compiler/linker path
  remained the Fedora system toolchain. The log shows `ENABLE_BUILTIN_OCR=ON`,
  Qt 6, static ORT/OpenCV inputs, and `-fno-lto` only on the RCC resource object.

  Build and package gates:
  ```text
  RPM %check: PASS, 18/18, 0 failures, 1.11 s.
  desktop-file-validate: PASS.
  appstreamcli validate --no-net: PASS with three pre-existing informational notes.
  Focused KWinScreenShot2Client, KdeWaylandImageGrabber, GlobalHotKeyHandler,
  WaylandGlobalShortcutManager, and OcrCaptureWorkflow rerun: PASS, 5/5.
  rpm -K / rpm --checksig -v: header and payload SHA-256 digests OK for both RPMs.
  Binary RPM SHA-256:
  6faebab50931d3f473834b934fd217552ac3d1bcbda335ebb0dfa0bb60fc62f5
  SRPM SHA-256:
  46f83fb24255e472d61eb6c4e7aaf53119324ebec35d3106bb86e1e9eace9f6e
  Extracted binary SHA-256:
  061928bf700e73eac4fbe367d33928d235befa23bc0de88c84916ea1be3423b6
  NEVRA: 1:ksnip-1.11.0-1.2876.gccf15f2a.dirty.fc44.x86_64
  RPM bytes: 18066566; installed size: 36582872; SRPM bytes: 425171024.
  QT_QPA_PLATFORM=offscreen extracted/usr/bin/ksnip --version
  Version: 1.11.0-2876.gccf15f2a.dirty
  Build: ccf15f2abe48dc2fd5a36c5c7eda7b991ade1f7b-dirty-tracked
  ```

  Payload and ELF gates:
  ```text
  Desktop Exec=/usr/bin/ksnip %F and restricted interfaces include
  org.kde.KWin.ScreenShot2; extracted desktop validation passes.
  Strings expose Recognize Text in Area, Recognize text in area, ocr.rect_area,
  all three :/ocr resource paths, and the missing-resource diagnostic.
  Exact embedded occurrences: detector 4766440 bytes (1), recognizer 7882715
  bytes (1), dictionary 1663 bytes (1); each SHA-256 matches the pinned value.
  Payload contains OCR attribution/licenses but no external ONNX model or dictionary.
  RPM Requires, ELF DT_NEEDED, and ldd contain no ORT/OpenCV/Paddle/Tesseract/Qt5
  dependency; ldd has no missing library; readelf has no RPATH or RUNPATH.
  UTF-16 strings expose expected/received byte and worker/reply timing diagnostics;
  the ELF imports socketpair@GLIBC_2.2.5. Source0 contains the exact
  socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, ...) implementation and its
  deterministic buffered-writer test.
  ```

  No install, uninstall, resident-process termination, desktop-cache mutation,
  commit, push, or system-file edit was performed. Remaining live acceptance is
  installation/restart of this exact RPM, network-disabled OCR, ordinary native
  captures, repeated RectArea fallback, natural resume/display-state behavior,
  latency, cancellation, and descriptor-leak observation.
  At final verification, the prior `/home/stfu/.local/libexec/ksnip-eof-diagnostic/ksnip`
  remained running and its user desktop override remained active; the system RPM
  was still `1.11.0-1.2875.gfae65bc1.fc44.x86_64`. The diagnostic uninstall state
  has no backup, so its uninstaller will remove rather than replace that override.

- 2026-09-10: Implemented the minimal client mitigation for demonstrated buffered
  nonblocking QFile producer truncation: `socketpair(AF_UNIX, SOCK_STREAM |
  SOCK_CLOEXEC, 0, ...)` replaces `pipe2` at the existing backend creation point.
  Original uncommitted diagnostics are preserved. QDBus descriptor duplication,
  local endpoint close, shared reader ownership until reply, worker ownership
  after release, async cancellation/destruction handling and all validation remain.
  This narrowly supersedes the pipe creation instruction in `03_MINIMAL_DESIGN.md`.

  Prior standalone evidence (`build-eof-diagnostic/writer-probe-report.md`):
  original buffered KWin writer failed 96/120 exact transfers on an 8192-byte
  pipe, versus 0/120 on default Unix stream sockets; completed buffered portions
  of extended runs had 621/800 pipe failures and 0/1200 socket failures. Some
  whole matrices timed out later during unbuffered controls; these are not
  complete extended matrix runs. The producer can report all bytes accepted,
  then lose its <=16 KiB QFile tail when destructor flush hits EAGAIN. Reading
  sooner cannot recover discarded bytes. Socket readiness/buffer headroom is an
  empirically supported mitigation on this host, not a universal guarantee for
  arbitrary socket buffer settings, kernels, Qt versions or memory pressure.
  The upstream writer remedy remains unbuffered writes with progress-aware poll.

  New deterministic production-reader tests use the original buffered writer
  pattern on a worker (5-second test poll timeout), with attribution retained.
  Payloads: 8196, 16380, 16384, 16388, 33177600 and 33177604 bytes; every byte
  is compared. Small cases finish QFile flush/FD close before reading; large
  frames are checked against actual SO_SNDBUF and the reader starts only after
  the first write. No sleeps or GUI production blocking were added. Existing
  early EOF, timeout, invalid metadata and closure assertions are preserved.

  Exact commands and outcomes:
  ```text
  cmake --build build-eof-diagnostic --target KWinScreenShot2ClientTests --parallel 2
  PASS (existing CaptureDto SFINAE warning).
  env QT_QPA_PLATFORM=offscreen ctest --test-dir build-eof-diagnostic -R '^KWinScreenShot2ClientTests$' --output-on-failure
  PASS: 1/1, 0.62 s.
  cmake --build build-eof-diagnostic --parallel 2
  PASS: rebuilt ksnip and all test targets (existing CaptureDto warning).
  env QT_QPA_PLATFORM=offscreen ctest --test-dir build-eof-diagnostic --output-on-failure
  PASS: 18/18, 1.76 s. Qt 6.11.1, OCR-disabled diagnostic configuration retained.
  git check-ignore build-eof-diagnostic/dist
  build-eof-diagnostic/dist
  python3 build-eof-diagnostic/stage-diagnostic.py
  PASS: created new dist; refuses any existing destination. Root dist untouched.
  python3 build-eof-diagnostic/check-install.py
  PASS: isolated HOME refusal, confirmed install, reinstall, exact binary/Exec,
  desktop validation, uninstall backup/restore and unrelated-file preservation.
  Repeated with wrapper checks: resident guard and successful checksum/install/
  validation/cache-refresh/launch argument flow pass; pgrep, kbuildsycoca6 and
  systemd-cat were mocked, so no real session operation or launch occurred.
  Evidence: build-eof-diagnostic/installer-check-vz3q2hne/
  sha256sum --check SHA256SUMS
  (workdir: build-eof-diagnostic/dist) PASS: all seven payload hashes.
  bash -n build-eof-diagnostic/install-and-launch.sh build-eof-diagnostic/dist/install-user.sh build-eof-diagnostic/dist/uninstall-user.sh
  desktop-file-validate build-eof-diagnostic/dist/org.ksnip.ksnip.desktop
  git diff --check
  PASS: all three checks.
  sha256sum build-eof-diagnostic/dist/ksnip
  15ceb095e5d5832e6eee10f0dfcf643583d54d537a69259e6cb00a2b9f1f09ec
  ```

  Sources: KWin writer/FD handling and executable authorization re-read from:
  - https://raw.githubusercontent.com/KDE/kwin/v6.7.3/src/plugins/screenshot/screenshotdbusinterface2.cpp
  - https://raw.githubusercontent.com/KDE/kwin/v6.7.3/src/utils/serviceutils.h
    (initial serviceutils.cpp lookup returned HTTP 404; implementation is in .h).
  Prior probe's Qt buffer/flush sources:
  - https://raw.githubusercontent.com/qt/qtbase/v6.11.1/src/corelib/io/qfiledevice.cpp
  - https://raw.githubusercontent.com/qt/qtbase/v6.11.1/src/corelib/io/qfsfileengine_unix.cpp

  User commands (after tray Quit; keep launch terminal open):
  ```bash
  bash /home/stfu/ai/trash-can/ksnip/build-eof-diagnostic/install-and-launch.sh
  journalctl -b -t ksnip-eof -o short-iso --no-pager
  # After quitting the diagnostic, restore the previous user desktop override
  # (or expose the system desktop entry when no previous override existed):
  bash /home/stfu/ai/trash-can/ksnip/build-eof-diagnostic/dist/uninstall-user.sh
  kbuildsycoca6 --noincremental
  ```
  No real user installation, process termination, live capture, system-file edit,
  suspend, commit or push performed. Live authorization, capture repetition,
  latency, resume and descriptor-leak acceptance remain pending. No claim that
  the demonstrated synthetic mechanism explains every previously observed EOF.

- 2026-09-10: Added failure-only ScreenShot2 transport diagnostics. IDs are
  process-wide across client instances; dispatched D-Bus failures include the
  error name and dispatch-to-reply time. Validated transfer failures include
  expected/received bytes and width/height/stride/format/effective scale; worker
  failures include queue and read durations in monotonic milliseconds. Read time
  includes validation/allocation/read/image construction, and reply time includes
  event-loop delivery. Unvalidated metadata is not dumped. Cancellation, async
  ownership, deadlines, and fallback behavior are preserved. Synthetic EOF tests
  now cover 0, 3, 4, and 23 of 24 bytes, reject partial images, verify descriptor
  closure and detail fields; timeout detail is checked too.

  Read-only runtime evidence:
  ```text
  rpm -q kwin kwin-wayland ksnip
  kwin-6.7.3-1.fc44.x86_64
  package kwin-wayland is not installed
  ksnip-1.11.0-1.2875.gfae65bc1.fc44.x86_64
  rpm -qf /usr/bin/kwin_wayland /usr/bin/ksnip
  kwin-6.7.3-1.fc44.x86_64
  ksnip-1.11.0-1.2875.gfae65bc1.fc44.x86_64
  pgrep -a -x 'ksnip|kwin_wayland'
  3557 /usr/bin/kwin_wayland [Wayland/Xwayland arguments]
  2171304 /usr/bin/ksnip
  readlink /proc/3557/exe /proc/2171304/exe
  /usr/bin/ksnip (KWin exe symlink did not resolve)
  journalctl -b --no-pager -o short-iso -g 'suspend entry|suspend exit|PM: suspend|System returned from sleep|ScreenShot2|image data ended|Error writing screenshot' -n 100
  2026-09-10 00:27:58+03:00: PID 2171304 background early EOF
  2026-09-10 01:17:20+03:00: suspend entry (s2idle)
  2026-09-10 09:17:40+03:00: suspend exit
  2026-09-10 12:35:47/49/52+03:00: PID 2171304 background early EOFs
  journalctl -b _COMM=kwin_wayland --no-pager -o short-iso --since '2026-09-10 09:17:00' --until '2026-09-10 12:36:30' -n 50
  2026-09-10 09:17:40+03:00: The main thread was hanging temporarily!
  ```
  This establishes temporal correlation, not suspend causality: EOF already
  occurred before today's suspend in the same resident PID. Targeted glob lookup
  for `screenshotdbusinterface2.cpp` under /home/stfu and screenshot sources under
  /usr/src (including /usr/src/debug) found none. A /tmp lookup was denied by tool
  policy and not retried. Producer-side write behavior remains unverified locally.
  Installed desktop entry read at /usr/share/applications/org.ksnip.ksnip.desktop
  has Exec=/usr/bin/ksnip and retains the ScreenShot2 restricted interface.

  Exact build/check commands and outcomes:
  ```text
  cmake --build build-diagnostic --target ksnip --parallel 2
  FAILED: build-diagnostic-deps/ort/Release/libonnxruntime_session.a missing.
  env -u CONDA_PREFIX PATH=/usr/bin:/bin cmake -S . -B build-eof-diagnostic -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_WITH_QT6=ON -DBUILD_TESTS=ON -DENABLE_BUILTIN_OCR=OFF -DUSE_SUBMODULE_KCOLORPICKER=ON -DUSE_SUBMODULE_KIMAGEANNOTATOR=ON -DGTest_DIR=/usr/lib64/cmake/GTest
  PASS. Separate capture-only candidate avoids rebuilding missing OCR dependencies.
  cmake --build build-eof-diagnostic --target KWinScreenShot2ClientTests --parallel 2
  First invocation hit the tool's 200000 ms limit at 172/375 steps; identical
  command with 600000 ms limit resumed and passed.
  env QT_QPA_PLATFORM=offscreen ctest --test-dir build-eof-diagnostic -R '^KWinScreenShot2ClientTests$' --output-on-failure
  PASS: 1/1, 0.08 s.
  cmake --build build-eof-diagnostic --parallel 2 > build-eof-diagnostic/build.log 2>&1
  PASS: application and all tests (existing Qt deprecation warnings).
  env QT_QPA_PLATFORM=offscreen ctest --test-dir build-eof-diagnostic --output-on-failure
  PASS: 18/18, 0 failures, 0.87 s; includes fake OCR workflow, not real OCR engine.
  env QT_QPA_PLATFORM=offscreen build-eof-diagnostic/src/ksnip --version
  Version: 1.11.0; Build: [empty]
  git diff --check
  PASS.
  sha256sum build-eof-diagnostic/src/ksnip /usr/bin/ksnip
  90ce677b71d7257f99a5f0c2d97c654ebe1501cea425119f6e7c8056f5bf3cb0  build-eof-diagnostic/src/ksnip
  e00d64f82ad591f5f50712565cf3c2e301b8f7d22158ad3f253db2b019b12f49  /usr/bin/ksnip
  ```
  No installation, resident termination, live screenshot, suspend, or commit was
  performed. The candidate has built-in OCR disabled; use ordinary captures for
  this transport investigation. User launch (after Quit from the tray), subject
  to the desktop authorization prerequisite above:
  ```bash
  if pgrep -x ksnip >/dev/null; then
      printf 'Quit the existing KSnip from its tray menu first.\n'
  else
      systemd-cat --identifier=ksnip-eof stdbuf -oL -eL /home/stfu/ai/trash-can/ksnip/build-eof-diagnostic/src/ksnip
  fi
  ```
  Read results with `journalctl -b -t ksnip-eof -o short-iso --no-pager`.
  Keep the launch terminal open. Capture before/after the next normal resume,
  note the wall-clock time and selected mode, then compare the byte shortfall and
  worker timings with the read-only journal commands above. Live authorization
  and reproducing an EOF with this candidate remain untested.

- 2026-08-31: Nine frozen-background early EOFs and one live-fallback early EOF
  were observed after display-state changes while the resident process remained
  healthy. The black/flickering failure-only selector was traced to changing
  `WA_TranslucentBackground` after the reused top-level widget had already shown
  a frozen background. `WaylandSnippingArea` now opts into a persistent alpha
  surface at construction; a regression covers frozen selection, Escape, a
  subsequent background early EOF, live fallback, and cancellation. Focused
  `KdeWaylandImageGrabberTests` pass 1/1, the full offscreen Qt 6 suite passes
  18/18, and RPM `%check` passes 18/18. The local binary RPM is
  `/home/stfu/ai/trash-can/ksnip/build-translucency-rpm/RPMS/x86_64/ksnip-1.11.0-1.2875.gfae65bc1.fc44.x86_64.rpm` with SHA-256
  `8fff0423427dd44441b5815023c3a070835256a3ab3b9d6b3a3abacdedd9e81f`;
  the SRPM is
  `/home/stfu/ai/trash-can/ksnip/build-translucency-rpm/SRPMS/ksnip-1.11.0-1.2875.gfae65bc1.fc44.src.rpm`
  with SHA-256
  `efaea62067f5b56dad70b76e83f9fb0b63b5bf0e681ca750ac86d5fff52b1457`.
  The first local build stopped on absent `python3-flatbuffers`; the successful
  retry used the already installed Miniconda Python module without changing the
  host. The package intentionally carries pre-commit `fae65bc1` metadata but was
  built and tested from the fixed tracked worktree. Installed live reproduction
  remains next.

- 2026-08-24: Intermittent missing selectors were traced to repeated
  `CaptureWorkspace` early EOFs while the resident process remained healthy.
  Added a dedicated RectArea-client fallback state machine: background failure
  opens the existing transparent selector, overlay completion queues logical
  `CaptureArea`, and final terminal ownership remains serialized with latest
  request replacement. Focused transport/backend tests, the full 18-test Qt 6
  suite, OCR-enabled build, and local RPM `%check` pass. Installed fallback
  activation is next.

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
- 2026-08-23: Live fractional-scale diagnostics captured selector canvas and
  frozen background at the same `1463x914` dimensions while the old primary-DPR
  path transformed selection `(0,403 1043x226)` into out-of-bounds crop
  `(0,806 2086x452)`. The extent-derived crop remained
  `(0,403 1043x226)`. Switching only KDE's frozen crop to that mapping produced
  the same requested YouTube title/description region as Spectacle. Temporary
  diagnostic logging was removed; focused `KdeWaylandImageGrabberTests` and the
  full offscreen Qt 6 suite pass 1/1 and 18/18 respectively.
- 2026-08-23: The clean OCR-enabled local candidate recognized the same corrected
  YouTube title/description region at 175% scale. The output retained expected
  model-level homoglyph and punctuation errors, but no text from the previously
  shifted bottom-edge crop appeared. Advanced to one final RPM build.
- 2026-08-23: The fractional-scale fix built into local binary RPM
  `ksnip-1.11.0-1.2871.gecd59bda.fc44.x86_64.rpm` with SHA-256
  `3c34c22246bdbc46c5b4154dbf234fc9e04f129098d00efc58cf4f8ad6d7849d`.
  RPM `%check` passed 18/18 tests. Payload, Requires, `ldd`, and `readelf` retain
  the embedded-only OCR closure with no model file, OCR shared dependency, or
  RPATH; the final ELF contains no temporary RectArea diagnostic string.
  Advanced to installed-package live acceptance.

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

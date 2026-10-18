# Goal: disk-backed open screenshot tabs

Status: active
Source: user-approved memory plan and audit; implement, commit, push, and finish only after a successful GitHub Actions build.
Last updated: 2026-10-18

## Objective

Keep approximately 100 screenshot tabs open without keeping every inactive
document's raster payload decoded in RAM. Use session-local disk backing and
exact raster deduplication without losing editable annotations or undo/redo.

## Execution Directive

Complete the frozen Required Outcomes using the listed Change Envelope and
Primary Evidence. Work on the smallest unresolved outcome. Do not add
requirements from reviews, tests, tools, speculative risks, or optional source
text. Finish when every required outcome is resolved and affected constraints
remain satisfied.

## Frozen Contract

### Required Outcomes

- R1: Open inactive documents release decoded screenshot resources and use
  deduplicated, lossless session-local backing files.
  - Source: user's 100-open-screenshot disk-cache request and approved audit.
  - Acceptance: real editor tests exercise 100 distinct and identical open tabs,
    release inactive raster owners, and restore matching pixels and metadata.
  - Primary evidence: kImageAnnotator store/paging tests.
  - Status: verified
  - Evidence: final Qt 6 editor build and 51/51 offscreen test executables pass;
    real 100-tab same/overlapping/distinct cases check actual inactive pixmaps,
    resource round trips, and exact deduplication. Controlled measurements below.
- R2: Editing, undo/redo, current capture auto-copy, background export/save,
  closing, cancellation, and storage failures preserve documents and ordering.
  - Source: approved implementation plan and existing public behavior.
  - Acceptance: targeted library and KSnip tests pass; pending raster work and
    acquisition are bounded; no new cache disk I/O or waits on the GUI thread.
  - Primary evidence: library paging/command tests and KSnip handler tests.
  - Status: verified
  - Evidence: final editor suite 51/51 and integrated KSnip suite 21/21 pass,
    including transform/snapshot fidelity, deferred operations, reservation
    failures, cancellation, and asynchronous shutdown cleanup.
- R3: Commit and push a reproducible source revision whose GitHub Actions
  Fedora RPM build completes successfully.
  - Source: user's explicit completion condition.
  - Acceptance: published root and library commits, successful Actions run for
    the delivered root SHA, with test/build steps and RPM artifact.
  - Primary evidence: git SHAs and GitHub Actions run URL/conclusion.
  - Status: in_progress
  - Evidence: library ac73599e0dc5a59831e2a0e34ca64810aaa933d5 is published on
    `0FL01/ksnip:kimageannotator-raster-cache`; root push/Actions pending.

### Constraints

- Local compilation uses at most four CPU cores/jobs and a reduced scheduling
  priority. Agents do not run competing builds.
- Preserve existing Wayland/X11/Windows/macOS and OCR behavior, desktop identity,
  permissions, and old system-library builds. Do not restart or manipulate the
  user's running KSnip or modify installed/system files.
- Current-document raster history stays resident; inactive scene/vector/undo
  metadata stays live. Do not promise a fixed whole-process MiB target.
- Keep source changes reproducible, including the modified library revision.

### Non-goals

- Compression, persistent document recovery, startup orphan-session GC,
  prefetch, configurable cache policies, perceptual/tiled deduplication,
  per-command asynchronous undo, OCR or capture-backend optimization.
- Whole-project serialization, flattened document replacement, undo limits,
  automatic clipboard clearing, and unrelated cleanup.

## Change Envelope

- Bundled kImageAnnotator: private raster store/handles, raster-owning commands
  and items, tab readiness/lifetime, additive opt-in APIs, and relevant tests.
- KSnip: annotator adapter, capture metadata/operations, bounded input admission,
  current-image readiness and shutdown; associated tests and build integration.
- Existing Fedora RPM workflow, source pinning (including its macOS retagging
  consumer), and this execution record.
- One QtCore worker, raw-only private backing files, no new runtime dependency.

## Current Checkpoint

- Closes: R3.
- Smallest next action: publish the root commit and inspect its Fedora RPM and
  compatibility jobs until the required build/test artifact succeeds.
- Expected evidence: successful Actions run for the published root revision.
- Replan if: CI identifies a concrete build/test failure.

## Current State

- Resolved: R1/R2 implementation and final local test gates are green. The
  modified library commit is already published; its worktree is clean.
- Last relevant evidence: 51/51 editor tests (26.21 s), 21/21 app tests (4.43 s),
  and the controlled 100-screenshot memory experiment below.
- Blocker: none proven. `gh` is absent; existing git transport and Actions read
  tools remain available for publication and verification.
- Next: push the root pin/integration and verify GitHub Actions.

## Material Decisions

- 2026-10-18: raw payload files; metadata/digest remain in RAM. No disk codec,
  compression, database, or cross-session recovery.
- 2026-10-18: current-ready actions retain synchronous semantics; only cold
  document preparation/export is asynchronous.
- 2026-10-18: local compiler work is centralized, limited to four cores/jobs,
  and run with lowered priority.
- 2026-10-18: publish the standalone modified library on the
  `kimageannotator-raster-cache` branch of the same authorized GitHub repository
  and pin its commit in `.gitmodules`/the gitlink. This avoids an unpublished
  submodule SHA, vendoring, and requiring creation of a separate fork. The
  library branch has a standalone library tree, so recursive checkout terminates.
- 2026-10-18: the macOS dependency helper must preserve the editor gitlink:
  fetching the latest tag from the shared publication remote would otherwise
  select a KSnip application tag instead of the modified library source tree.
- 2026-10-18: source review proved a pure capacity query could strand an already
  admitted delayed capture after a storage failure and make Quit wait forever.
  Added one explicit pre-acquisition reservation, consumed by one input even if
  capacity later fails; preserve old unbacked resources until a successful retry
  and reject further input. No frame queue or additional worker is introduced.
- 2026-10-18: a known backing-write failure freezes further editing/raster
  production, while recovery Save/Copy remains available. A previously granted
  input credit is still honored; selecting another tab explicitly retries I/O.
- 2026-10-18: normal Quit awaits asynchronous worker/session cleanup before
  exiting the process. Merely queuing cleanup in widget destruction could leave
  backing files even on a normal exit; no GUI-thread wait or polling is added.

## Checkpoint History

- 2026-10-18: froze R1-R3 and delegated independent library/handler work;
  no build or implementation evidence yet.
- 2026-10-18: Qt 6 Debug/test configuration succeeded; built the unchanged
  kColorPicker dependency with `nice -n 10 taskset -c 0-3 cmake --build
  build-memory --target kColorPicker --parallel 4`. Selected system
  `GTest_DIR=/usr/lib64/cmake/GTest` to avoid unrelated Conda runtime paths.
  `git push --dry-run origin HEAD:refs/heads/master` succeeded; publication
  transport is available. Next: compile and test completed integration.
- 2026-10-18: syntax-only checks passed for the adapter, input/IPC/drop flow,
  capture handlers, and new tests. A separate offscreen Qt probe passed all
  64 real-tab move combinations, preserving current identity and metadata.
  Workflow YAML parsing and `git diff --check` passed. The full Qt 6 build is
  running with the four-core/low-priority limit; no test-suite pass claimed yet.
- 2026-10-18: `nice -n 10 taskset -c 0-3 cmake --build build-memory
  --parallel 4` passed. `QT_QPA_PLATFORM=offscreen nice -n 10 taskset -c 0-3
  ctest --test-dir build-memory --output-on-failure --no-tests=error -j1`
  passed all 21 test executables, including the new handler/adapter/drop tests.
  Library ownership/paging verification and the final integrated rebuild remain
  pending; this is not yet R1/R2 completion evidence.
- 2026-10-18: standalone editor configuration exposed a kColorPicker build-tree
  package export-name mismatch (`kColorPicker-targets.cmake` was missing).
  Staging the existing dependency with `cmake --install build-memory/kColorPicker
  --prefix "$PWD/build-memory/editor-dependencies"` and selecting that prefix
  fixed configuration without changing the dependency. The RPM editor-test gate
  now stages the same private dependency prefix. The separate editor build is
  running with the four-core/low-priority limit.
- 2026-10-18: actual Qt paging checks pass for 100 identical, seven overlapping,
  and 100 distinct screenshots, including empty inactive background pixmaps.
  Transform undo/redo, stable inactive effect geometry, and one reserved input
  across a failed write pass. A fidelity failure identified a stale Duplicate
  effect cache after its late snapshot assignment; source invalidation and
  direct drawing for no-op effects fix the mismatch. Tests also exposed Qt's
  nested-render discovery-bit behavior for unpainted blur masks; cache-mode
  export now prepares pending masks before the outer scene render. The full
  editor gate and final app rebuild remain pending.
- 2026-10-18: the full editor run exposed an undo-stack teardown signal into a
  half-destroyed tab layout and an uninitialized Cut output at fractional DPR.
  Disconnecting teardown notifications and initializing the cut image fix the
  reproduced crash and pixel mismatch. The next gate also covers shutdown drain
  and mutation blocking after a confirmed (not merely queued) write failure.
- 2026-10-18: both final gates pass: `nice -n 10 taskset -c 0-3 cmake --build
  build-memory-editor --parallel 4`, then `QT_QPA_PLATFORM=offscreen
  TMPDIR="$PWD/build-memory/test-tmp" nice -n 10 taskset -c 0-3 ctest
  --test-dir build-memory-editor/tests --output-on-failure --no-tests=error -j1`
  passes 51/51; the corresponding `build-memory` rebuild/test passes 21/21.
  Published library commit `ac73599e0dc5a59831e2a0e34ca64810aaa933d5` before
  committing the root gitlink, so recursive checkout is reproducible.

## Controlled Memory Evidence

Qt 6.11.2/offscreen, one isolated editor process per mode, same feature binary
with backing disabled/enabled, 1920x1080 ARGB32 patterned inputs, one current
document and no raster-edit history. Input images are released after admission.
Read `/proc/self/smaps_rollup` after each completed write. Measurements are
logical process attribution, not a claim about the user's running full KSnip.

| Open tabs | PSS, backing disabled (KiB) | PSS, distinct backed images (KiB) | PSS, identical backed images (KiB) |
| --- | ---: | ---: | ---: |
| 1 | 40,368 | 43,668 | 43,352 |
| 10 | 113,632 | 60,079 | 59,789 |
| 100 | 846,580 | 68,311 | 67,945 |

SwapPss is zero in every sample. At 100 distinct images, PSS is approximately
826.7 versus 66.7 MiB (91.9% lower, 12.4x). Backing consists of 100 raw blobs /
829,440,000 bytes; 100 identical inputs use one blob / 8,294,400 bytes. This
does not predict OCR, clipboard, capture buffers, or a large active undo history.
The standalone probe and logs are under ignored `build-memory/memory-probe*`
and `build-memory/memory-measurements.log`; it also awaits normal cleanup.

## Completion

- Resolved outcomes: pending.
- Commands and artifacts: pending.
- Constraint and diff-scope check: pending.
- Final status: active.

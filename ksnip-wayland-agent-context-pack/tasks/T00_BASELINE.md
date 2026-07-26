# T00 — Establish the baseline

## Goal

Prove the unchanged checkout builds and collect runtime capabilities.

## Actions

- Record git status and SHA.
- Initialize submodules.
- Build Qt 6 unchanged.
- Run the binary with `--version`.
- Run `scripts/probe-wayland.sh`.
- Locate the actual executable output.
- Read the current constructors/factories; do not rely on stale signatures in
  this context pack.
- Update `STATE.md`.

## Acceptance

- Baseline build command is reproducible.
- Existing build/test failures are separated from new work.
- ScreenShot2 and GlobalShortcuts versions are recorded.
- No source behavior has changed.

## Deliverable

A baseline commit only if build scripts or notes were added; otherwise record
the baseline SHA without creating a no-op commit.

# T05 — Verify responsiveness and create dist/

## Goal

Run the complete acceptance checklist and deliver reproducible artifacts.

## Actions

- Build RelWithDebInfo or Release.
- Run automated tests that are viable.
- Run functional repetitions.
- Measure shortcut-to-overlay latency.
- Check idle CPU.
- Check file descriptors before/after repeated captures.
- Check `file`, `ldd`, and `--version`.
- Generate installer/uninstaller.
- Generate build info, report, patch, and SHA256SUMS.

## Acceptance

Every mandatory item in `05_ACCEPTANCE_CRITERIA.md` is green or has a precise,
honest evidence-backed exception. Mandatory user workflows may not be marked
as exceptions.

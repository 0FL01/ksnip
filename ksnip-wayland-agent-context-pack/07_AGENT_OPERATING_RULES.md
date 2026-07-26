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

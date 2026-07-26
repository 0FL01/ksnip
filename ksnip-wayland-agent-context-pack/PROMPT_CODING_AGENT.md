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

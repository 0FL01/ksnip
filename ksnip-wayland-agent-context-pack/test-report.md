# KSnip KDE Wayland test report

Date: 2026-07-26
Candidate: `dist/ksnip`
Target: Fedora 44 x86_64, KDE Plasma 6.7.3 Wayland, Qt 6.11.1

## Automated evidence

- `cmake --build build-agent --parallel 2`: PASS.
- `cmake --build build-tests --parallel 2`: PASS.
- `ctest --test-dir build-tests --output-on-failure -j1`: PASS, 13/13.
- `KWinScreenShot2ClientTests`: PASS (bounded pipe reads, metadata, EOF,
  timeout, descriptor ownership, invalid area).
- `WaylandGlobalShortcutManagerTests`: PASS (safe XDG trigger conversion and
  exact `a(sa{sv})` QtDBus signature).
- `git diff --check`: PASS.
- Delivery shell syntax, desktop validation, and `SHA256SUMS`: PASS.
- Temporary-HOME installer sentinel test: PASS. Replacement without the
  confirmation flag was refused; confirmed install, reinstall, uninstall,
  pre-existing desktop restoration, and unrelated-file preservation passed.
- The exact `dist/ksnip` candidate is installed at
  `/home/stfu/.local/libexec/ksnip-wayland/ksnip`; binary comparison, desktop
  validation, and the `~/.local/bin/ksnip-wayland` link passed.

## Target-session checks

These checks require the user-installed candidate and remain pending until the
user runs the live checklist:

- [x] FullScreen/workspace capture returns a non-empty image.
- [x] CurrentScreen captures the active screen.
- [x] ActiveWindow captures the expected window.
- [ ] selected-window capture works and Escape cancels quietly.
- [x] RectArea uses the KSnip overlay and preserves the content of a
  focus-sensitive fullscreen application.
- [x] Escape cancels RectArea quietly.
- [ ] Cursor-enabled capture does not crash.
- [ ] Forced generic Wayland uses the Screenshot portal fallback.
- [ ] Five portal shortcut IDs trigger their matching action exactly once with
  another application focused.
- [x] A clean restart preserves the three user-assigned global bindings without
  duplicate activation.
- [ ] Twenty shortcut-to-overlay samples have median below 200 ms and no
  KSnip-caused multi-second outlier.
- [ ] Thirty RectArea and thirty FullScreen captures complete without hangs or
  empty success; RectArea FD count is stable before/after.
- [ ] Idle CPU shows no shortcut polling loop or per-trigger process startup.
- [x] Installed desktop launcher works before and after an application restart.

Live acceptance status: **USER-REQUIRED WORKFLOWS PASS; BROADER CHECKLIST
PENDING**.

User observation on 2026-07-26: FullScreen, RectArea, and ActiveWindow pass.
Runtime tracing proved the portal activation/capture path, but KDE stored active
physical bindings as `none`. The recovery candidate adds Options > Configure
Global Shortcuts through portal v2 and awaits restart, user assignment, and the
physical-key/Yakuake live gate.

After assigning shortcuts in KDE, RectArea, CurrentScreen, and ActiveWindow
activate globally and persist across a clean KSnip restart. Fullscreen Yakuake
and Minecraft exposed a RectArea regression: activating the live selector hid
the source content. The current candidate takes one ScreenShot2 workspace image
before activating the selector and crops that frozen background. Automated
build and all 13 tests pass. The user confirmed correct RectArea behavior with
fullscreen Yakuake and Minecraft, quiet cancellation, and subjectively much
lower latency than Spectacle.

# KSnip KDE Wayland test report

Date: 2026-07-26
Candidate: `dist/ksnip`
Target: Fedora 44 x86_64, KDE Plasma 6.7.3 Wayland, Qt 6.11.1

## Automated evidence

- `cmake --build build-agent --parallel`: PASS.
- `cmake --build build-tests --parallel`: PASS.
- `ctest --test-dir build-tests --output-on-failure`: PASS, 13/13.
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
- [ ] CurrentScreen captures the active screen.
- [x] ActiveWindow captures the expected window.
- [ ] selected-window capture works and Escape cancels quietly.
- [x] RectArea uses the KSnip overlay and returns the selected area.
- [ ] Escape cancels RectArea quietly.
- [ ] Cursor-enabled capture does not crash.
- [ ] Forced generic Wayland uses the Screenshot portal fallback.
- [ ] Five portal shortcut IDs trigger their matching action exactly once with
  another application focused.
- [ ] Restart and settings-session recreation do not duplicate activations.
- [ ] Twenty shortcut-to-overlay samples have median below 200 ms and no
  KSnip-caused multi-second outlier.
- [ ] Thirty RectArea and thirty FullScreen captures complete without hangs or
  empty success; RectArea FD count is stable before/after.
- [ ] Idle CPU shows no shortcut polling loop or per-trigger process startup.
- [ ] Installed desktop launcher works before and after an application restart.

Live acceptance status: **PENDING USER VERIFICATION**.

User observation on 2026-07-26: FullScreen, RectArea, and ActiveWindow pass.
Configured Wayland global shortcuts currently produce no activation; diagnosis
is the next checkpoint.

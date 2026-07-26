# Primary objective

Modify the current upstream KSnip checkout and deliver a **working, locally
installable, Qt 6, x86_64 Linux binary** for this target:

- Fedora Linux
- KDE Plasma 6.7.3
- Wayland session
- AMD Ryzen 7 7840HS
- resident/tray workflow
- low-latency global shortcuts

The installed build must provide these capture actions:

1. Rectangular area selected with the existing KSnip overlay.
2. Full workspace / all screens.
3. Active window.
4. User-selected window.
5. Current/active screen when it is cheap to retain.

Required implementation paths:

- KDE Plasma capture: `org.kde.KWin.ScreenShot2`.
- Other Wayland capture: preserve the existing Screenshot Portal fallback.
- Wayland global shortcuts: `org.freedesktop.portal.GlobalShortcuts`.
- X11 and Windows global shortcuts: preserve the existing implementations.

The final result is not an analysis or patch proposal. The final result is:

- a built ELF executable;
- a safe user-local installer;
- a safe uninstaller;
- build metadata;
- SHA-256 checksum;
- test report;
- source diff or commit list.

Success is measured on the target KDE Wayland session, not by theoretical
support for every compositor.

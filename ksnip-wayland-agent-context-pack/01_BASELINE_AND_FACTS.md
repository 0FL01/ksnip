# Baseline and verified facts

Treat the actual checkout as the source of truth. Re-check every item before
editing because upstream may move.

At context-pack creation time, upstream had the following relevant properties:

- C++17 and CMake.
- Optional Qt 6 build via `BUILD_WITH_QT6=ON`.
- `KdeWaylandImageGrabber` still used the legacy:
  - service `org.kde.KWin`
  - object `/Screenshot`
  - interface `org.kde.kwin.Screenshot`
  - serialized `QImage` transport through `QDataStream`
- The legacy KDE grabber advertised several modes but routed unmatched modes to
  the old `interactive` method. In particular, advertised RectArea support was
  not a correct KSnip-overlay plus `CaptureArea` implementation.
- `KeyHandlerFactory` returned `DummyKeyHandler` on Wayland.
- The existing high-level `GlobalHotKeyHandler` already maps configured
  shortcuts to capture modes and actions.
- The generic Wayland grabber already calls
  `org.freedesktop.portal.Screenshot`; keep it as fallback.
- The desktop entry already mentioned
  `X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2`, but its `Exec`
  value was hard-coded and must match the actually installed binary for a
  reliable development installation.
- The existing “FullScreen” desktop action used `ksnip -m`; inspect CLI
  semantics and correct it only if verified wrong.

Likely relevant files:

```text
CMakeLists.txt
src/CMakeLists.txt
desktop/org.ksnip.ksnip.desktop

src/backend/imageGrabber/KdeWaylandImageGrabber.*
src/backend/imageGrabber/WaylandImageGrabber.*
src/backend/imageGrabber/ImageGrabberFactory.*
src/backend/imageGrabber/AbstractImageGrabber.*
src/backend/imageGrabber/AbstractRectAreaImageGrabber.*

src/gui/snippingArea/WaylandSnippingArea.*
src/gui/snippingArea/AbstractSnippingArea.*

src/gui/globalHotKeys/GlobalHotKeyHandler.*
src/gui/globalHotKeys/GlobalHotKey.*
src/gui/globalHotKeys/keyHandler/KeyHandlerFactory.*

src/dependencyInjector/*
src/common/platform/PlatformChecker.*
```

Reference implementation for a minimal subset of ScreenShot2 behavior:

```text
KDE Spectacle:
src/Platforms/ImagePlatformKWin.cpp
src/Platforms/ImagePlatformKWin.h

KWin protocol definition:
src/plugins/screenshot/org.kde.KWin.ScreenShot2.xml
```

Do not copy a large Spectacle subsystem. Reuse only the small transport pattern
needed by KSnip and preserve copyright/license attribution for copied code.

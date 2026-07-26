# Minimal design

## 1. Backend selection

Do not build a generalized backend framework.

Use a small runtime probe:

```text
Wayland + ScreenShot2 service/path/interface available
    -> KdeWaylandImageGrabber using ScreenShot2
otherwise
    -> existing WaylandImageGrabber portal fallback
```

Prefer probing the D-Bus interface/property over trusting only
`XDG_CURRENT_DESKTOP`.

## 2. KWin ScreenShot2 endpoint

```text
service:   org.kde.KWin.ScreenShot2
path:      /org/kde/KWin/ScreenShot2
interface: org.kde.KWin.ScreenShot2
property:  Version
```

Minimal mode mapping:

```text
FullScreen        -> CaptureWorkspace
CurrentScreen     -> CaptureActiveScreen
ActiveWindow      -> CaptureActiveWindow
WindowUnderCursor -> CaptureInteractive(kind = 0)
RectArea          -> existing KSnip overlay, then CaptureArea
```

For KDE Wayland, the existing `WindowUnderCursor` action may represent
“click/select a window” through `CaptureInteractive(0)`. Do not add a new enum
or translate the whole UI in this MVP.

## 3. ScreenShot2 transport

Use the proven minimal pattern from Spectacle:

1. Create a blocking pipe with `pipe2(..., O_CLOEXEC)`.
2. Do **not** retain the old `O_NONBLOCK` plus recursive polling loop.
3. Build an asynchronous D-Bus call and pass the write descriptor as
   `QDBusUnixFileDescriptor`.
4. Close the local write descriptor after dispatch.
5. Use a finite timeout:
   - normal capture: approximately 4 seconds;
   - interactive window selection: approximately 60 seconds.
6. On a successful `QVariantMap` reply:
   - require `type == "raw"`;
   - validate `width`, `height`, `format`, `stride`, and calculated byte sizes;
   - allocate a `QImage`;
   - read the pipe in a worker/future, never on the GUI thread;
   - handle partial reads and early EOF;
   - set `devicePixelRatio` from `scale` when valid;
   - emit success only for a non-null complete image.
7. Treat `org.kde.KWin.ScreenShot2.Error.Cancelled` as cancellation, not failure.
8. Close every descriptor exactly once on every path.
9. Prevent stale async completions from acting on a destroyed object.

Keep the implementation small. One focused helper such as
`KWinScreenShot2Client` is acceptable only if it reduces duplication and makes
metadata parsing testable.

## 4. Area selection flow

Reuse `WaylandSnippingArea` / the existing KSnip selector.

Preferred MVP flow:

```text
shortcut/action
  -> show transparent KSnip overlay without frozen screenshot
  -> user selects logical QRect
  -> overlay closes
  -> event loop resumes
  -> CaptureArea(x, y, width, height)
  -> image enters the existing editor pipeline
```

Rules:

- Escape emits cancellation.
- Reject empty or negative-size rectangles.
- Use global logical coordinates expected by KWin.
- Avoid arbitrary sleep delays.
- Do not first capture the whole desktop and crop it.
- Do not use the portal picker on the native KDE path.
- Save last-rect only if it is already a trivial integration.

## 5. Global shortcuts

Keep the existing `GlobalHotKeyHandler` for non-Wayland platforms.

On Wayland, add one small manager around:

```text
org.freedesktop.portal.Desktop
/org/freedesktop/portal/desktop
org.freedesktop.portal.GlobalShortcuts
```

Session lifecycle:

1. Probe the portal interface/version.
2. Create one session for the resident KSnip process.
3. Use stable shortcut IDs:
   - `capture.rect_area`
   - `capture.full_screen`
   - `capture.current_screen`
   - `capture.active_window`
   - `capture.select_window`
4. List existing shortcuts for the application.
5. Bind missing shortcuts once, with descriptions and preferred triggers.
6. Map `Activated` signals directly to existing
   `captureTriggered(CaptureModes)` behavior.
7. On hotkey settings changes, close and recreate the portal session rather
   than trying to mutate a session that only permits one bind operation.
8. Close the session on shutdown.
9. Do not poll.

Do not force the portal into the native event-filter/IKeyHandler abstraction.
Portal activation is an action signal, not a raw key event.

If and only if the target host is proven not to expose a usable
GlobalShortcuts Portal, a small optional `KF6::GlobalAccel` fallback may be
considered. Do not implement both preemptively.

## 6. Desktop identity and installation

KWin ScreenShot2 is a restricted D-Bus interface. The development build must
be launched through an installed desktop entry whose identity and `Exec` path
match the actual binary.

The final package must install only into the user account, for example:

```text
~/.local/libexec/ksnip-wayland/ksnip
~/.local/share/applications/org.ksnip.ksnip.desktop
~/.local/bin/ksnip-wayland -> actual binary
```

The generated desktop entry must:

- use the absolute actual executable path;
- declare `org.kde.KWin.ScreenShot2`;
- retain normal KSnip metadata/actions;
- use the desktop ID expected by the application.

Never disable compositor security checks. Never patch system portal files.

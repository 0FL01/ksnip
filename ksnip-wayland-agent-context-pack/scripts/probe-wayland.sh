#!/usr/bin/env bash
set -u

echo "== session =="
printf 'XDG_SESSION_TYPE=%s\n' "${XDG_SESSION_TYPE:-}"
printf 'XDG_CURRENT_DESKTOP=%s\n' "${XDG_CURRENT_DESKTOP:-}"
printf 'WAYLAND_DISPLAY=%s\n' "${WAYLAND_DISPLAY:-}"
printf 'QT_QPA_PLATFORM=%s\n' "${QT_QPA_PLATFORM:-}"

echo
echo "== KWin ScreenShot2 version =="
busctl --user get-property \
  org.kde.KWin.ScreenShot2 \
  /org/kde/KWin/ScreenShot2 \
  org.kde.KWin.ScreenShot2 \
  Version 2>&1 || true

echo
echo "== KWin ScreenShot2 interface =="
busctl --user introspect \
  org.kde.KWin.ScreenShot2 \
  /org/kde/KWin/ScreenShot2 2>&1 || true

echo
echo "== XDG GlobalShortcuts version =="
busctl --user get-property \
  org.freedesktop.portal.Desktop \
  /org/freedesktop/portal/desktop \
  org.freedesktop.portal.GlobalShortcuts \
  version 2>&1 || true

echo
echo "== portal GlobalShortcuts interface =="
busctl --user introspect \
  org.freedesktop.portal.Desktop \
  /org/freedesktop/portal/desktop \
  org.freedesktop.portal.GlobalShortcuts 2>&1 || true

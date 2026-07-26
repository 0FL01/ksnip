#!/usr/bin/env bash
set -euo pipefail

PREFIX="${HOME:?HOME is not set}/.local"
INSTALL_DIR="$PREFIX/libexec/ksnip-wayland"
INSTALL_BIN="$INSTALL_DIR/ksnip"
STATE_FILE="$INSTALL_DIR/install-state"
DESKTOP_DIR="$PREFIX/share/applications"
DESKTOP_FILE="$DESKTOP_DIR/org.ksnip.ksnip.desktop"
DESKTOP_BACKUP="$INSTALL_DIR/org.ksnip.ksnip.desktop.backup"
CLI_LINK="$PREFIX/bin/ksnip-wayland"

if [[ ! -f "$STATE_FILE" ]] || ! grep -Fqx 'format=1' "$STATE_FILE"; then
  echo "ERROR: no recognized ksnip-wayland user installation at $INSTALL_DIR" >&2
  exit 1
fi

state_value() {
  local key="$1"
  awk -F= -v key="$key" '$1 == key { print substr($0, length(key) + 2); exit }' "$STATE_FILE"
}

remove_if_unchanged() {
  local path="$1"
  local expected_sha="$2"
  if [[ ! -e "$path" ]]; then
    return 0
  fi
  if [[ "$(sha256sum "$path" | awk '{print $1}')" != "$expected_sha" ]]; then
    echo "WARNING: leaving modified installer file in place: $path" >&2
    return 1
  fi
  rm -f -- "$path"
}

binary_removed=true
desktop_removed=true
remove_if_unchanged "$INSTALL_BIN" "$(state_value binary_sha256)" || binary_removed=false
remove_if_unchanged "$DESKTOP_FILE" "$(state_value desktop_sha256)" || desktop_removed=false

if [[ -L "$CLI_LINK" && "$(readlink -- "$CLI_LINK")" == "$INSTALL_BIN" ]]; then
  rm -f -- "$CLI_LINK"
elif [[ -e "$CLI_LINK" || -L "$CLI_LINK" ]]; then
  echo "WARNING: leaving modified CLI path in place: $CLI_LINK" >&2
fi

if [[ "$desktop_removed" == true && -f "$DESKTOP_BACKUP" ]]; then
  install -m 0644 "$DESKTOP_BACKUP" "$DESKTOP_FILE"
  rm -f -- "$DESKTOP_BACKUP"
fi

if [[ "$binary_removed" == true && "$desktop_removed" == true ]]; then
  rm -f -- "$STATE_FILE"
  rmdir --ignore-fail-on-non-empty "$INSTALL_DIR"
else
  echo "WARNING: installer state retained because modified files remain." >&2
fi

if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database "$DESKTOP_DIR" ||
    echo "WARNING: desktop database update failed; files were removed." >&2
fi

echo "Uninstalled installer-owned ksnip-wayland files."

#!/usr/bin/env bash
set -euo pipefail

DIST_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="${HOME:?HOME is not set}/.local"
INSTALL_DIR="$PREFIX/libexec/ksnip-wayland"
INSTALL_BIN="$INSTALL_DIR/ksnip"
STATE_FILE="$INSTALL_DIR/install-state"
DESKTOP_DIR="$PREFIX/share/applications"
DESKTOP_FILE="$DESKTOP_DIR/org.ksnip.ksnip.desktop"
DESKTOP_BACKUP="$INSTALL_DIR/org.ksnip.ksnip.desktop.backup"
CLI_LINK="$PREFIX/bin/ksnip-wayland"
REPLACE_DESKTOP=false

if [[ "${1:-}" == "--replace-existing-desktop" && "$#" -eq 1 ]]; then
  REPLACE_DESKTOP=true
elif [[ "$#" -ne 0 ]]; then
  echo "Usage: $0 [--replace-existing-desktop]" >&2
  exit 2
fi

if [[ ! -x "$DIST_DIR/ksnip" || ! -f "$DIST_DIR/org.ksnip.ksnip.desktop" ]]; then
  echo "ERROR: run this script from a complete ksnip dist directory" >&2
  exit 1
fi

expected_execs=(
  "Exec=$INSTALL_BIN %F"
  "Exec=$INSTALL_BIN -r -c"
  "Exec=$INSTALL_BIN -l -c"
  "Exec=$INSTALL_BIN -f -c"
  "Exec=$INSTALL_BIN -a -c"
)
for expected_exec in "${expected_execs[@]}"; do
  if ! grep -Fqx "$expected_exec" "$DIST_DIR/org.ksnip.ksnip.desktop"; then
    echo "ERROR: desktop entry does not target this user's install path: $expected_exec" >&2
    exit 1
  fi
done

if [[ -e "$INSTALL_DIR" && ! -f "$STATE_FILE" ]]; then
  echo "ERROR: refusing to overwrite unrelated path: $INSTALL_DIR" >&2
  exit 1
fi
if [[ -f "$STATE_FILE" ]] && ! grep -Fqx 'format=1' "$STATE_FILE"; then
  echo "ERROR: unrecognized installer state: $STATE_FILE" >&2
  exit 1
fi
if [[ -f "$STATE_FILE" ]]; then
  binary_sha256="$(awk -F= '$1 == "binary_sha256" { print $2; exit }' "$STATE_FILE")"
  desktop_sha256="$(awk -F= '$1 == "desktop_sha256" { print $2; exit }' "$STATE_FILE")"
  if [[ -e "$INSTALL_BIN" && "$(sha256sum "$INSTALL_BIN" | awk '{print $1}')" != "$binary_sha256" ]]; then
    echo "ERROR: refusing to overwrite modified installer file: $INSTALL_BIN" >&2
    exit 1
  fi
  if [[ -e "$DESKTOP_FILE" && "$(sha256sum "$DESKTOP_FILE" | awk '{print $1}')" != "$desktop_sha256" ]]; then
    echo "ERROR: refusing to overwrite modified installer file: $DESKTOP_FILE" >&2
    exit 1
  fi
fi
if [[ -e "$CLI_LINK" || -L "$CLI_LINK" ]]; then
  if [[ ! -f "$STATE_FILE" || ! -L "$CLI_LINK" || "$(readlink -- "$CLI_LINK")" != "$INSTALL_BIN" ]]; then
    echo "ERROR: refusing to overwrite unrelated path: $CLI_LINK" >&2
    exit 1
  fi
fi
if [[ -e "$DESKTOP_FILE" && ! -f "$STATE_FILE" && "$REPLACE_DESKTOP" != true ]]; then
  echo "ERROR: user desktop entry already exists: $DESKTOP_FILE" >&2
  echo "Re-run with --replace-existing-desktop to confirm backup and replacement." >&2
  exit 1
fi

install -d -m 0755 "$INSTALL_DIR" "$DESKTOP_DIR" "$PREFIX/bin"
if [[ -e "$DESKTOP_FILE" && ! -f "$STATE_FILE" ]]; then
  install -m 0644 "$DESKTOP_FILE" "$DESKTOP_BACKUP"
fi

install -m 0755 "$DIST_DIR/ksnip" "$INSTALL_BIN"
install -m 0644 "$DIST_DIR/org.ksnip.ksnip.desktop" "$DESKTOP_FILE"
ln -sfn "$INSTALL_BIN" "$CLI_LINK"

state_tmp="$STATE_FILE.tmp"
{
  echo 'format=1'
  echo "binary_sha256=$(sha256sum "$INSTALL_BIN" | awk '{print $1}')"
  echo "desktop_sha256=$(sha256sum "$DESKTOP_FILE" | awk '{print $1}')"
} > "$state_tmp"
chmod 0600 "$state_tmp"
mv -f "$state_tmp" "$STATE_FILE"

if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database "$DESKTOP_DIR" ||
    echo "WARNING: desktop database update failed; files are installed." >&2
fi

echo "Installed binary: $INSTALL_BIN"
echo "Installed desktop entry: $DESKTOP_FILE"
echo "CLI command: $CLI_LINK"

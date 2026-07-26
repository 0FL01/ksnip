#!/usr/bin/env bash
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel)"
BUILD_DIR="${BUILD_DIR:-${ROOT}/build-agent}"
DIST_DIR="${DIST_DIR:-${ROOT}/dist}"
BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}"
BUILD_JOBS="${BUILD_JOBS:-2}"
BASELINE_SHA="${BASELINE_SHA:-62fa0ff6ec888125ce6dd592b5fb346658160ac5}"
PACK_DIR="$ROOT/ksnip-wayland-agent-context-pack"
INSTALL_BIN="${HOME:?HOME is not set}/.local/libexec/ksnip-wayland/ksnip"
CONFIGURE_COMMAND="cmake -S $ROOT -B $BUILD_DIR -G Ninja -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DBUILD_WITH_QT6=ON -DUSE_SUBMODULE_KCOLORPICKER=ON -DUSE_SUBMODULE_KIMAGEANNOTATOR=ON"
BUILD_COMMAND="cmake --build $BUILD_DIR --parallel $BUILD_JOBS"

cd "$ROOT"
git submodule update --init --recursive

cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DBUILD_WITH_QT6=ON \
  -DUSE_SUBMODULE_KCOLORPICKER=ON \
  -DUSE_SUBMODULE_KIMAGEANNOTATOR=ON

cmake --build "$BUILD_DIR" --parallel "$BUILD_JOBS"

BIN="$BUILD_DIR/src/ksnip"
if [[ ! -x "$BIN" ]]; then
  echo "ERROR: built ksnip executable was not found at $BIN" >&2
  exit 1
fi

mkdir -p "$DIST_DIR"
install -m 0755 "$BIN" "$DIST_DIR/ksnip"
install -m 0755 "$PACK_DIR/scripts/install-user.sh" "$DIST_DIR/install-user.sh"
install -m 0755 "$PACK_DIR/scripts/uninstall-user.sh" "$DIST_DIR/uninstall-user.sh"
install -m 0644 "$PACK_DIR/test-report.md" "$DIST_DIR/test-report.md"
sed "s|@KSNIP_EXEC@|$INSTALL_BIN|g" \
  "$PACK_DIR/org.ksnip.ksnip.desktop.in" > "$DIST_DIR/org.ksnip.ksnip.desktop"
chmod 0644 "$DIST_DIR/org.ksnip.ksnip.desktop"

SOURCE_PATCH="$DIST_DIR/source.patch"
git diff --binary "$BASELINE_SHA" -- . \
  ':!dist' ':!ksnip-wayland-agent-context-pack' > "$SOURCE_PATCH"
while IFS= read -r source_file; do
  git diff --binary --no-index /dev/null "$source_file" >> "$SOURCE_PATCH" || [[ "$?" -eq 1 ]]
done < <(git ls-files --others --exclude-standard -- src tests)

{
  echo "build_utc=$(date -u +%FT%TZ)"
  echo "repository_url=$(git remote get-url origin)"
  echo "source_sha=$(git rev-parse HEAD)"
  echo "baseline_sha=$BASELINE_SHA"
  echo "source_status=$(if [[ -n "$(git status --porcelain=v1)" ]]; then echo dirty; else echo clean; fi)"
  echo "build_type=$BUILD_TYPE"
  echo "compiler=$(c++ --version | sed -n '1p')"
  echo "cmake=$(cmake --version | sed -n '1p')"
  echo "qt=$(qmake6 -query QT_VERSION)"
  echo "kernel=$(uname -srmo)"
  echo "fedora=$(cat /etc/fedora-release)"
  echo "plasma=$(plasmashell --version 2>&1)"
  echo "session_type=${XDG_SESSION_TYPE:-}"
  echo "current_desktop=${XDG_CURRENT_DESKTOP:-}"
  echo "configure_command=$CONFIGURE_COMMAND"
  echo "build_command=$BUILD_COMMAND"
  echo "binary_source=$BIN"
  echo "binary_dist=$DIST_DIR/ksnip"
  echo
  echo "== file =="
  file "$DIST_DIR/ksnip"
  echo
  echo "== version =="
  "$DIST_DIR/ksnip" --version
  echo
  echo "== ldd =="
  ldd "$DIST_DIR/ksnip"
  echo
  echo "== source status =="
  git status --short
} > "$DIST_DIR/build-info.txt"

(
  cd "$DIST_DIR"
  sha256sum ksnip install-user.sh uninstall-user.sh \
    org.ksnip.ksnip.desktop build-info.txt test-report.md source.patch \
    > SHA256SUMS
)

echo "Built delivery directory: $DIST_DIR"

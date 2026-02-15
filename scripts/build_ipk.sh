#!/bin/sh
set -eu

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
PKG_NAME="awg-manager-alpha"
VERSION="${1:-0.1.0-1}"
ARCH="${2:-aarch64-3.10}"

BIN_SRC="$ROOT_DIR/build/awg-manager-alpha"
INIT_SRC="$ROOT_DIR/init.d/S99awg-manager-alpha"
WEB_SRC="$ROOT_DIR/web"
KMOD_SRC="$ROOT_DIR/amneziawg.ko"
UPDATER_SRC="$ROOT_DIR/scripts/updater.sh"
CONTROL_TEMPLATE="$ROOT_DIR/packaging/ipk/CONTROL/control.in"
POSTINST_SRC="$ROOT_DIR/packaging/ipk/CONTROL/postinst"
PRERM_SRC="$ROOT_DIR/packaging/ipk/CONTROL/prerm"

DIST_DIR="$ROOT_DIR/dist"
WORK_BASE="${TMPDIR:-/tmp}"
WORK_DIR="$WORK_BASE/${PKG_NAME}-ipk-work.$$"
CONTROL_DIR="$WORK_DIR/control"
DATA_DIR="$WORK_DIR/data"
PKG_FILE="$DIST_DIR/${PKG_NAME}_${VERSION}_${ARCH}.ipk"

cleanup() {
    rm -rf "$WORK_DIR"
}

trap cleanup EXIT INT TERM

require_file() {
    if [ ! -f "$1" ]; then
        echo "missing required file: $1" >&2
        exit 1
    fi
}

require_dir() {
    if [ ! -d "$1" ]; then
        echo "missing required directory: $1" >&2
        exit 1
    fi
}

require_file "$BIN_SRC"
require_file "$INIT_SRC"
require_file "$CONTROL_TEMPLATE"
require_file "$POSTINST_SRC"
require_file "$PRERM_SRC"
require_file "$KMOD_SRC"
require_file "$UPDATER_SRC"
require_dir "$WEB_SRC"

if command -v file >/dev/null 2>&1; then
    BIN_DESC="$(file -b "$BIN_SRC" 2>/dev/null || true)"
    case "$ARCH" in
    aarch64*|arm64*)
        if ! printf "%s" "$BIN_DESC" | grep -qiE "aarch64|arm64"; then
            echo "binary/package arch mismatch: binary is '$BIN_DESC', package arch is '$ARCH'" >&2
            echo "build arm64 binary first (cross-compile or compile on router)" >&2
            exit 1
        fi
        ;;
    x86_64*|amd64*|x64*)
        if ! printf "%s" "$BIN_DESC" | grep -qiE "x86[-_ ]64|amd64"; then
            echo "binary/package arch mismatch: binary is '$BIN_DESC', package arch is '$ARCH'" >&2
            exit 1
        fi
        ;;
    esac
fi

mkdir -p "$DIST_DIR"
mkdir -p "$CONTROL_DIR" "$DATA_DIR/opt/bin" "$DATA_DIR/opt/etc/init.d" \
    "$DATA_DIR/opt/share/awg-manager-alpha/www" "$DATA_DIR/opt/lib/modules" \
    "$DATA_DIR/opt/libexec/awg-manager-alpha"

sed "s/@VERSION@/$VERSION/g; s/@ARCH@/$ARCH/g" "$CONTROL_TEMPLATE" >"$CONTROL_DIR/control"
cp "$POSTINST_SRC" "$CONTROL_DIR/postinst"
cp "$PRERM_SRC" "$CONTROL_DIR/prerm"

cp "$BIN_SRC" "$DATA_DIR/opt/bin/awg-manager-alpha"
cp "$INIT_SRC" "$DATA_DIR/opt/etc/init.d/S99awg-manager-alpha"
cp -R "$WEB_SRC/." "$DATA_DIR/opt/share/awg-manager-alpha/www/"
cp "$KMOD_SRC" "$DATA_DIR/opt/lib/modules/amneziawg.ko"
cp "$UPDATER_SRC" "$DATA_DIR/opt/libexec/awg-manager-alpha/updater.sh"

chmod 0755 "$CONTROL_DIR/postinst" "$CONTROL_DIR/prerm"
chmod 0755 "$DATA_DIR/opt/bin/awg-manager-alpha" "$DATA_DIR/opt/etc/init.d/S99awg-manager-alpha"
chmod 0755 "$DATA_DIR/opt/libexec/awg-manager-alpha/updater.sh"
chmod 0644 "$DATA_DIR/opt/lib/modules/amneziawg.ko"

printf "2.0\n" >"$WORK_DIR/debian-binary"

(
    cd "$CONTROL_DIR"
    tar -czf "$WORK_DIR/control.tar.gz" .
)

(
    cd "$DATA_DIR"
    tar -czf "$WORK_DIR/data.tar.gz" .
)

rm -f "$PKG_FILE"
(
    cd "$WORK_DIR"
    tar -czf "$PKG_FILE" debian-binary control.tar.gz data.tar.gz
)

echo "created: $PKG_FILE"

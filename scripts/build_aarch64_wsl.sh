#!/bin/sh
set -eu

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
ARCH="${1:-aarch64-3.10}"
VERSION="${2:-0.1.0-1}"

need_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "missing command: $1" >&2
        exit 1
    fi
}

find_zig() {
    if command -v zig >/dev/null 2>&1; then
        command -v zig
        return 0
    fi
    if [ -x /snap/bin/zig ]; then
        printf "%s\n" "/snap/bin/zig"
        return 0
    fi
    return 1
}

need_cmd file
need_cmd make

cd "$ROOT_DIR"

echo "[1/5] Running host unit tests..."
make test

echo "[2/5] Building aarch64 binary..."
make clean
mkdir -p build dist
if ZIG_BIN="$(find_zig)"; then
    "$ZIG_BIN" cc -target aarch64-linux-musl -Os -Wall -Wextra -Wpedantic -std=c11 -D_GNU_SOURCE \
        -static -s src/main.c src/config.c src/session.c src/hash.c src/router_auth.c src/server.c \
        -o build/awg-manager-alpha
else
    need_cmd aarch64-linux-gnu-gcc
    need_cmd aarch64-linux-gnu-strip
    echo "      zig not found: using gcc-aarch64-linux-gnu (possible glibc mismatch on Entware)"
    make CC=aarch64-linux-gnu-gcc STRIP=aarch64-linux-gnu-strip
    aarch64-linux-gnu-strip --strip-unneeded build/awg-manager-alpha || true
fi

BIN_DESC="$(file -b build/awg-manager-alpha 2>/dev/null || true)"
echo "      $BIN_DESC"
if ! printf "%s" "$BIN_DESC" | grep -qiE "aarch64|arm64"; then
    echo "build failed: output binary is not aarch64" >&2
    exit 1
fi

if command -v upx >/dev/null 2>&1; then
    echo "[3/5] Compressing with UPX..."
    upx --best --lzma build/awg-manager-alpha >/dev/null 2>&1 || true
else
    echo "[3/5] UPX not found, skipping compression"
fi

echo "[4/5] Building IPK package..."
./scripts/build_ipk.sh "$VERSION" "$ARCH"

echo "[5/5] Done"
echo "      dist/awg-manager-alpha_${VERSION}_${ARCH}.ipk"

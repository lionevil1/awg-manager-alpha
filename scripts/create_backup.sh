#!/bin/sh
set -eu

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
BACKUP_DIR="$ROOT_DIR/backups"
LABEL="${1:-checkpoint}"
STAMP="$(date +%Y%m%d-%H%M%S)"
ARCHIVE="$BACKUP_DIR/awg-manager-alpha_${LABEL}_${STAMP}.tar.gz"

mkdir -p "$BACKUP_DIR"

(
    cd "$ROOT_DIR"
    tar -czf "$ARCHIVE" \
        --exclude='./build' \
        --exclude='./backups' \
        .
)

echo "created backup: $ARCHIVE"

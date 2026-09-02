#!/bin/sh
# Cài git hook để phiên bản tự tăng mỗi khi commit mã nguồn.
set -e

ROOT=$(git rev-parse --show-toplevel)
HOOK_DIR="$ROOT/.git/hooks"

mkdir -p "$HOOK_DIR"
cp "$ROOT/scripts/hooks/pre-commit" "$HOOK_DIR/pre-commit"
chmod +x "$HOOK_DIR/pre-commit"

echo "Đã cài hook pre-commit vào $HOOK_DIR"
echo "Từ giờ mỗi commit chạm vào src/ sẽ tự tăng số patch trong VERSION.txt."

#!/bin/bash
# Áp ghi đè module lên cây mã nguồn LibreOffice. Dùng: apply-overrides.sh <thư mục core>
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
core="$1"
echo "== Ghi đè external/fontconfig (từ distro/collabora/co-26.04)"
rm -rf "$core/external/fontconfig"
cp -r "$here/overrides/external/fontconfig" "$core/external/fontconfig"
# Thay hai dòng FONTCONFIG_SHA256SUM / FONTCONFIG_TARBALL trong download.lst
tmp="$(mktemp)"
grep -v '^FONTCONFIG_' "$core/download.lst" > "$tmp"
sed 's/^[0-9]*://' "$here/overrides/download.lst.fontconfig" >> "$tmp"
mv "$tmp" "$core/download.lst"
grep -n '^FONTCONFIG_' "$core/download.lst"

#!/usr/bin/env bash
set -euo pipefail
# shellcheck source=scripts/lib/common.sh
source "$(cd "$(dirname "$0")" && pwd)/lib/common.sh"

if [[ "$(uname -s)" != Linux || "$(uname -m)" != x86_64 ]]; then
  echo "Automatic m68k toolchain bootstrap currently supports Linux x86_64." >&2
  exit 2
fi
command -v curl >/dev/null || { echo "curl is required" >&2; exit 2; }
command -v tar >/dev/null || { echo "tar is required" >&2; exit 2; }

if find "$TOOLCHAIN_HOME" -type f -path '*/bin/m68k-elf-gcc' -perm -111 -print -quit 2>/dev/null | grep -q .; then
  echo "m68k toolchain already installed: $TOOLCHAIN_HOME"
  exit 0
fi

ensure_dir "$DEPS_DIR/downloads"
archive="$DEPS_DIR/downloads/$M68K_TOOLCHAIN_ASSET"
url="https://github.com/iratahack/m68k-elf-gcc/releases/download/$M68K_TOOLCHAIN_VERSION/$M68K_TOOLCHAIN_ASSET"
if [[ ! -f "$archive" ]]; then
  echo "Downloading m68k-elf toolchain $M68K_TOOLCHAIN_VERSION..."
  curl --fail --location --retry 3 --output "$archive" "$url"
fi

rm -rf "$TOOLCHAIN_HOME"
ensure_dir "$TOOLCHAIN_HOME"
tar xzf "$archive" -C "$TOOLCHAIN_HOME"
GCC_PATH="$(find "$TOOLCHAIN_HOME" -type f -path '*/bin/m68k-elf-gcc' -perm -111 -print -quit)"
[[ -n "$GCC_PATH" ]] || { echo "m68k-elf-gcc was not found after extraction" >&2; exit 3; }
TOOLCHAIN_BIN="$(dirname "$GCC_PATH")"
printf '%s\n' "$TOOLCHAIN_BIN" > "$TOOLCHAIN_HOME/.bin-path"
echo "Installed m68k toolchain: $TOOLCHAIN_BIN"

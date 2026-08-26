#!/usr/bin/env bash
set -euo pipefail
ROM="$1"; OUTBASE="$2"
mkdir -p "$(dirname "$OUTBASE")"
ld -r -b binary "$ROM" -o "${OUTBASE}.o"
START_SYM="$(nm -g "${OUTBASE}.o" | awk '$3 ~ /_start$/ {print $3; exit}')"
[ -n "$START_SYM" ] || { echo "could not find binary start symbol" >&2; exit 2; }
objcopy --redefine-sym "${START_SYM}=rom_data" "${OUTBASE}.o"
SIZE="$(stat -c %s "$ROM")"
cat > "${OUTBASE}_size.c" <<EOF
#include <stddef.h>
const size_t rom_size = ${SIZE}u;
EOF
echo "host ROM blob: $ROM -> ${OUTBASE}.o (${SIZE} bytes)"

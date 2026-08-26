#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "$0")/.." && pwd)/scripts/lib/common.sh"
cd "$PROJECT_ROOT"
mkdir -p "$BUILD_DIR/mapper_size"
: > "$BUILD_DIR/mapper_size.log"
for p in mbc1 mbc3 mbc5; do
  pref="${p}test"; dir="$(sgdk_generated_dir "$pref")"; out="$BUILD_DIR/mapper_size/${p}"
  mkdir -p "$out"
  [[ -f "$dir/${pref}_rom.c" ]] || { echo "Missing generated source for $pref; run make ${p}-test first" >&2; exit 2; }
  core=$(find "$dir" -maxdepth 1 \( -name "${pref}.c" -o -name "${pref}_funcs_*.c" -o -name "${pref}_dispatch_chunk_*.c" \) -printf '%s\n' | awk '{s+=$1}END{print s+0}')
  romc=$(stat -c %s "$dir/${pref}_rom.c")
  rm -f "$out"/*.o
  for f in "$dir/${pref}.c" "$dir/${pref}"_funcs_*.c "$dir/${pref}"_dispatch_chunk_*.c; do
    gcc -std=c11 -Os -ffunction-sections -fdata-sections -Wno-unused-label -Isgdk_runtime/include -Isrc -I"$dir" -c "$f" -o "$out/$(basename "${f%.c}").o"
  done
  text=$(size "$out"/*.o | awk 'NR>1 && $1 ~ /^[0-9]+$/ {t+=$1} END{print t+0}')
  rombytes=$(stat -c %s "$FIXTURES_DIR/${pref}.gb")
  printf '%s rom_bytes=%s core_c_bytes=%s rom_c_bytes=%s x86_Os_text=%s\n' "$p" "$rombytes" "$core" "$romc" "$text" | tee -a "$BUILD_DIR/mapper_size.log"
done

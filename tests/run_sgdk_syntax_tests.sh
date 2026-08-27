#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "$0")/.." && pwd)/scripts/lib/common.sh"
cd "$PROJECT_ROOT"
mkdir -p "$BUILD_DIR/syntax"
CC="${CC:-gcc}"
CFLAGS=(-std=c11 -O2 -Wall -Wextra -Werror -pedantic)

for game in cakegame mbc1test mbc3test mbc5test; do
  target="$(sgdk_generated_dir "$game")"
  [[ -f "$target/$game.c" ]] || "$PROJECT_ROOT/scripts/generate.sh" "$game" >/dev/null
done

render_main() {
  local game="$1" out="$2"
  python3 - "sgdk/template/src/main.c" "$out" "$game" <<'PY'
from pathlib import Path
import re, sys
src=Path(sys.argv[1]); dst=Path(sys.argv[2]); game=sys.argv[3]
s=src.read_text()
s=re.sub(r'#include "(?:basicdemo|cakegame|mbc1test|mbc3test|mbc5test|irqtest|timertest)\.h"', f'#include "{game}.h"', s)
s=re.sub(r'(?:basicdemo|cakegame|mbc1test|mbc3test|mbc5test|irqtest|timertest)_init\(&gbctx\);', f'{game}_init(&gbctx);', s)
s=re.sub(r'gbrt_sgdk_run_frame\(&gbctx, (?:basicdemo|cakegame|mbc1test|mbc3test|mbc5test|irqtest|timertest)_run\);', f'gbrt_sgdk_run_frame(&gbctx, {game}_run);', s)
dst.write_text(s)
PY
}

CAKE="$(sgdk_generated_dir cakegame)"
render_main cakegame "$BUILD_DIR/syntax/main_cakegame.c"
"$CC" "${CFLAGS[@]}" -Wno-main -Itests/sgdk_stub -Isgdk_runtime/include -Isrc -I"$CAKE" -c "$BUILD_DIR/syntax/main_cakegame.c" -o "$BUILD_DIR/syntax/main_cakegame.o"
"$CC" "${CFLAGS[@]}" -Wno-unused-label -Isgdk_runtime/include -Isrc -I"$CAKE" -c "$CAKE/cakegame.c" -o "$BUILD_DIR/syntax/cakegame_dispatch.o"
if nm -u "$BUILD_DIR/syntax/cakegame_dispatch.o" | grep -Eq 'fprintf|stderr|exit'; then echo 'desktop libc diagnostics leaked into SGDK dispatch' >&2; exit 1; fi

MBC1="$(sgdk_generated_dir mbc1test)"
render_main mbc1test "$BUILD_DIR/syntax/main_mbc1test.c"
"$CC" "${CFLAGS[@]}" -DGBRT_SGDK_USE_CART_SRAM -Wno-main -Itests/sgdk_stub -Isgdk_runtime/include -Isrc -I"$MBC1" -c "$BUILD_DIR/syntax/main_mbc1test.c" -o "$BUILD_DIR/syntax/main_mbc1test.o"
"$CC" "${CFLAGS[@]}" -DGBRT_SGDK_USE_CART_SRAM -Itests/sgdk_stub -Isgdk_runtime/include -Isrc -c sgdk_runtime/src/gbrt_sgdk_min.c -o "$BUILD_DIR/syntax/gbrt_mbc1_sram.o"
"$CC" "${CFLAGS[@]}" -DGBRT_SGDK_USE_CART_SRAM -Itests/sgdk_stub -Isgdk_runtime/include -Isrc -c sgdk_runtime/src/gbrt_cpu.c -o "$BUILD_DIR/syntax/gbrt_cpu.o"
for symbol in gb_add8 gb_stop gb_rrca gb_daa gb_write16 gbrt_timed_bus_read8 gbrt_timed_hl_read_auto gbrt_timed_push16 gbrt_timed_pop16 gbrt_timed_rst; do
  if ! nm "$BUILD_DIR/syntax/gbrt_cpu.o" "$BUILD_DIR/syntax/gbrt_mbc1_sram.o" | grep -Eq " [Tt] ${symbol}$"; then
    echo "generated CPU helper missing from SGDK runtime: $symbol" >&2
    exit 1
  fi
done

"$CC" "${CFLAGS[@]}" -DGBRT_SGDK_USE_CART_SRAM -Itests/sgdk_stub -Isgdk_runtime/include -Isrc -c sgdk_runtime/src/gbrt_sgdk_min.c -o "$BUILD_DIR/syntax/gbrt_mbc35_sram.o"
python3 tools/make_sgdk_rom_blob.py --rom fixtures/mbc3test.gb --out "$BUILD_DIR/syntax/mbc3_rom_blob.s" --incbin-path fixtures/mbc3test.gb
as "$BUILD_DIR/syntax/mbc3_rom_blob.s" -o "$BUILD_DIR/syntax/mbc3_rom_blob.o"

"$CC" "${CFLAGS[@]}" -DGBRT_SGDK_USE_CART_SRAM -DGBRT_SGDK_USE_FAR_ROM -Itests/sgdk_stub -Isgdk_runtime/include -Isrc -c sgdk_runtime/src/gbrt_sgdk_min.c -o "$BUILD_DIR/syntax/gbrt_far_rom.o"
python3 tools/make_sgdk_rom_blob.py --rom fixtures/mbc5test.gb --out "$BUILD_DIR/syntax/mbc5_far_rom_blob.s" --incbin-path fixtures/mbc5test.gb
python3 - "$BUILD_DIR/syntax/mbc5_far_rom_blob.s" <<'PY'
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text()
assert '.section .rodata_binf' in s
assert s.index('rom_size:') < s.index('rom_data:')
PY

# Target-style header mode: SGDK owns bool/stdint/size_t aliases.
"$CC" "${CFLAGS[@]}" -DSGDK_GCC -DGBRT_SGDK_USE_CART_SRAM -Itests/sgdk_stub -Isgdk_runtime/include -Isrc -c sgdk_runtime/src/gbrt_sgdk_min.c -o "$BUILD_DIR/syntax/gbrt_sgdk_target.o"
"$CC" "${CFLAGS[@]}" -DSGDK_GCC -DGBRT_SGDK_USE_CART_SRAM -Itests/sgdk_stub -Isgdk_runtime/include -Isrc -c sgdk_runtime/src/gbrt_cpu.c -o "$BUILD_DIR/syntax/gbrt_cpu_sgdk_target.o"
"$CC" "${CFLAGS[@]}" -DSGDK_GCC -Itests/sgdk_stub -Isrc -c src/gbmd_backend.c -o "$BUILD_DIR/syntax/gbmd_sgdk_target.o"

echo "SGDK syntax gates passed."

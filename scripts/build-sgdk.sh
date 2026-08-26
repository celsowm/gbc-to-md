#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "$0")" && pwd)/lib/common.sh"
cd "$PROJECT_ROOT"

GAME="${GAME:-${1:-cakegame}}"
SGDK="${SGDK:-$SGDK_HOME}"
PREFIX="${PREFIX:-m68k-elf-}"
GOAL="${SGDK_BUILD:-release}"

[[ -f "$SGDK/makefile.gen" ]] || { echo "SGDK not found at $SGDK. Run: make bootstrap-sgdk" >&2; exit 2; }

if ! command -v "${PREFIX}gcc" >/dev/null 2>&1; then
  if [[ -f "$TOOLCHAIN_HOME/.bin-path" ]]; then
    export PATH="$(cat "$TOOLCHAIN_HOME/.bin-path"):$PATH"
  fi
fi
for tool in gcc objcopy nm; do
  command -v "${PREFIX}${tool}" >/dev/null || { echo "Missing ${PREFIX}${tool}. Run: make bootstrap-sgdk" >&2; exit 2; }
done
command -v java >/dev/null || { echo "Java is required by SGDK" >&2; exit 2; }

case "$GAME" in
  basicdemo|irqtest|timertest|cakegame|mbc1test|mbc3test|mbc5test) PREFIX_NAME="$GAME" ;;
  *) echo "Unsupported GAME=$GAME" >&2; exit 2 ;;
esac

TARGET="$(sgdk_generated_dir "$PREFIX_NAME")"
ROM="$FIXTURES_DIR/$PREFIX_NAME.gb"
if [[ ! -f "$TARGET/$PREFIX_NAME.c" || ! -f "$ROM" ]]; then
  "$PROJECT_ROOT/scripts/generate.sh" "$GAME" >/dev/null
fi

ROM_BYTES="$(stat -c %s "$ROM")"
USE_FAR_ROM=0
if (( ROM_BYTES > 4194304 )); then USE_FAR_ROM=1; fi
if (( USE_FAR_ROM )); then
  if ! grep -Eq '^[[:space:]]*#define[[:space:]]+ENABLE_BANK_SWITCH[[:space:]]+1([[:space:]]|$)' "$SGDK/inc/config.h"; then
    echo "$GAME requires the official SEGA mapper because the retained guest ROM is larger than 4 MiB." >&2
    echo "Run: SGDK=$SGDK ./scripts/enable-sgdk-bank-switch.sh" >&2
    exit 3
  fi
fi

STAGE="$BUILD_DIR/sgdk/$GAME"
rm -rf "$STAGE"
mkdir -p "$STAGE/src"
cp src/gbmd_backend.c src/gbmd_backend.h "$STAGE/src/"
cp sgdk_runtime/src/gbrt_sgdk_min.c sgdk_runtime/include/gbrt.h "$STAGE/src/"
cp "$TARGET/$PREFIX_NAME.c" "$TARGET/$PREFIX_NAME.h" "$TARGET/${PREFIX_NAME}_internal.h" "$STAGE/src/"
cp "$TARGET"/${PREFIX_NAME}_funcs_*.c "$TARGET"/${PREFIX_NAME}_dispatch_chunk_*.c "$STAGE/src/"
cp "$ROM" "$STAGE/src/${PREFIX_NAME}.gb"
python3 tools/make_sgdk_rom_blob.py --rom "$ROM" --out "$STAGE/src/${PREFIX_NAME}_rom.s" --incbin-path "src/${PREFIX_NAME}.gb"

python3 - "sgdk/template/src/main.c" "$STAGE/src/main.c" "$PREFIX_NAME" <<'PY'
from pathlib import Path
import re, sys
src=Path(sys.argv[1]); dst=Path(sys.argv[2]); game=sys.argv[3]
s=src.read_text()
s=re.sub(r'#include "(?:basicdemo|cakegame|mbc1test|mbc3test|mbc5test|irqtest|timertest)\.h"', f'#include "{game}.h"', s)
s=re.sub(r'(?:basicdemo|cakegame|mbc1test|mbc3test|mbc5test|irqtest|timertest)_init\(&gbctx\);', f'{game}_init(&gbctx);', s)
s=re.sub(r'gbrt_sgdk_run_frame\(&gbctx, (?:basicdemo|cakegame|mbc1test|mbc3test|mbc5test|irqtest|timertest)_run\);', f'gbrt_sgdk_run_frame(&gbctx, {game}_run);', s)
dst.write_text(s)
PY

EXTRA_FLAGS="${EXTRA_FLAGS:-} -DGBRT_SGDK_USE_CART_SRAM"
if (( USE_FAR_ROM )); then EXTRA_FLAGS="$EXTRA_FLAGS -DGBRT_SGDK_USE_FAR_ROM"; fi

make -C "$STAGE" -f "$SGDK/makefile.gen" "$GOAL" GDK="$SGDK" PREFIX="$PREFIX" EXTRA_FLAGS="$EXTRA_FLAGS"

ELF="$STAGE/out/rom.out"
if (( USE_FAR_ROM )); then
  [[ -f "$ELF" ]] || { echo "Missing SGDK ELF for far-ROM layout check: $ELF" >&2; exit 4; }
  "${PREFIX}nm" -n "$ELF" > "$STAGE/out/far_rom_symbols.txt"
  python3 - "$STAGE/out/far_rom_symbols.txt" "$ROM_BYTES" <<'PY'
from pathlib import Path
import sys
rows=Path(sys.argv[1]).read_text(errors='replace').splitlines(); rom_bytes=int(sys.argv[2])
max_text=0; syms={}
for line in rows:
    p=line.split()
    if len(p) < 3: continue
    try: addr=int(p[0],16)
    except ValueError: continue
    typ,name=p[1],p[2]
    if typ in 'Tt': max_text=max(max_text,addr)
    if name in {'rom_size','rom_data'}: syms[name]=addr
for name in ('rom_size','rom_data'):
    if name not in syms: raise SystemExit(f'missing {name} in linked ELF')
if max_text >= 0x00380000:
    raise SystemExit(f'executable text reaches 0x{max_text:08X}; region 7 far-ROM window would evict code')
if syms['rom_size'] >= 0x00300000:
    raise SystemExit(f'rom_size is at 0x{syms["rom_size"]:08X}; startup must read it without mapper')
end=syms['rom_data'] + rom_bytes
if end > 0x02000000:
    raise SystemExit(f'guest ROM ends at logical 0x{end:08X}; official SEGA mapper limit is 32 MiB')
print(f'far-ROM layout OK: max_text=0x{max_text:06X} rom_size=0x{syms["rom_size"]:06X} rom_data=0x{syms["rom_data"]:06X} end=0x{end:06X}')
PY
fi

ARTIFACT_DIR="$BUILD_DIR/artifacts/$GAME"
mkdir -p "$ARTIFACT_DIR"
cp "$STAGE/out/rom.bin" "$ARTIFACT_DIR/rom.bin"
[[ -f "$ELF" ]] && cp "$ELF" "$ARTIFACT_DIR/rom.out"
[[ -f "$STAGE/out/symbol.txt" ]] && cp "$STAGE/out/symbol.txt" "$ARTIFACT_DIR/symbol.txt"
echo "SGDK build complete: $ARTIFACT_DIR/rom.bin"

#!/usr/bin/env bash
set -euo pipefail
# shellcheck source=scripts/lib/common.sh
source "$(cd "$(dirname "$0")/.." && pwd)/scripts/lib/common.sh"
cd "$PROJECT_ROOT"
ensure_gbrecomp

OUT="$(upstream_dir basicdemo)"
BUILD="$BUILD_DIR"
ROM="$FIXTURES_DIR/basicdemo.gb"
mkdir -p "$BUILD"
python3 "$FIXTURES_DIR/make_basicdemo.py"
rm -rf "$OUT"
mkdir -p "$OUT"

"$GBRECOMP" "$ROM" -o "$OUT" --output-prefix basicdemo --reachable-only --no-scan --no-comments -j 2 \
  | tee "$BUILD/gbrecomp.log"

CC="${CC:-gcc}"
COMMON_CFLAGS=(-std=c11 -O2 -Wall -Wextra -Wno-unused-label -Wno-dangling-pointer -D_POSIX_C_SOURCE=200809L -I"$OUT" -I"$OUT/runtime/include" -I"$PROJECT_ROOT/src")
GENERATED=("$OUT/basicdemo.c" "$OUT"/basicdemo_dispatch_chunk_*.c "$OUT"/basicdemo_funcs_*.c "$OUT/basicdemo_rom.c")
RUNTIME=(
  "$OUT/runtime/src/gbrt.c" "$OUT/runtime/src/gbrt_data_mod.c" "$OUT/runtime/src/gbrt_hash.c"
  "$OUT/runtime/src/gbrt_host_configuration.c" "$OUT/runtime/src/gbrt_port.c" "$OUT/runtime/src/gbrt_presentation.c"
  "$OUT/runtime/src/gbrt_semantic.c" "$OUT/runtime/src/differential.c" "$OUT/runtime/src/ppu.c"
  "$OUT/runtime/src/audio.c" "$OUT/runtime/src/audio_stats.c" "$OUT/runtime/src/interpreter.c"
)

"$CC" "${COMMON_CFLAGS[@]}" "${GENERATED[@]}" "${RUNTIME[@]}" tests/upstream_headless.c -lm -o "$BUILD/upstream_headless"
"$BUILD/upstream_headless" | tee "$BUILD/upstream_headless.log"
"$CC" "${COMMON_CFLAGS[@]}" "${GENERATED[@]}" "${RUNTIME[@]}" src/gbmd_backend.c tests/upstream_bridge.c -lm -o "$BUILD/upstream_bridge"
"$BUILD/upstream_bridge" | tee "$BUILD/upstream_bridge.log"

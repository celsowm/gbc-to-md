#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "$0")/.." && pwd)/scripts/lib/common.sh"
cd "$PROJECT_ROOT"; ensure_gbrecomp
OUT="$(upstream_dir cakegame)"; TARGET="$(sgdk_generated_dir cakegame)"; BUILD="$BUILD_DIR"; ROM="$FIXTURES_DIR/cakegame.gb"
mkdir -p "$BUILD"; python3 "$FIXTURES_DIR/make_cakegame.py"; rm -rf "$OUT" "$TARGET"; mkdir -p "$OUT"
"$GBRECOMP" "$ROM" -o "$OUT" --output-prefix cakegame --reachable-only --no-scan --no-comments -j 2 | tee "$BUILD/cake_gbrecomp.log"
./tools/prepare_sgdk_generated.py --src "$OUT" --dst "$TARGET" --prefix cakegame
CC="${CC:-gcc}"; COMMON=(-std=c11 -O2 -Wall -Wextra -Wno-unused-label -Wno-dangling-pointer -D_POSIX_C_SOURCE=200809L)
RUNTIME=("$OUT/runtime/src/gbrt.c" "$OUT/runtime/src/gbrt_data_mod.c" "$OUT/runtime/src/gbrt_hash.c" "$OUT/runtime/src/gbrt_host_configuration.c" "$OUT/runtime/src/gbrt_port.c" "$OUT/runtime/src/gbrt_presentation.c" "$OUT/runtime/src/gbrt_semantic.c" "$OUT/runtime/src/differential.c" "$OUT/runtime/src/ppu.c" "$OUT/runtime/src/audio.c" "$OUT/runtime/src/audio_stats.c" "$OUT/runtime/src/interpreter.c")
"$CC" "${COMMON[@]}" -I"$OUT" -I"$OUT/runtime/include" -Isrc "$OUT/cakegame.c" "$OUT"/cakegame_dispatch_chunk_*.c "$OUT"/cakegame_funcs_*.c "$OUT/cakegame_rom.c" "${RUNTIME[@]}" tests/cake_runtime/upstream_cake_headless.c -lm -o "$BUILD/upstream_cake_headless"
"$BUILD/upstream_cake_headless" | tee "$BUILD/upstream_cake_headless.log"
BASE=(-std=c11 -O2 -Wall -Wextra -Werror -pedantic -Wno-unused-label -Isgdk_runtime/include -Isrc -I"$TARGET")
GEN=("$TARGET/cakegame.c" "$TARGET"/cakegame_funcs_*.c "$TARGET"/cakegame_dispatch_chunk_*.c "$TARGET/cakegame_rom.c")
"$CC" "${BASE[@]}" src/gbmd_backend.c sgdk_runtime/src/gbrt_sgdk_min.c "${GEN[@]}" tests/cake_runtime/test_cake_runtime.c -o "$BUILD/cake_runtime_test"
"$BUILD/cake_runtime_test" | tee "$BUILD/cake_runtime_test.log"
"$CC" "${BASE[@]}" -DGBRT_SGDK_PROFILE src/gbmd_backend.c sgdk_runtime/src/gbrt_sgdk_min.c "${GEN[@]}" tests/cake_runtime/perf_cake_runtime.c -o "$BUILD/cake_perf"
"$BUILD/cake_perf" | tee "$BUILD/cake_perf.log"

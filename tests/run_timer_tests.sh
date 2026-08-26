#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "$0")/.." && pwd)/scripts/lib/common.sh"
cd "$PROJECT_ROOT"; ensure_gbrecomp
OUT="$(upstream_dir timertest)"; TARGET="$(sgdk_generated_dir timertest)"; BUILD="$BUILD_DIR"; ROM="$FIXTURES_DIR/timertest.gb"
mkdir -p "$BUILD"; python3 "$FIXTURES_DIR/make_timertest.py"; rm -rf "$OUT" "$TARGET"; mkdir -p "$OUT"
"$GBRECOMP" "$ROM" -o "$OUT" --output-prefix timertest --reachable-only --no-scan --no-comments -j 2 | tee "$BUILD/timer_gbrecomp.log"
./tools/prepare_sgdk_generated.py --src "$OUT" --dst "$TARGET" --prefix timertest
CC="${CC:-gcc}"; COMMON=(-std=c11 -O2 -Wall -Wextra -Wno-unused-label -Wno-dangling-pointer -D_POSIX_C_SOURCE=200809L)
RUNTIME=("$OUT/runtime/src/gbrt.c" "$OUT/runtime/src/gbrt_data_mod.c" "$OUT/runtime/src/gbrt_hash.c" "$OUT/runtime/src/gbrt_host_configuration.c" "$OUT/runtime/src/gbrt_port.c" "$OUT/runtime/src/gbrt_presentation.c" "$OUT/runtime/src/gbrt_semantic.c" "$OUT/runtime/src/differential.c" "$OUT/runtime/src/ppu.c" "$OUT/runtime/src/audio.c" "$OUT/runtime/src/audio_stats.c" "$OUT/runtime/src/interpreter.c")
"$CC" "${COMMON[@]}" -I"$OUT" -I"$OUT/runtime/include" -Isrc "$OUT/timertest.c" "$OUT"/timertest_dispatch_chunk_*.c "$OUT"/timertest_funcs_*.c "$OUT/timertest_rom.c" "${RUNTIME[@]}" tests/timer_runtime/upstream_timer_headless.c -lm -o "$BUILD/upstream_timer_headless"
"$BUILD/upstream_timer_headless" | tee "$BUILD/upstream_timer_headless.log"
BASE=(-std=c11 -O2 -Wall -Wextra -Werror -pedantic -Wno-unused-label -Isgdk_runtime/include -Isrc -I"$TARGET")
GEN=("$TARGET/timertest.c" "$TARGET"/timertest_funcs_*.c "$TARGET"/timertest_dispatch_chunk_*.c "$TARGET/timertest_rom.c")
"$CC" "${BASE[@]}" src/gbmd_backend.c sgdk_runtime/src/gbrt_sgdk_min.c "${GEN[@]}" tests/timer_runtime/test_timer_runtime.c -o "$BUILD/timer_runtime_test"
"$BUILD/timer_runtime_test" | tee "$BUILD/timer_runtime_test.log"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -pedantic -Isgdk_runtime/include -Isrc src/gbmd_backend.c sgdk_runtime/src/gbrt_sgdk_min.c tests/timer_runtime/test_timer_math.c -o "$BUILD/timer_math"
"$BUILD/timer_math" | tee "$BUILD/timer_math.log"

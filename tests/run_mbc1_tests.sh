#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "$0")/.." && pwd)/scripts/lib/common.sh"
cd "$PROJECT_ROOT"; ensure_gbrecomp
OUT="$(upstream_dir mbc1test)"; TARGET="$(sgdk_generated_dir mbc1test)"; BUILD="$BUILD_DIR"; ROM="$FIXTURES_DIR/mbc1test.gb"
mkdir -p "$BUILD"; python3 "$FIXTURES_DIR/make_mbc1test.py"; rm -rf "$OUT" "$TARGET"; mkdir -p "$OUT"
"$GBRECOMP" "$ROM" -o "$OUT" --output-prefix mbc1test --reachable-only --no-scan --no-comments -j 2 | tee "$BUILD/mbc1_gbrecomp.log"
./tools/prepare_sgdk_generated.py --src "$OUT" --dst "$TARGET" --prefix mbc1test
grep -q 'case 1: func_01_4000' "$OUT"/mbc1test_dispatch_chunk_*.c
grep -q 'case 2: func_02_4000' "$OUT"/mbc1test_dispatch_chunk_*.c
grep -q 'case 3: func_03_4000' "$OUT"/mbc1test_dispatch_chunk_*.c
CC="${CC:-gcc}"; UP=(-std=c11 -O2 -Wall -Wextra -Wno-unused-label -Wno-dangling-pointer -D_POSIX_C_SOURCE=200809L)
RUNTIME=("$OUT/runtime/src/gbrt.c" "$OUT/runtime/src/gbrt_data_mod.c" "$OUT/runtime/src/gbrt_hash.c" "$OUT/runtime/src/gbrt_host_configuration.c" "$OUT/runtime/src/gbrt_port.c" "$OUT/runtime/src/gbrt_presentation.c" "$OUT/runtime/src/gbrt_semantic.c" "$OUT/runtime/src/differential.c" "$OUT/runtime/src/ppu.c" "$OUT/runtime/src/audio.c" "$OUT/runtime/src/audio_stats.c" "$OUT/runtime/src/interpreter.c")
"$CC" "${UP[@]}" -I"$OUT" -I"$OUT/runtime/include" -Isrc "$OUT/mbc1test.c" "$OUT"/mbc1test_dispatch_chunk_*.c "$OUT"/mbc1test_funcs_*.c "$OUT/mbc1test_rom.c" "${RUNTIME[@]}" tests/mbc1_runtime/upstream_mbc1_headless.c -lm -o "$BUILD/upstream_mbc1_headless"
"$BUILD/upstream_mbc1_headless" | tee "$BUILD/upstream_mbc1_headless.log"
BASE=(-std=c11 -O2 -Wall -Wextra -Werror -pedantic -Wno-unused-label -Isgdk_runtime/include -Isrc -I"$TARGET")
GEN=("$TARGET/mbc1test.c" "$TARGET"/mbc1test_funcs_*.c "$TARGET"/mbc1test_dispatch_chunk_*.c "$TARGET/mbc1test_rom.c")
"$CC" "${BASE[@]}" src/gbmd_backend.c sgdk_runtime/src/gbrt_sgdk_min.c "${GEN[@]}" tests/mbc1_runtime/test_mbc1_runtime.c -o "$BUILD/mbc1_runtime_test"
"$BUILD/mbc1_runtime_test" | tee "$BUILD/mbc1_runtime_test.log"
"$CC" "${BASE[@]}" -Itests/sgdk_stub -DGBRT_SGDK_USE_CART_SRAM src/gbmd_backend.c sgdk_runtime/src/gbrt_sgdk_min.c tests/mbc1_runtime/fake_sram.c "${GEN[@]}" tests/mbc1_runtime/test_mbc1_runtime.c -o "$BUILD/mbc1_sram_runtime_test"
"$BUILD/mbc1_sram_runtime_test" | tee "$BUILD/mbc1_sram_runtime_test.log"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -pedantic -Isgdk_runtime/include -Isrc src/gbmd_backend.c sgdk_runtime/src/gbrt_sgdk_min.c tests/mbc1_runtime/test_mbc1_math.c -o "$BUILD/mbc1_math_test"
"$BUILD/mbc1_math_test" | tee "$BUILD/mbc1_math_test.log"

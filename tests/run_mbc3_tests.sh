#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "$0")/.." && pwd)/scripts/lib/common.sh"
cd "$PROJECT_ROOT"; ensure_gbrecomp
OUT="$(upstream_dir mbc3test)"; TARGET="$(sgdk_generated_dir mbc3test)"; BUILD="$BUILD_DIR"; ROM="$FIXTURES_DIR/mbc3test.gb"; ANN="$FIXTURES_DIR/mbc3test.annotations"
mkdir -p "$BUILD"; python3 "$FIXTURES_DIR/make_mbc3test.py"; rm -rf "$OUT" "$TARGET"; mkdir -p "$OUT"
"$GBRECOMP" "$ROM" -o "$OUT" --output-prefix mbc3test --reachable-only --no-scan --no-comments --annotations "$ANN" -j 4 | tee "$BUILD/mbc3_gbrecomp.log"
./tools/prepare_sgdk_generated.py --src "$OUT" --dst "$TARGET" --prefix mbc3test
[[ "$(grep -A140 'void mbc3test_dispatch_40' "$OUT"/mbc3test_dispatch_chunk_*.c | grep -c 'case [0-9].*: sym_Bank')" -ge 127 ]]
./tools/make_host_rom_blob.sh "$ROM" "$BUILD/mbc3_rom_blob"
CC="${CC:-gcc}"; UP=(-std=c11 -O2 -Wall -Wextra -Wno-unused-label -Wno-dangling-pointer -D_POSIX_C_SOURCE=200809L)
RUNTIME=("$OUT/runtime/src/gbrt.c" "$OUT/runtime/src/gbrt_data_mod.c" "$OUT/runtime/src/gbrt_hash.c" "$OUT/runtime/src/gbrt_host_configuration.c" "$OUT/runtime/src/gbrt_port.c" "$OUT/runtime/src/gbrt_presentation.c" "$OUT/runtime/src/gbrt_semantic.c" "$OUT/runtime/src/differential.c" "$OUT/runtime/src/ppu.c" "$OUT/runtime/src/audio.c" "$OUT/runtime/src/audio_stats.c" "$OUT/runtime/src/interpreter.c")
"$CC" "${UP[@]}" -I"$OUT" -I"$OUT/runtime/include" -Isrc "$OUT/mbc3test.c" "$OUT"/mbc3test_dispatch_chunk_*.c "$OUT"/mbc3test_funcs_*.c "$BUILD/mbc3_rom_blob_size.c" "$BUILD/mbc3_rom_blob.o" "${RUNTIME[@]}" tests/mbc3_runtime/upstream_mbc3_headless.c -lm -o "$BUILD/upstream_mbc3_headless"
"$BUILD/upstream_mbc3_headless" | tee "$BUILD/upstream_mbc3_headless.log"
BASE=(-std=c11 -O2 -Wall -Wextra -Werror -pedantic -Wno-unused-label -Isgdk_runtime/include -Isrc -I"$TARGET")
GEN=("$TARGET/mbc3test.c" "$TARGET"/mbc3test_funcs_*.c "$TARGET"/mbc3test_dispatch_chunk_*.c "$BUILD/mbc3_rom_blob_size.c" "$BUILD/mbc3_rom_blob.o")
"$CC" "${BASE[@]}" src/gbmd_backend.c sgdk_runtime/src/gbrt_sgdk_min.c "${GEN[@]}" tests/mbc3_runtime/test_mbc3_runtime.c -o "$BUILD/mbc3_runtime_test"
"$BUILD/mbc3_runtime_test" | tee "$BUILD/mbc3_runtime_test.log"
"$CC" "${BASE[@]}" -Itests/sgdk_stub -DGBRT_SGDK_USE_CART_SRAM src/gbmd_backend.c sgdk_runtime/src/gbrt_sgdk_min.c tests/mbc1_runtime/fake_sram.c "${GEN[@]}" tests/mbc3_runtime/test_mbc3_runtime.c -o "$BUILD/mbc3_sram_runtime_test"
"$BUILD/mbc3_sram_runtime_test" | tee "$BUILD/mbc3_sram_runtime_test.log"

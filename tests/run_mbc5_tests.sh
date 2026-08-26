#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "$0")/.." && pwd)/scripts/lib/common.sh"
cd "$PROJECT_ROOT"; ensure_gbrecomp
OUT="$(upstream_dir mbc5test)"; TARGET="$(sgdk_generated_dir mbc5test)"; BUILD="$BUILD_DIR"; ROM="$FIXTURES_DIR/mbc5test.gb"; ANN="$FIXTURES_DIR/mbc5test.annotations"
mkdir -p "$BUILD"; python3 "$FIXTURES_DIR/make_mbc5test.py"; rm -rf "$OUT" "$TARGET"; mkdir -p "$OUT"
"$GBRECOMP" "$ROM" -o "$OUT" --output-prefix mbc5test --reachable-only --no-scan --no-comments --annotations "$ANN" -j 8 | tee "$BUILD/mbc5_gbrecomp.log"
./tools/prepare_sgdk_generated.py --src "$OUT" --dst "$TARGET" --prefix mbc5test
[[ "$(grep -h 'case [0-9].*: sym_Bank' "$OUT"/mbc5test_dispatch_chunk_*.c | wc -l)" -ge 511 ]]
./tools/make_host_rom_blob.sh "$ROM" "$BUILD/mbc5_rom_blob"
CC="${CC:-gcc}"; UP=(-std=c11 -O2 -Wall -Wextra -Wno-unused-label -Wno-dangling-pointer -D_POSIX_C_SOURCE=200809L)
RUNTIME=("$OUT/runtime/src/gbrt.c" "$OUT/runtime/src/gbrt_data_mod.c" "$OUT/runtime/src/gbrt_hash.c" "$OUT/runtime/src/gbrt_host_configuration.c" "$OUT/runtime/src/gbrt_port.c" "$OUT/runtime/src/gbrt_presentation.c" "$OUT/runtime/src/gbrt_semantic.c" "$OUT/runtime/src/differential.c" "$OUT/runtime/src/ppu.c" "$OUT/runtime/src/audio.c" "$OUT/runtime/src/audio_stats.c" "$OUT/runtime/src/interpreter.c")
"$CC" "${UP[@]}" -I"$OUT" -I"$OUT/runtime/include" -Isrc "$OUT/mbc5test.c" "$OUT"/mbc5test_dispatch_chunk_*.c "$OUT"/mbc5test_funcs_*.c "$BUILD/mbc5_rom_blob_size.c" "$BUILD/mbc5_rom_blob.o" "${RUNTIME[@]}" tests/mbc5_runtime/upstream_mbc5_headless.c -lm -o "$BUILD/upstream_mbc5_headless"
"$BUILD/upstream_mbc5_headless" | tee "$BUILD/upstream_mbc5_headless.log"
BASE=(-std=c11 -O2 -Wall -Wextra -Werror -pedantic -Wno-unused-label -Isgdk_runtime/include -Isrc -I"$TARGET")
GEN=("$TARGET/mbc5test.c" "$TARGET"/mbc5test_funcs_*.c "$TARGET"/mbc5test_dispatch_chunk_*.c "$BUILD/mbc5_rom_blob_size.c" "$BUILD/mbc5_rom_blob.o")
"$CC" "${BASE[@]}" src/gbmd_backend.c sgdk_runtime/src/gbrt_sgdk_min.c "${GEN[@]}" tests/mbc5_runtime/test_mbc5_runtime.c -o "$BUILD/mbc5_runtime_test"
"$BUILD/mbc5_runtime_test" | tee "$BUILD/mbc5_runtime_test.log"
"$CC" "${BASE[@]}" -DGBRT_SGDK_USE_FAR_ROM -DGBRT_SGDK_FAR_ROM_HOST_TEST src/gbmd_backend.c sgdk_runtime/src/gbrt_sgdk_min.c "${GEN[@]}" tests/mbc5_runtime/test_mbc5_runtime.c -o "$BUILD/mbc5_far_runtime_test"
"$BUILD/mbc5_far_runtime_test" | tee "$BUILD/mbc5_far_runtime_test.log"
"$CC" "${BASE[@]}" -DGBRT_SGDK_USE_FAR_ROM -Itests/sgdk_stub src/gbmd_backend.c sgdk_runtime/src/gbrt_sgdk_min.c tests/mbc5_runtime/fake_mapper.c "${GEN[@]}" tests/mbc5_runtime/test_mbc5_far_target.c -o "$BUILD/mbc5_far_target_test"
"$BUILD/mbc5_far_target_test" | tee "$BUILD/mbc5_far_target_test.log"
"$CC" "${BASE[@]}" -Itests/sgdk_stub -DGBRT_SGDK_USE_CART_SRAM src/gbmd_backend.c sgdk_runtime/src/gbrt_sgdk_min.c tests/mbc1_runtime/fake_sram.c "${GEN[@]}" tests/mbc5_runtime/test_mbc5_runtime.c -o "$BUILD/mbc5_sram_runtime_test"
"$BUILD/mbc5_sram_runtime_test" | tee "$BUILD/mbc5_sram_runtime_test.log"

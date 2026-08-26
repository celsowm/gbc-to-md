#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "$0")/.." && pwd)/scripts/lib/common.sh"
cd "$PROJECT_ROOT"; ensure_gbrecomp
OUT="$(upstream_dir irqtest)"; TARGET="$(sgdk_generated_dir irqtest)"; BUILD="$BUILD_DIR"; ROM="$FIXTURES_DIR/irqtest.gb"
mkdir -p "$BUILD"; python3 "$FIXTURES_DIR/make_irqtest.py"; rm -rf "$OUT" "$TARGET"; mkdir -p "$OUT"
"$GBRECOMP" "$ROM" -o "$OUT" --output-prefix irqtest --reachable-only --no-scan --no-comments -j 2 | tee "$BUILD/irq_gbrecomp.log"
./tools/prepare_sgdk_generated.py --src "$OUT" --dst "$TARGET" --prefix irqtest
CC="${CC:-gcc}"; COMMON=(-std=c11 -O2 -Wall -Wextra -Wno-unused-label -Wno-dangling-pointer -D_POSIX_C_SOURCE=200809L)
RUNTIME=("$OUT/runtime/src/gbrt.c" "$OUT/runtime/src/gbrt_data_mod.c" "$OUT/runtime/src/gbrt_hash.c" "$OUT/runtime/src/gbrt_host_configuration.c" "$OUT/runtime/src/gbrt_port.c" "$OUT/runtime/src/gbrt_presentation.c" "$OUT/runtime/src/gbrt_semantic.c" "$OUT/runtime/src/differential.c" "$OUT/runtime/src/ppu.c" "$OUT/runtime/src/audio.c" "$OUT/runtime/src/audio_stats.c" "$OUT/runtime/src/interpreter.c")
"$CC" "${COMMON[@]}" -I"$OUT" -I"$OUT/runtime/include" -Isrc "$OUT/irqtest.c" "$OUT"/irqtest_dispatch_chunk_*.c "$OUT"/irqtest_funcs_*.c "$OUT/irqtest_rom.c" "${RUNTIME[@]}" tests/irq_runtime/upstream_irq_headless.c -lm -o "$BUILD/upstream_irq_headless"
"$BUILD/upstream_irq_headless" | tee "$BUILD/upstream_irq_headless.log"
BASE=(-std=c11 -O2 -Wall -Wextra -Werror -pedantic -Wno-unused-label -Isgdk_runtime/include -Isrc -I"$TARGET")
GEN=("$TARGET/irqtest.c" "$TARGET"/irqtest_funcs_*.c "$TARGET"/irqtest_dispatch_chunk_*.c "$TARGET/irqtest_rom.c")
"$CC" "${BASE[@]}" src/gbmd_backend.c sgdk_runtime/src/gbrt_sgdk_min.c "${GEN[@]}" tests/irq_runtime/test_irq_runtime.c -o "$BUILD/irq_runtime_test"
"$BUILD/irq_runtime_test" | tee "$BUILD/irq_runtime_test.log"
"$CC" "${BASE[@]}" -DGBRT_SGDK_PROFILE src/gbmd_backend.c sgdk_runtime/src/gbrt_sgdk_min.c "${GEN[@]}" tests/irq_runtime/perf_irq_runtime.c -o "$BUILD/irq_perf"
"$BUILD/irq_perf" | tee "$BUILD/irq_perf.log"
if nm -u "$BUILD/irq_runtime_test" | grep -Eq 'fprintf|stderr|exit'; then echo 'desktop diagnostics leaked into IRQ target' >&2; exit 1; fi

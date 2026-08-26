#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "$0")/.." && pwd)/scripts/lib/common.sh"
cd "$PROJECT_ROOT"
mkdir -p "$BUILD_DIR"
CC="${CC:-gcc}"
CFLAGS=(-std=c11 -O2 -Wall -Wextra -Werror -pedantic)

python3 "$FIXTURES_DIR/make_basicdemo.py"
"$CC" "${CFLAGS[@]}" -Isrc src/gbmd_backend.c src/basicdemo_generated.c tests/test_backend.c -o "$BUILD_DIR/test_backend"
"$BUILD_DIR/test_backend"
python3 tests/run_rom_smoke.py

TARGET="$(sgdk_generated_dir basicdemo)"
if [[ ! -f "$TARGET/basicdemo.c" ]]; then "$PROJECT_ROOT/scripts/generate.sh" basicdemo >/dev/null; fi
GEN=("$TARGET/basicdemo.c" "$TARGET"/basicdemo_funcs_*.c "$TARGET"/basicdemo_dispatch_chunk_*.c "$TARGET/basicdemo_rom.c")
BASE=("${CFLAGS[@]}" -Wno-unused-label -Isgdk_runtime/include -Isrc -I"$TARGET")
"$CC" "${BASE[@]}" src/gbmd_backend.c sgdk_runtime/src/gbrt_sgdk_min.c "${GEN[@]}" tests/min_runtime/test_min_runtime.c -o "$BUILD_DIR/min_runtime_test"
"$BUILD_DIR/min_runtime_test"
"$CC" "${BASE[@]}" -DGBRT_SGDK_PROFILE src/gbmd_backend.c sgdk_runtime/src/gbrt_sgdk_min.c "${GEN[@]}" tests/perf_runtime.c -o "$BUILD_DIR/perf_runtime"
"$BUILD_DIR/perf_runtime"

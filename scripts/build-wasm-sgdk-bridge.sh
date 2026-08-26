#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "$0")" && pwd)/lib/common.sh"
cd "$PROJECT_ROOT"

MODULE_JS="${MODULE_JS:-$BUILD_DIR/wasm/dist/gbrecomp_probe.js}"
ROM="${ROM:-$FIXTURES_DIR/basicdemo.gb}"
STAGE="${STAGE:-$BUILD_DIR/wasm-sgdk-bridge}"
SGDK="${SGDK:-$SGDK_HOME}"
PREFIX="${PREFIX:-m68k-elf-}"

[[ -f "$MODULE_JS" ]] || { echo "Missing WASM module glue: $MODULE_JS" >&2; exit 2; }
[[ -f "${MODULE_JS%.js}.wasm" ]] || { echo "Missing WASM module: ${MODULE_JS%.js}.wasm" >&2; exit 2; }
[[ -f "$ROM" ]] || { echo "Missing fixture ROM: $ROM" >&2; exit 2; }
[[ -f "$SGDK/makefile.gen" ]] || { echo "Missing SGDK: $SGDK" >&2; exit 2; }

if [[ -f "$TOOLCHAIN_HOME/.bin-path" ]]; then
  export PATH="$(cat "$TOOLCHAIN_HOME/.bin-path"):$PATH"
fi
for tool in gcc objcopy nm; do
  command -v "${PREFIX}${tool}" >/dev/null || { echo "Missing ${PREFIX}${tool}" >&2; exit 2; }
done
command -v node >/dev/null || { echo "Node.js is required for the bridge proof" >&2; exit 2; }
command -v java >/dev/null || { echo "Java is required by SGDK" >&2; exit 2; }

rm -rf "$STAGE"
node wasm/dump-sgdk-bundle.cjs "$MODULE_JS" "$ROM" "$STAGE"

cp src/gbmd_backend.c src/gbmd_backend.h "$STAGE/src/"
cp sgdk_runtime/src/gbrt_sgdk_min.c sgdk_runtime/include/gbrt.h "$STAGE/src/"
sed 's/cakegame/browserrom/g' sgdk/template/src/main.c > "$STAGE/src/main.c"

make -C "$STAGE" -f "$SGDK/makefile.gen" release \
  GDK="$SGDK" PREFIX="$PREFIX" EXTRA_FLAGS="-DGBRT_SGDK_USE_CART_SRAM"

[[ -s "$STAGE/out/rom.bin" ]] || { echo "Bridge SGDK build did not produce rom.bin" >&2; exit 3; }

mkdir -p "$BUILD_DIR/artifacts/wasm-bridge"
cp "$STAGE/out/rom.bin" "$BUILD_DIR/artifacts/wasm-bridge/rom.bin"
cp "$STAGE/out/rom.out" "$BUILD_DIR/artifacts/wasm-bridge/rom.out"
[[ -f "$STAGE/out/symbol.txt" ]] && cp "$STAGE/out/symbol.txt" "$BUILD_DIR/artifacts/wasm-bridge/symbol.txt"

printf 'PASS: WASM-generated SGDK bundle linked into real Mega Drive ROM\n'
wc -c "$BUILD_DIR/artifacts/wasm-bridge/rom.bin"
sha256sum "$BUILD_DIR/artifacts/wasm-bridge/rom.bin"

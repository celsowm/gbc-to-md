#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
. ./versions.env

command -v emcmake >/dev/null || { echo 'emcmake is required (Emscripten SDK).' >&2; exit 1; }
command -v em++ >/dev/null || { echo 'em++ is required (Emscripten SDK).' >&2; exit 1; }
command -v ninja >/dev/null || { echo 'ninja is required.' >&2; exit 1; }

SRC="$ROOT/build/wasm/gb-recompiled"
UPSTREAM_BUILD="$ROOT/build/wasm/upstream"
OUT="$ROOT/build/wasm/dist"
rm -rf "$SRC" "$UPSTREAM_BUILD" "$OUT"
mkdir -p "$ROOT/build/wasm" "$OUT"

git clone --depth 1 --branch "$GBRECOMP_VERSION" https://github.com/arcanite24/gb-recompiled.git "$SRC"

emcmake cmake -G Ninja -S "$SRC" -B "$UPSTREAM_BUILD" \
  -DBUILD_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Release
ninja -C "$UPSTREAM_BUILD" gbrecomp_core

CORE_LIB="$UPSTREAM_BUILD/lib/libgbrecomp_core.a"
[[ -f "$CORE_LIB" ]] || { echo "gbrecomp_core archive not found: $CORE_LIB" >&2; exit 1; }

em++ -std=c++20 -O2 \
  -I"$SRC/recompiler/include" \
  wasm/probe.cpp wasm/compiler.cpp "$CORE_LIB" \
  -sMODULARIZE=1 \
  -sEXPORT_NAME=createGBRecompProbe \
  -sENVIRONMENT=web,node \
  -sALLOW_MEMORY_GROWTH=1 \
  -sFILESYSTEM=0 \
  -sEXPORTED_FUNCTIONS='["_gbrecomp_wasm_probe","_gbrecomp_wasm_compile","_gbrecomp_wasm_file_count","_gbrecomp_wasm_file_name_ptr","_gbrecomp_wasm_file_name_size","_gbrecomp_wasm_file_data_ptr","_gbrecomp_wasm_file_data_size","_gbrecomp_wasm_error_ptr","_gbrecomp_wasm_error_size","_malloc","_free"]' \
  -sEXPORTED_RUNTIME_METHODS='["HEAPU8"]' \
  --no-entry \
  -o "$OUT/gbrecomp_probe.js"

printf 'WASM probe built:\n'
ls -lh "$OUT/gbrecomp_probe.js" "$OUT/gbrecomp_probe.wasm"

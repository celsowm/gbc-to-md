#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LLVM_TAG="${LLVM_TAG:-llvmorg-16.0.4}"
BUILD_ROOT="${LLVM_WASM_BUILD_ROOT:-$ROOT/build/llvm-m68k-wasm}"
SRC="$BUILD_ROOT/llvm-project"
HOST="$BUILD_ROOT/host"
WASM="$BUILD_ROOT/wasm"
DIST="$BUILD_ROOT/dist"
CMAKE_BIN="${CMAKE_BIN:-/usr/bin/cmake}"

[[ -x "$CMAKE_BIN" ]] || { echo "missing $CMAKE_BIN" >&2; exit 2; }
command -v ninja >/dev/null
command -v emcmake >/dev/null
command -v em++ >/dev/null

rm -rf "$BUILD_ROOT"
mkdir -p "$BUILD_ROOT" "$DIST"

git clone --depth 1 --branch "$LLVM_TAG" https://github.com/llvm/llvm-project.git "$SRC"

COMMON=(
  -G Ninja
  -S "$SRC/llvm"
  -DCMAKE_BUILD_TYPE=Release
  -DLLVM_INCLUDE_TESTS=OFF
  -DLLVM_INCLUDE_EXAMPLES=OFF
  -DLLVM_INCLUDE_BENCHMARKS=OFF
  -DLLVM_ENABLE_BINDINGS=OFF
  -DLLVM_ENABLE_TERMINFO=OFF
  -DLLVM_ENABLE_LIBEDIT=OFF
  -DLLVM_ENABLE_LIBXML2=OFF
  -DLLVM_ENABLE_FFI=OFF
  -DLLVM_ENABLE_ZLIB=OFF
  -DLLVM_ENABLE_ZSTD=OFF
  -DLLVM_ENABLE_ASSERTIONS=OFF
  -DLLVM_BUILD_UTILS=OFF
  -DLLVM_BUILD_EXAMPLES=OFF
)

# LLVM cross builds require a host-native llvm-tblgen from the exact same tree.
"$CMAKE_BIN" "${COMMON[@]}" -B "$HOST" \
  -DLLVM_TARGETS_TO_BUILD="" \
  -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=""
"$CMAKE_BIN" --build "$HOST" --target llvm-tblgen -j "${JOBS:-4}"

# This follows the proven Emscripten LLVM-tool pattern, but keeps only M68k.
# wait4 is referenced by LLVM's Unix process helpers and is supplied by
# Emscripten under the syscall spelling.
export CXXFLAGS="${CXXFLAGS:-} -Dwait4=__syscall_wait4"

EM_LINK_FLAGS="-s NO_INVOKE_RUN -s ALLOW_MEMORY_GROWTH=1 -s INITIAL_MEMORY=67108864 -s EXPORTED_RUNTIME_METHODS=FS,callMain -s MODULARIZE=1 -s EXPORT_NAME=createLLCM68k -s ENVIRONMENT=web,node -s WASM_BIGINT=1"

# Ubuntu 24.04 runners also ship a newer /usr/local CMake. Emscripten 3.1.6's
# toolchain modules are compatible with Ubuntu's /usr/bin/cmake 3.28, while
# CMake 3.31 trips over a private UnixPaths command. Pin the system CMake here.
emcmake "$CMAKE_BIN" "${COMMON[@]}" -B "$WASM" \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DCMAKE_EXE_LINKER_FLAGS="$EM_LINK_FLAGS" \
  -DLLVM_TARGETS_TO_BUILD="" \
  -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=M68k \
  -DLLVM_TABLEGEN="$HOST/bin/llvm-tblgen" \
  -DLLVM_ENABLE_THREADS=OFF \
  -DLLVM_DEFAULT_TARGET_TRIPLE=m68k-unknown-elf \
  -DLLVM_TARGET_ARCH=M68k \
  -DLLVM_BUILD_TOOLS=ON \
  -DLLVM_INCLUDE_TOOLS=ON

"$CMAKE_BIN" --build "$WASM" --target llc -j "${JOBS:-4}"

[[ -f "$WASM/bin/llc.js" ]] || { echo "missing $WASM/bin/llc.js" >&2; exit 2; }
[[ -f "$WASM/bin/llc.wasm" ]] || { echo "missing $WASM/bin/llc.wasm" >&2; exit 2; }
cp "$WASM/bin/llc.js" "$DIST/llc-m68k.js"
cp "$WASM/bin/llc.wasm" "$DIST/llc-m68k.wasm"

echo "W2 llc-m68k WASM built"
wc -c "$DIST/llc-m68k.js" "$DIST/llc-m68k.wasm"

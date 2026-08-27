#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LLVM_TAG="${LLVM_TAG:-llvmorg-16.0.4}"
BUILD_ROOT="${LLVM_CLANG_WASM_BUILD_ROOT:-$ROOT/build/clang-m68k-wasm}"
SRC="$BUILD_ROOT/llvm-project"
HOST="$BUILD_ROOT/host"
WASM="$BUILD_ROOT/wasm"
DIST="$BUILD_ROOT/dist"
CMAKE_BIN="${CMAKE_BIN:-/usr/bin/cmake}"
CMAKE_COMPAT="$ROOT/wasm/llvm/cmake-emscripten-compat.cmake"
JOBS="${JOBS:-2}"

[[ -x "$CMAKE_BIN" ]] || { echo "missing $CMAKE_BIN" >&2; exit 2; }
[[ -f "$CMAKE_COMPAT" ]] || { echo "missing $CMAKE_COMPAT" >&2; exit 2; }
command -v ninja >/dev/null
command -v emcmake >/dev/null
command -v em++ >/dev/null

rm -rf "$BUILD_ROOT"
mkdir -p "$BUILD_ROOT" "$DIST"

git clone --depth 1 --branch "$LLVM_TAG" https://github.com/llvm/llvm-project.git "$SRC"

COMMON=(
  -G Ninja
  -S "$SRC/llvm"
  -DLLVM_ENABLE_PROJECTS=clang
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
  -DCLANG_ENABLE_ARCMT=OFF
  -DCLANG_ENABLE_STATIC_ANALYZER=OFF
)

# Cross-building Clang needs target-table generators that run on the host.
"$CMAKE_BIN" "${COMMON[@]}" -B "$HOST" \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_TARGETS_TO_BUILD="" \
  -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=M68k
"$CMAKE_BIN" --build "$HOST" --target llvm-tblgen clang-tblgen -j "$JOBS"

# LLVM's Unix process helpers reference wait4. Emscripten exposes the syscall
# implementation under this spelling. The browser compiler is invoked in
# in-process -cc1 mode, so it never needs fork/exec to launch a child compiler.
export CXXFLAGS="${CXXFLAGS:-} -Dwait4=__syscall_wait4"

EM_LINK_FLAGS="-s NO_INVOKE_RUN -s ALLOW_MEMORY_GROWTH=1 -s INITIAL_MEMORY=134217728 -s STACK_SIZE=8388608 -s EXPORTED_RUNTIME_METHODS=FS,callMain -s MODULARIZE=1 -s EXPORT_NAME=createClangM68k -s ENVIRONMENT=web,node -s WASM_BIGINT=1"

emcmake "$CMAKE_BIN" "${COMMON[@]}" -B "$WASM" \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DCMAKE_PROJECT_INCLUDE_BEFORE="$CMAKE_COMPAT" \
  -DCMAKE_EXE_LINKER_FLAGS="$EM_LINK_FLAGS" \
  -DLLVM_TARGETS_TO_BUILD="" \
  -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=M68k \
  -DLLVM_TABLEGEN="$HOST/bin/llvm-tblgen" \
  -DCLANG_TABLEGEN="$HOST/bin/clang-tblgen" \
  -DLLVM_ENABLE_THREADS=OFF \
  -DLLVM_DEFAULT_TARGET_TRIPLE=m68k-unknown-elf \
  -DLLVM_TARGET_ARCH=M68k \
  -DLLVM_BUILD_TOOLS=ON \
  -DLLVM_INCLUDE_TOOLS=ON

"$CMAKE_BIN" --build "$WASM" --target clang -j "$JOBS"

[[ -f "$WASM/bin/clang.js" ]] || { echo "missing $WASM/bin/clang.js" >&2; exit 2; }
[[ -f "$WASM/bin/clang.wasm" ]] || { echo "missing $WASM/bin/clang.wasm" >&2; exit 2; }
cp "$WASM/bin/clang.js" "$DIST/clang-m68k.js"
cp "$WASM/bin/clang.wasm" "$DIST/clang-m68k.wasm"

echo "W2 clang-m68k WASM built"
wc -c "$DIST/clang-m68k.js" "$DIST/clang-m68k.wasm"

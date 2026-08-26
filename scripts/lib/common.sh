#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# shellcheck disable=SC1091
source "$PROJECT_ROOT/versions.env"

BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build}"
DEPS_DIR="${DEPS_DIR:-$PROJECT_ROOT/.deps}"
FIXTURES_DIR="$PROJECT_ROOT/fixtures"
GENERATED_DIR="$BUILD_DIR/generated"
GBRECOMP_HOME="$DEPS_DIR/gb-recompiled/$GBRECOMP_VERSION"
GBRECOMP="$GBRECOMP_HOME/gb-recompiled-linux/gbrecomp"
SGDK_HOME="$DEPS_DIR/SGDK/$SGDK_VERSION"
TOOLCHAIN_HOME="$DEPS_DIR/m68k-elf/$M68K_TOOLCHAIN_VERSION"

ensure_dir() {
  mkdir -p "$1"
}

ensure_gbrecomp() {
  if [[ ! -x "$GBRECOMP" ]]; then
    "$PROJECT_ROOT/scripts/bootstrap-gbrecomp.sh"
  fi
}

fixture_rom() {
  printf '%s/%s.gb\n' "$FIXTURES_DIR" "$1"
}

upstream_dir() {
  printf '%s/%s/upstream\n' "$GENERATED_DIR" "$1"
}

sgdk_generated_dir() {
  printf '%s/%s/sgdk\n' "$GENERATED_DIR" "$1"
}

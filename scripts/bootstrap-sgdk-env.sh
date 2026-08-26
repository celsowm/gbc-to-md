#!/usr/bin/env bash
set -euo pipefail
# shellcheck source=scripts/lib/common.sh
source "$(cd "$(dirname "$0")" && pwd)/lib/common.sh"
"$PROJECT_ROOT/scripts/bootstrap-sgdk.sh"
"$PROJECT_ROOT/scripts/bootstrap-toolchain.sh"
TOOLCHAIN_BIN="$(cat "$TOOLCHAIN_HOME/.bin-path")"
cat <<ENV
SGDK=$SGDK_HOME
PATH=$TOOLCHAIN_BIN:\$PATH
PREFIX=m68k-elf-
SGDK_BUILD=release
ENV

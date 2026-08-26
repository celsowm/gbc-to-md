#!/usr/bin/env bash
set -euo pipefail
# shellcheck source=scripts/lib/common.sh
source "$(cd "$(dirname "$0")" && pwd)/lib/common.sh"

command -v git >/dev/null || { echo "git is required" >&2; exit 2; }

if [[ -f "$SGDK_HOME/makefile.gen" ]]; then
  echo "SGDK already installed: $SGDK_HOME"
  exit 0
fi

ensure_dir "$(dirname "$SGDK_HOME")"
rm -rf "$SGDK_HOME"
echo "Cloning SGDK $SGDK_VERSION..."
git clone --depth 1 --branch "$SGDK_VERSION" https://github.com/Stephane-D/SGDK.git "$SGDK_HOME"
[[ -f "$SGDK_HOME/makefile.gen" ]] || { echo "SGDK bootstrap failed" >&2; exit 3; }
echo "Installed SGDK $SGDK_VERSION: $SGDK_HOME"

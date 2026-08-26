#!/usr/bin/env bash
set -euo pipefail
# shellcheck source=scripts/lib/common.sh
source "$(cd "$(dirname "$0")" && pwd)/lib/common.sh"

if [[ "$(uname -s)" != Linux || "$(uname -m)" != x86_64 ]]; then
  echo "Automatic GB Recompiled bootstrap currently supports Linux x86_64." >&2
  echo "Install gbrecomp manually and place it at: $GBRECOMP" >&2
  exit 2
fi

if [[ -x "$GBRECOMP" ]]; then
  echo "GB Recompiled already installed: $GBRECOMP"
  exit 0
fi

command -v curl >/dev/null || { echo "curl is required" >&2; exit 2; }
command -v tar >/dev/null || { echo "tar is required" >&2; exit 2; }
command -v sha256sum >/dev/null || { echo "sha256sum is required" >&2; exit 2; }

ensure_dir "$DEPS_DIR/downloads"
ensure_dir "$GBRECOMP_HOME"
archive="$DEPS_DIR/downloads/$GBRECOMP_LINUX_ASSET"
url="https://github.com/arcanite24/gb-recompiled/releases/download/$GBRECOMP_VERSION/$GBRECOMP_LINUX_ASSET"

if [[ ! -f "$archive" ]]; then
  echo "Downloading GB Recompiled $GBRECOMP_VERSION..."
  curl --fail --location --retry 3 --output "$archive" "$url"
fi

echo "$GBRECOMP_LINUX_SHA256  $archive" | sha256sum --check --status || {
  echo "Checksum mismatch for $archive" >&2
  rm -f "$archive"
  exit 3
}

rm -rf "$GBRECOMP_HOME"
ensure_dir "$GBRECOMP_HOME"
tar xzf "$archive" -C "$GBRECOMP_HOME"
[[ -x "$GBRECOMP" ]] || { echo "gbrecomp was not found after extraction" >&2; exit 4; }
echo "Installed GB Recompiled $GBRECOMP_VERSION: $GBRECOMP"

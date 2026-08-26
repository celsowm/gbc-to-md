#!/usr/bin/env bash
set -euo pipefail
missing=0
for cmd in bash python3 gcc make curl tar sha256sum git ld objcopy nm size java; do
  if command -v "$cmd" >/dev/null 2>&1; then
    printf 'ok      %s\n' "$cmd"
  else
    printf 'missing %s\n' "$cmd"
    missing=1
  fi
done
if (( missing )); then
  cat <<'MSG'

On Ubuntu/Debian, install the host dependencies with:
  sudo apt-get update
  sudo apt-get install -y build-essential binutils curl git python3 ca-certificates openjdk-17-jre-headless
MSG
  exit 1
fi

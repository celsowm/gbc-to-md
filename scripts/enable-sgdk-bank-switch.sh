#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "$0")" && pwd)/lib/common.sh"
SGDK="${SGDK:-$SGDK_HOME}"
CONFIG="$SGDK/inc/config.h"
[[ -f "$CONFIG" ]] || { echo "SGDK config not found: $CONFIG" >&2; exit 2; }
if grep -Eq '^[[:space:]]*#define[[:space:]]+ENABLE_BANK_SWITCH[[:space:]]+1([[:space:]]|$)' "$CONFIG"; then
  echo "SGDK bank switching is already enabled."
  exit 0
fi
python3 - "$CONFIG" <<'PY'
from pathlib import Path
import re, sys
p=Path(sys.argv[1]); s=p.read_text()
s2,n=re.subn(r'(?m)^\s*#define\s+ENABLE_BANK_SWITCH\s+0\s*$', '#define ENABLE_BANK_SWITCH      1', s, count=1)
if n != 1:
    raise SystemExit('Could not locate ENABLE_BANK_SWITCH 0 in SGDK config.h')
p.write_text(s2)
PY
echo "Enabled official SEGA mapper support in: $CONFIG"

#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "$0")" && pwd)/lib/common.sh"
cd "$PROJECT_ROOT"
ensure_gbrecomp

GAME="${1:-cakegame}"
case "$GAME" in
  basicdemo)   GENERATOR=make_basicdemo.py;  PREFIX_NAME=basicdemo;  JOBS=2; ANNOTATION_ARGS=() ;;
  irqtest)   GENERATOR=make_irqtest.py;  PREFIX_NAME=irqtest;  JOBS=2; ANNOTATION_ARGS=() ;;
  timertest) GENERATOR=make_timertest.py;PREFIX_NAME=timertest;JOBS=2; ANNOTATION_ARGS=() ;;
  cakegame)  GENERATOR=make_cakegame.py; PREFIX_NAME=cakegame; JOBS=2; ANNOTATION_ARGS=() ;;
  mbc1test)  GENERATOR=make_mbc1test.py; PREFIX_NAME=mbc1test; JOBS=2; ANNOTATION_ARGS=() ;;
  mbc3test)  GENERATOR=make_mbc3test.py; PREFIX_NAME=mbc3test; JOBS=4; ANNOTATION_ARGS=(--annotations "$FIXTURES_DIR/mbc3test.annotations") ;;
  mbc5test)  GENERATOR=make_mbc5test.py; PREFIX_NAME=mbc5test; JOBS=8; ANNOTATION_ARGS=(--annotations "$FIXTURES_DIR/mbc5test.annotations") ;;
  *) echo "Unsupported fixture: $GAME" >&2; exit 2 ;;
esac

ROM="$FIXTURES_DIR/$PREFIX_NAME.gb"
OUT="$(upstream_dir "$PREFIX_NAME")"
TARGET="$(sgdk_generated_dir "$PREFIX_NAME")"
mkdir -p "$BUILD_DIR"
python3 "$FIXTURES_DIR/$GENERATOR"
rm -rf "$OUT" "$TARGET"
mkdir -p "$OUT"
"$GBRECOMP" "$ROM" -o "$OUT" --output-prefix "$PREFIX_NAME" --reachable-only --no-scan --no-comments "${ANNOTATION_ARGS[@]}" -j "$JOBS"
"$PROJECT_ROOT/tools/prepare_sgdk_generated.py" --src "$OUT" --dst "$TARGET" --prefix "$PREFIX_NAME"
printf '%s\n' "$TARGET"

#!/bin/bash
# build_flash.sh — build firmware and copy to Desktop with git short hash version
# Usage:  ./scripts/build_flash.sh [env]   default env = lilygo_330
#         ./scripts/build_flash.sh waveshare7b_display

set -e

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ENV="${1:-lilygo_330}"
DEST_DIR="$HOME/Desktop"

# Derive a friendly base name from the env
case "$ENV" in
  lilygo_330)           BASE="be_velar_tcan485" ;;
  waveshare7b_display)  BASE="be_waveshare7b_display" ;;
  stark_330)            BASE="be_stark_330" ;;
  lilygo_2CAN_330)      BASE="be_lilygo_2can" ;;
  BECom_330)            BASE="be_becom" ;;
  *)                    BASE="be_${ENV}" ;;
esac

HASH=$(cd "$REPO_ROOT" && git rev-parse --short HEAD)
BRANCH=$(cd "$REPO_ROOT" && git rev-parse --abbrev-ref HEAD)
OUTFILE="${DEST_DIR}/${BASE}_${BRANCH}_${HASH}.bin"

echo "Building env: $ENV  ($BRANCH @ $HASH)"
cd "$REPO_ROOT" && pio run -e "$ENV"

cp "$REPO_ROOT/.pio/build/$ENV/firmware.bin" "$OUTFILE"
echo "Copied → $OUTFILE  ($(du -h "$OUTFILE" | cut -f1))"

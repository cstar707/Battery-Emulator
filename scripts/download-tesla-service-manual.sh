#!/usr/bin/env bash
# Download Tesla Service Manual (Model S or Model 3, English) for offline viewing.
# Phase 1: mirror index + assets. Phase 2: download every TOC page from URL list.
# Requires: wget (install with brew install wget on macOS)
#
# Run from repo root:
#   ./scripts/download-tesla-service-manual.sh          # Model S (default)
#   ./scripts/download-tesla-service-manual.sh Model3   # Model 3 English
#   ./scripts/download-tesla-service-manual.sh ModelS /path/to/output  # custom output

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Model: ModelS (default) or Model3 (English only)
MODEL="${1:-ModelS}"
if [[ "$MODEL" == Model3 ]]; then
  BASE_URL="https://service.tesla.com/docs/Model3/ServiceManual/en-us"
  OUTPUT_DIR="${2:-$REPO_ROOT/docs/offline-manuals/Model3-ServiceManual}"
  INDEX_PATH="service.tesla.com/docs/Model3/ServiceManual/en-us/index.html"
else
  BASE_URL="https://service.tesla.com/docs/ModelS/ServiceManual/en-us"
  OUTPUT_DIR="${2:-$REPO_ROOT/docs/offline-manuals/ModelS-ServiceManual}"
  INDEX_PATH="service.tesla.com/docs/ModelS/ServiceManual/en-us/index.html"
fi

WGET_OPTS=(
  --no-check-certificate
  --execute robots=off
  --wait=1
  --limit-rate=500k
  --user-agent="Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36"
)

echo "Downloading Tesla $MODEL Service Manual (English)..."
echo "  URL: $BASE_URL"
echo "  Output: $OUTPUT_DIR"
echo ""

mkdir -p "$OUTPUT_DIR"
cd "$OUTPUT_DIR"

# Phase 1: mirror index and page requisites (CSS, JS, images)
echo "Phase 1: Mirror index and assets..."
wget \
  --mirror \
  --page-requisites \
  --convert-links \
  --adjust-extension \
  --no-parent \
  --level=0 \
  --execute robots=off \
  --domains=service.tesla.com \
  "${WGET_OPTS[@]}" \
  "$BASE_URL/"

INDEX_FILE="$(pwd)/$INDEX_PATH"
if [[ ! -f "$INDEX_FILE" ]]; then
  echo "Error: index not found at $INDEX_FILE"
  exit 1
fi

# Phase 2: extract every GUID page URL from the index and download explicitly
# (recursive follow can miss or timeout; this gets all ~1100 pages)
echo ""
echo "Phase 2: Downloading all TOC pages from index..."
URL_LIST="$(mktemp)"
grep -oE 'href="GUID-[A-F0-9-]+\.html"' "$INDEX_FILE" \
  | sed 's/href="//;s/"//' \
  | sort -u \
  | while read -r f; do echo "$BASE_URL/$f"; done \
  > "$URL_LIST"
N=$(wc -l < "$URL_LIST" | tr -d ' ')
echo "  Found $N page URLs in TOC. Downloading..."
wget \
  --input-file="$URL_LIST" \
  --page-requisites \
  --convert-links \
  --adjust-extension \
  --timestamping \
  "${WGET_OPTS[@]}"
rm -f "$URL_LIST"

echo ""
if [[ -f "$(pwd)/$INDEX_PATH" ]]; then
  echo "Done. Open in a browser:"
  echo "  file://$(pwd)/$INDEX_PATH"
  echo "  or run: open \"$(pwd)/$INDEX_PATH\""
else
  echo "Done. Look for index.html under: $(pwd)"
fi

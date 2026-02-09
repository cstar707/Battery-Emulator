#!/usr/bin/env bash
# Serve the offline Tesla Service Manual (Model S or Model 3) so you can view it at
# http://localhost:8000/...  Links work reliably this way (better than file://).
#
# Run from repo root:
#   ./scripts/serve-tesla-service-manual.sh          # Model S
#   ./scripts/serve-tesla-service-manual.sh Model3  # Model 3

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
MODEL="${1:-ModelS}"
if [[ "$MODEL" == Model3 ]]; then
  MANUAL_DIR="${2:-$REPO_ROOT/docs/offline-manuals/Model3-ServiceManual}"
  INDEX_URL="http://localhost:8000/service.tesla.com/docs/Model3/ServiceManual/en-us/"
else
  MANUAL_DIR="${2:-$REPO_ROOT/docs/offline-manuals/ModelS-ServiceManual}"
  INDEX_URL="http://localhost:8000/service.tesla.com/docs/ModelS/ServiceManual/en-us/"
fi

if [[ ! -d "$MANUAL_DIR" ]]; then
  echo "Manual directory not found: $MANUAL_DIR"
  echo "Run: ./scripts/download-tesla-service-manual.sh $MODEL"
  exit 1
fi

echo "Serving Tesla $MODEL manual at: $INDEX_URL"
echo "Press Ctrl+C to stop."
echo ""

cd "$MANUAL_DIR"
python3 -m http.server 8000

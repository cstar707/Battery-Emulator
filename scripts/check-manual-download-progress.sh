#!/usr/bin/env bash
# Check progress of the Tesla service manual download.
# Run from repo root: ./scripts/check-manual-download-progress.sh
# Optional: ./scripts/check-manual-download-progress.sh --watch   (check every 2 min until done)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
MANUAL_DIR="$REPO_ROOT/docs/offline-manuals/ModelS-ServiceManual"
TARGET_PAGES=1098
WATCH="${1:-}"

progress() {
  local count
  count=$(find "$MANUAL_DIR" -name "GUID-*.html" 2>/dev/null | wc -l | tr -d ' ')
  local pct=0
  [[ $TARGET_PAGES -gt 0 ]] && pct=$((count * 100 / TARGET_PAGES))
  local bar_len=30
  local filled=$((bar_len * count / TARGET_PAGES))
  local empty=$((bar_len - filled))
  local bar
  bar=$(printf "%${filled}s" | tr ' ' '█')
  bar+=$(printf "%${empty}s" | tr ' ' '░')

  # Is download process running?
  local running=""
  if pgrep -f "download-tesla-service-manual" >/dev/null 2>&1 || pgrep -f "wget.*ModelS/ServiceManual" >/dev/null 2>&1; then
    running=" [downloading...]"
  else
    running=" [idle - run ./scripts/download-tesla-service-manual.sh to fetch rest]"
  fi

  # Last activity (from log or file mtime)
  local last_activity=""
  local logfile="$REPO_ROOT/docs/offline-manuals/download.log"
  if [[ -f "$logfile" ]]; then
    last_activity=$(stat -f "%Sm" -t "%Y-%m-%d %H:%M" "$logfile" 2>/dev/null || stat -c "%y" "$logfile" 2>/dev/null | cut -c1-16)
  fi

  echo ""
  echo "  Tesla Model S Service Manual download"
  echo "  $count / $TARGET_PAGES pages  $pct%"
  echo "  [$bar]$running"
  if [[ -n "$last_activity" ]]; then
    echo "  Log last updated: $last_activity"
  fi
  echo ""
}

if [[ "$WATCH" == "--watch" || "$WATCH" == "-w" ]]; then
  echo "Watching progress (check every 2 min). Ctrl+C to stop."
  while true; do
    progress
    count=$(find "$MANUAL_DIR" -name "GUID-*.html" 2>/dev/null | wc -l | tr -d ' ')
    [[ "$count" -ge "$TARGET_PAGES" ]] && echo "Done! All $TARGET_PAGES pages downloaded." && break
    if ! pgrep -f "download-tesla-service-manual" >/dev/null 2>&1 && ! pgrep -f "wget.*ModelS/ServiceManual" >/dev/null 2>&1; then
      echo "Download process not running. Start with: ./scripts/download-tesla-service-manual.sh"
      break
    fi
    sleep 120
  done
else
  progress
fi

#!/usr/bin/env bash
# Run on the server (e.g. 10.10.53.92) to pull latest and restart Solis S6 UI.
# Usage: ./scripts/deploy-solis-s6-on-server.sh
#   Or from your machine: ssh user@10.10.53.92 'cd /path/to/repo && ./scripts/deploy-solis-s6-on-server.sh'
set -e
REPO_ROOT="${REPO_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
cd "$REPO_ROOT"
git pull
if systemctl is-active --quiet solis-s6-ui 2>/dev/null; then
  sudo systemctl restart solis-s6-ui
  echo "Restarted solis-s6-ui."
else
  echo "solis-s6-ui service not active; skip restart."
fi

#!/usr/bin/env bash
set -euo pipefail

WINNER="main-dashboard-port80.service"
LOSERS=(mqtt-dashboard.service secure-dashboard.service)

if [[ "${EUID}" -ne 0 ]]; then
  echo "Run as root (for example: sudo $0 [--execute])." >&2
  exit 1
fi

if [[ "${1:-}" != "--execute" ]]; then
  echo "Dry run only. Staged maintenance-window action for port 80:"
  echo "  1. systemctl daemon-reload"
  echo "  2. systemctl stop ${LOSERS[*]}"
  echo "  3. systemctl disable ${LOSERS[*]}"
  echo "  4. systemctl start ${WINNER}"
  echo "  5. systemctl status ${WINNER} --no-pager"
  echo "  6. ss -ltnp | grep :80"
  echo
  echo "Nothing changed. Re-run with --execute during the approved maintenance window only."
  exit 0
fi

systemctl daemon-reload
systemctl stop "${LOSERS[@]}" || true
systemctl disable "${LOSERS[@]}" || true
systemctl start "${WINNER}"

echo
systemctl --no-pager --full status "${WINNER}" --lines=20
echo
ss -ltnp | grep ':80' || true
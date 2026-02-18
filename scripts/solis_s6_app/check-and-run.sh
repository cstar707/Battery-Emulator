#!/usr/bin/env bash
# Run on 10.10.53.92 to fix "page not loading" for http://10.10.53.92:3007/
# Usage: cd scripts/solis_s6_app && ./check-and-run.sh
# Or:    ssh user@10.10.53.92 'cd /path/to/repo/scripts/solis_s6_app && ./check-and-run.sh'
set -e
cd "$(dirname "$0")"
PORT="${SOLIS_APP_PORT:-3007}"

echo "=== Solis S6 UI (port $PORT) check ==="

# 1. Is anything listening on 3007?
if command -v ss >/dev/null 2>&1; then
  if ss -tlnp 2>/dev/null | grep -q ":$PORT "; then
    echo "Port $PORT is in use. Checking process..."
    ss -tlnp 2>/dev/null | grep ":$PORT " || true
  else
    echo "Port $PORT is not in use (app not running)."
  fi
elif command -v netstat >/dev/null 2>&1; then
  if netstat -tlnp 2>/dev/null | grep -q ":$PORT "; then
    echo "Port $PORT is in use."
  else
    echo "Port $PORT is not in use (app not running)."
  fi
fi

# 2. Systemd service
if systemctl list-unit-files 2>/dev/null | grep -q solis-s6-ui; then
  echo ""
  echo "Service solis-s6-ui:"
  systemctl is-active solis-s6-ui 2>/dev/null || true
  systemctl status solis-s6-ui --no-pager 2>/dev/null || true
else
  echo "Service solis-s6-ui not installed."
fi

# 3. Venv and deps
echo ""
if [[ -d .venv ]]; then
  echo "Activating .venv..."
  . .venv/bin/activate
else
  echo "No .venv found. Creating one and installing deps..."
  python3 -m venv .venv
  . .venv/bin/activate
  pip install -q -r requirements.txt
fi

# 4. Import check (same as run.sh)
echo "Checking app import..."
if ! python3 -c "import main" 2>&1; then
  echo "App import failed. Fix the error above (e.g. pip install -r requirements.txt)."
  exit 1
fi
echo "Import OK."

# 5. Start if not already listening
if command -v ss >/dev/null 2>&1; then
  IN_USE=$(ss -tlnp 2>/dev/null | grep -c ":$PORT " || true)
else
  IN_USE=0
fi
if [[ "$IN_USE" -eq 0 ]]; then
  echo ""
  echo "Starting app on 0.0.0.0:$PORT (Ctrl+C to stop)..."
  export SOLIS_INVERTER_HOST="${SOLIS_INVERTER_HOST:-10.10.53.16}"
  export SOLIS_INVERTER_PORT="${SOLIS_INVERTER_PORT:-502}"
  export SOLIS_APP_PORT="$PORT"
  exec uvicorn main:app --host 0.0.0.0 --port "$PORT"
else
  echo ""
  echo "App already running on port $PORT. Open http://$(hostname -I 2>/dev/null | awk '{print $1}'):$PORT/ or http://10.10.53.92:$PORT/"
  echo "To restart: sudo systemctl restart solis-s6-ui   (if using systemd)"
  echo "Or kill the process on port $PORT and run this script again."
fi

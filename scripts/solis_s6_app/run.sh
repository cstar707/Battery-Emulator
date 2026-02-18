#!/usr/bin/env bash
# Run Solis S6 UI on the server (e.g. 10.10.53.92). Uses venv if present.
set -e
cd "$(dirname "$0")"
if [[ -d .venv ]]; then
  . .venv/bin/activate
fi
export SOLIS_INVERTER_HOST="${SOLIS_INVERTER_HOST:-10.10.53.16}"
export SOLIS_INVERTER_PORT="${SOLIS_INVERTER_PORT:-502}"
export SOLIS_APP_PORT="${SOLIS_APP_PORT:-3007}"
# Catch import errors before uvicorn
python3 -c "import main" 2>&1 || { echo "Import failed. Fix the error above."; exit 1; }
exec uvicorn main:app --host 0.0.0.0 --port "${SOLIS_APP_PORT}"

# Solis S6 / Solark UI

Web UI and API for **Solis1** (Solis S6 at 10.10.53.16:502) and labels for **Solark1** / **Solark2** (new board). Runs on **10.10.53.92:3007**.

## Quick start

```bash
cd scripts/solis_s6_app
python3 -m venv .venv
source .venv/bin/activate   # Windows: .venv\Scripts\activate
pip install -r requirements.txt
./run.sh
# or: uvicorn main:app --host 0.0.0.0 --port 3007
```

Then open **http://10.10.53.92:3007** (or http://localhost:3007 if running locally).

## Troubleshooting: http://10.10.53.92:3007/ not loading

On the server (10.10.53.92), run:

```bash
cd /path/to/tesla-model-y-stationary-power-plant/scripts/solis_s6_app
./check-and-run.sh
```

This script checks whether port 3007 is in use, systemd service status, venv/deps, and **starts the app** if it isn’t running. If the app is already running, it prints the URL and how to restart the service.

- **Manual start (foreground):** `./run.sh` or `uvicorn main:app --host 0.0.0.0 --port 3007`
- **If using systemd:** `sudo systemctl restart solis-s6-ui` then `sudo systemctl status solis-s6-ui`
- **Health check:** `curl -s http://localhost:3007/api/health` should return `{"ok":true,"version":"..."}` when the app is up.

## Version

The UI version is read from **`VERSION`** (e.g. `1.0.0`) and shown in the nav and at **`GET /api/version`**. Override with env **`SOLIS_UI_VERSION`**.

**Bump version on each push/release:**

```bash
./bump_version.sh        # patch: 1.0.0 -> 1.0.1
./bump_version.sh minor  # 1.0.1 -> 1.1.0
./bump_version.sh major  # 1.1.0 -> 2.0.0
```

Commit the updated `VERSION` file with your changes.

## Run on the server (10.10.53.92)

On the server machine:

```bash
cd /path/to/tesla-model-y-stationary-power-plant/scripts/solis_s6_app
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
./run.sh
```

To run as a service: copy `solis-s6-ui.service` to `/etc/systemd/system/`, edit `User`, `WorkingDirectory`, and `ExecStart` paths, then:

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now solis-s6-ui
```

## Config (env)

| Variable | Default | Description |
|----------|---------|-------------|
| `SOLIS_INVERTER_HOST` | 10.10.53.16 | Solis S6 Modbus TCP host |
| `SOLIS_INVERTER_PORT` | 502 | Modbus TCP port |
| `SOLIS_APP_PORT` | 3007 | This app’s HTTP port |
| `SOLARK1_HOST` | (empty) | IP of the new Solark board when set |
| `SOLIS_POLL_INTERVAL_SEC` | 5 | Seconds between Modbus polls |
| `MQTT_HOST` | 10.10.53.92 | MQTT broker host (same as solar stack). Set to empty to disable publishing. |
| `MQTT_PORT` | 1883 | MQTT broker port |
| `MQTT_TOPIC_PREFIX` | solis | Topic prefix (e.g. `solis/status`, `solis/sensors/<name>`) |
| `MQTT_USER` / `MQTT_PASSWORD` | api / test12345 | Same as solar-monitoring for now. **TODO: secure (strong password, env-only) after cutover.** Override in `.env` if needed. |
| `MQTT_CLIENT_ID` | solis-s6-ui | MQTT client id |

Optional: copy `.env.example` to `.env` in this directory and set `MQTT_USER` and `MQTT_PASSWORD` (and override `MQTT_HOST` if needed). `.env` is not committed.

## MQTT

When `MQTT_HOST` is non-empty, every poll publishes:

- **`<prefix>/status`** — One JSON message with all sensors (grid_power_W, pv_power_W, battery_soc_pct, storage_bits, hybrid_bits, energy today, etc.) plus `ts`.
- **`<prefix>/sensors/<name>`** — One topic per scalar (e.g. `solis/sensors/grid_power_W`, `solis/sensors/battery_soc_pct`).

On 10.10.53.92 the default broker is localhost (same host). Set `MQTT_USER` and `MQTT_PASSWORD` in `.env` or env to match the solar server broker (see `docs/mqtt-server-and-access.md`).

## Pages

- **Dashboard** — Grid / PV / battery / load power, SOC, energy today. Last-updated time; auto-refresh every 15s.
- **Sensors** — All Solis readings (power, battery, PV, AC, energy).
- **Control** — Self-use & Feed-in quick buttons; Export & charging table; full **storage (43110)** and **hybrid (43483)** bit tables with raw values.
- **Settings** — Inverter labels, current config, MQTT status (publishing to broker or disabled).
- **Debug** — Live Modbus debug stream: reads, writes, and errors from the Solis Modbus client (last 300 lines, refreshes every 2s).

## API

- `GET /api/dashboard` — Cached Solis data + ok/ts.
- `GET /api/sensors` — Same data dict.
- `GET /api/storage_bits` — Current 43110 bits as key/value.
- `GET /api/hybrid_bits` — Current 43483 bits (export, peak-shaving, etc.).
- `POST /api/storage_toggle` — Form or JSON: `bit_index` (0–11), `on` (true/false). Use `Content-Type: application/json` for scripting/HA.
- `POST /api/hybrid_toggle` — Form or JSON: `bit_index` (0–7), `on` (true/false).
- `POST /control` with `preset=use_all_solar` — Apply self-use, no export (load-based).
- `GET /api/debug/modbus` — Recent Modbus log lines (JSON) for debug.
- `POST /api/debug/modbus/clear` — Clear the debug ring buffer.

## Inverter labels

- **Solis1** — Solis S6 (this app talks to it via Modbus).
- **Solark1** — New board (Battery-Emulator + Solark RS485), Modbus ID 0x01.
- **Solark2** — Same board as Solark1, Modbus ID 0x02.

See `docs/INVERTER-LABELS.md` and `docs/SOLIS-STORAGE-MODE-TOGGLES.md`.

# MQTT reference – solar monitoring server

The **solar monitoring stack** (Envoy Debug Dashboard, unified API, etc.) uses a single **MQTT broker** (typically Mosquitto on the same host as the dashboards). To have the Battery-Emulator board (e.g. when replacing the ESPHome device at 10.10.53.32) publish to that same broker so dashboards and HA see both battery and Solark data, configure the board’s **Settings → MQTT** to use this broker.

**Credentials and server addresses must stay local.** Do not commit broker host, passwords, or usernames to the repo. Store the broker IP/hostname and credentials only in the board’s Settings and/or in local config on the server. See **`docs/local-config-and-secrets.md`**.

---

## 1. Broker (where to point the board)

| Item        | Value        | Notes |
|------------|--------------|--------|
| **Broker host** | *(local only)* | IP or hostname of the machine running Mosquitto (often the same host as the solar dashboards). Set in board **Settings → MQTT server**; do not commit. |
| **Broker port** | `1883`        | Standard MQTT. |
| **Authentication** | Yes          | Username and password are set on the server (e.g. in the unified-api or in `config/deployment.env`). Use the **same** username and password in the board’s **Settings → MQTT**. Store only on device and on server; never in this repo. |

So in the board’s web UI → **Settings → MQTT**:

- **MQTT server:** *(set to your broker host – e.g. the solar server’s IP – stored only on device)*
- **MQTT port:** `1883`
- **MQTT user** / **MQTT password:** same as used by the solar apps on the server (configured only on the device and on the server, not in git).

---

## 2. Topics used by the solar server

The unified API and dashboards **publish** to these MQTT topics (for reference; the board does not need to publish to these unless you add a bridge):

| Topic | Description |
|-------|-------------|
| `solar/dashboard` | Combined payload: `solark` (from 10.10.53.32 or replacement) + `envoy` (Envoy #1 / #2). JSON. |
| `solar/solark` | Solark summary (JSON). |
| `solar/solark/sensors/<name>` | Individual Solark sensors (e.g. `battery_soc`, `solar_power`, `grid_power`, `load_power`). |
| `solar/solark/summary` | Solark summary JSON. |
| `solar/envoy` | Envoy base topic. |
| `solar/<envoy_id>` | Per-Envoy data (e.g. envoy1, envoy2). |
| `solar/micro_grid_power` | Micro grid power (W). |
| `solar/All_Solar_Production_total` | Total solar production (W). |
| `solar/combined_total_solar_power` | Combined solar power (W). |

Today the server may get **Solark** data from the existing ESPHome device via **HTTP**. When you **replace 10.10.53.32** with the Battery-Emulator board:

- The **dedicated ESP** (replacement board) publishes **Solark** to **solar/solark** (same topic as today) and exposes the same data via **GET /solark_data** and **GET /sensor/sunsynk_***. No server change needed; sensors appear as they are today.

---

## 3. Summary

| What | Value / action |
|------|----------------|
| **Broker** | Same host as solar dashboards, port `1883` (Mosquitto). Set broker host in Settings; do not commit. |
| **Auth** | Username + password; set in board Settings to match server config (never commit). |
| **Board config** | Settings → MQTT: server = broker IP/hostname (local only), port `1883`, same user/password as server. |
| **Board publishes** | **solar/solark** (Solark, same as today), `BE/status`, `BE/board`, etc. (one MQTT config for both Battery Emulator and Solark). |
| **Server publishes** | `solar/dashboard`, `solar/solark`, `solar/solark/sensors/*`, `solar/envoy`, etc. |

For full board MQTT configuration (all fields, NVM keys), see **`docs/mqtt-server-and-access.md`**.

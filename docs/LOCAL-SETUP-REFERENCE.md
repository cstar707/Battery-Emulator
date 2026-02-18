# Local setup reference (all in this repo)

Everything below is **stored locally** in this repository. **Credentials and server addresses are never committed** – they stay on the ESP (Settings/NVM) or in local-only files (see **`docs/local-config-and-secrets.md`**). This file is your single local reference for the ESP replacement board (10.10.53.32).

---

## Build and flash

- **Env:** `lilygo_330` (Battery-Emulator + Solark on main RS485; inverter must be CAN, e.g. Sol-Ark LV).
- **Build:** `pio run -e lilygo_330`
- **Flash:** PlatformIO upload or web installer. First boot may be AP mode (Battery-Emulator Wi‑Fi).

---

## Reach the board (local)

- **By IP:** `http://<board-ip>`
- **By hostname:** `http://esphome-web-7a7e60.local/`
- **First time (AP):** Connect to Battery-Emulator Wi‑Fi → `http://192.168.4.1` → **Settings**

---

## Settings checklist (stored on device only)

Configure in **Settings**; values are saved in NVM on the ESP (not in this repo).

| Section | What to set |
|--------|--------------|
| **WiFi** | SSID, password (so board joins your LAN and can reach broker). |
| **MQTT** | Enable MQTT; **MQTT server** (broker IP/hostname – local only); **MQTT port** 1883; **MQTT user** / **MQTT password** (same as broker). **Customized MQTT topics** ✓; topic name **BE**; object ID prefix **sunsynk_**; HA device name **sunsynk**; HA device ID **sunsynk**. |
| **Web** | Username + password for web/API auth (use same on any client that calls /sensor/* or /solark_data). |
| **Inverter** | CAN type (e.g. Sol-Ark LV) so main RS485 is used for Solark. |

---

## MQTT topics (board publishes locally to your broker)

All from **one** MQTT config (one broker, one connection). No server IPs or passwords in this repo.

### BE / Tesla BMS (battery emulator) – already in BE

| Topic | Payload / content |
|-------|--------------------|
| **BE/status** | `online` (LWT when board disconnects). |
| **BE/info** | **BMS/battery:** bms_status, pause_status, event_level, emulator_status. **When BMS is alive:** SOC, SOC_real, state_of_health, temperature_min, temperature_max, **cpu_temp** (°C), stat_batt_power (W), battery_current (A), battery_voltage (V), cell_max_voltage, cell_min_voltage, cell_voltage_delta, total_capacity, remaining_capacity, remaining_capacity_real, max_discharge_power, max_charge_power; if supported: charged_energy, discharged_energy; balancing_active_cells, balancing_status. Same set with suffix `_2` for battery2 if present. |
| **BE/spec_data** | **Cell voltages (if “Send all cellvoltages via MQTT” on):** JSON with `cell_voltages` array (V per cell). **BE/spec_data_2** for battery2. |
| **BE/balancing_data** | **Cell balancing (if cell voltages on):** JSON with `cell_balancing` array (bool per cell). **BE/balancing_data_2** for battery2. |
| **BE/events** | Per event: event_type, severity, count, data, message, millis. |
| **BE/board** | **ESP health:** free_heap, min_free_heap, heap_size, max_alloc_heap (bytes), cpu_temp_c (°C), uptime_ms; if performance measurement on: core_task_10s_max_us, mqtt_task_10s_max_us, wifi_task_10s_max_us (µs). |

**Commands (subscribe on board):** **BE/command/BMSRESET**, **BE/command/PAUSE**, **BE/command/RESUME**, **BE/command/RESTART**, **BE/command/STOP** (buttons in HA / MQTT).

**HA discovery (if enabled):** Sensors for all BE/info and BE/events fields; cell voltage sensors per cell; buttons for BMSRESET, PAUSE, RESUME, RESTART, STOP.

### Solark (replacement 10.10.53.32) – same as today

| Topic | Content |
|-------|---------|
| **solar/solark** | available, last_read_millis, battery_power_W, battery_soc_pptt, battery_voltage_dV, battery_current_dA, grid_power_W, load_power_W, pv_power_W, raw_registers[]. Same topic name as today so sensors appear unchanged. |

---

## HTTP API (board, local network)

| Endpoint | Description |
|----------|-------------|
| **GET /solark_data** | Same JSON as solar/solark. |
| **GET /sensor/sunsynk_&lt;id&gt;** | Per-sensor (e.g. sunsynk_battery_soc). Response: `{"id":"sensor/...", "state": "51 %"}`. Same web auth as UI. |

---

## Solark (RS485, local wiring)

- Main RS485 (A+/B-, GND) to Solark. 9600 8N1, slave 0x01.
- No extra config: with lilygo_330 + CAN inverter, firmware uses main RS485 for Solark only.
- **Web:** Menu → **Solark** (or **Solark debug**) for live view.

---

## Verify (local)

1. **Web:** Board → Solark page → RS485 “Yes”, values updating.
2. **MQTT:** Subscribe to `BE/board` and `solar/solark` on your broker (same as in Settings).
3. **API:** `curl -u user:pass http://<board-ip>/solark_data` and `http://<board-ip>/sensor/sunsynk_battery_soc`.

---

## Other local docs (this repo)

| Doc | Purpose |
|-----|--------|
| **`docs/local-config-and-secrets.md`** | No credentials/server IPs in repo; where to store them (device only). |
| **`docs/esp-setup-replacement-10.10.53.32.md`** | Full ESP setup walkthrough. |
| **`docs/mqtt-server-and-access.md`** | MQTT config section, NVM keys, how to access broker. |
| **`docs/solark-rs485-integration.md`** | Solark integration, register map, MQTT/API details. |
| **`docs/solark-10.10.53.32-parity.md`** | Parity checklist vs 10.10.53.32. |
| **`docs/mqtt-solar-server-reference.md`** | Broker/solar server (server will be changed later; no creds in repo). |

Everything above is **local** to this project. Solark data is on **solar/solark** (same as today); no server change needed.

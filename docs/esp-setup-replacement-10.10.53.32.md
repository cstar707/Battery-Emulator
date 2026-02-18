# ESP setup – replacement board (10.10.53.32)

This guide focuses on **setting up the dedicated ESP** (Battery-Emulator board) that replaces the device at 10.10.53.32: build, flash, reach the board, then configure MQTT and Solark so it publishes to your broker and exposes the API. The server (solar dashboard) will be changed later to consume from this board.

**Single local reference:** **`docs/LOCAL-SETUP-REFERENCE.md`** – build, Settings, MQTT topics (including BE/board), API, Solark, and verify; all stored locally in this repo (no credentials or server IPs in repo).

---

## 1. Build and flash

- **Environment:** Use **`lilygo_330`** so the board runs Battery-Emulator **and** Solark on the main RS485 (inverter must be **CAN**, e.g. Sol-Ark LV).
- **Build:** From the repo root:
  ```bash
  pio run -e lilygo_330
  ```
- **Flash:** Use PlatformIO upload, or the upstream web installer with a build from this repo. After flash, the board may start in AP mode (Battery-Emulator network) or join WiFi if already configured.

---

## 2. Reach the board

- **By IP:** `http://<board-ip>` (e.g. from your router or after connecting to the board’s AP).
- **By hostname (this build):** `http://esphome-web-7a7e60.local/` (default hostname when replacing 10.10.53.32; requires mDNS).
- **First time:** Connect to the **Battery-Emulator** Wi‑Fi (default AP password in upstream README), then open `http://192.168.4.1` (or the IP shown) and go to **Settings**.

---

## 3. Settings checklist (ESP setup)

Configure everything in the board’s **web UI → Settings**. Credentials and broker details stay on the device (see **`docs/local-config-and-secrets.md`**).

### WiFi (optional but recommended)

- Connect the board to your LAN so you can reach it by IP/hostname and it can reach the MQTT broker.
- Set **WiFi SSID** and **WiFi password**; save. The board will reconnect on reboot.

### MQTT (one config for Battery Emulator + Solark)

| What | Where | Value |
|------|--------|--------|
| Enable MQTT | Settings → MQTT | Check **Enable MQTT** |
| Broker | **MQTT server** | Broker hostname or IP (same broker your solar server uses) |
| Port | **MQTT port** | **1883** |
| Auth | **MQTT user** / **MQTT password** | Same as broker (e.g. same as solar server) |
| Customized topics | **Customized MQTT topics** | Check (so sunsynk defaults apply) |
| Topic name | **MQTT topic name** | **BE** (default) |
| Object ID prefix | **Prefix for MQTT object ID** | **sunsynk_** (default for this build) |
| HA device name | **HA device name** | **sunsynk** (default) |
| HA device ID | **HA device ID** | **sunsynk** (default) |
| HA discovery | **Enable Home Assistant auto discovery** | Optional: check if you use HA |

The board will publish **BE/status**, **solar/solark** (Solark – same topic as today), and (if enabled) HA discovery for battery and Solark. One MQTT connection; no separate config for Solark.

### Web UI auth (recommended)

- Set **Username** and **Password** under the web server / login section so the API and Solark page are protected. Use the same credentials on any client (e.g. solar server) that scrapes **GET /sensor/sunsynk_*** or **GET /solark_data**.

---

## 4. Solark (RS485)

- **Hardware:** Main RS485 (A+/B-, GND) to the Solark inverter. 9600 8N1, slave 0x01. No extra board config: with **lilygo_330** and a **CAN** inverter selected, the firmware uses the main RS485 for Solark only.
- **Wiring:** Same as the previous 10.10.53.32 device (RS485 to Solark). No second RS485 port needed.
- **Inverter type:** In **Settings**, ensure the inverter is set to a **CAN** type (e.g. Sol-Ark LV). Solark on main RS485 is only enabled when the inverter is CAN.

After saving, the board will poll Solark and fill **solar/solark** and the API; no separate “Solark enable” checkbox.

---

## 5. What the ESP publishes and exposes

| Output | Description |
|--------|-------------|
| **MQTT** | **solar/solark** – JSON (battery_power_W, battery_soc_pptt, battery_voltage_dV, battery_current_dA, grid_power_W, load_power_W, pv_power_W, raw_registers). Same topic as today so sensors appear unchanged. Same interval as other MQTT (e.g. 5 s). |
| **MQTT** | **BE/board** – ESP health: free_heap, min_free_heap, heap_size, max_alloc_heap (bytes), cpu_temp_c (°C), uptime_ms; when performance measurement is on: core_task_10s_max_us, mqtt_task_10s_max_us, wifi_task_10s_max_us (µs). Use to check for overload. |
| **MQTT** | **BE/status**, common info, cell voltages, events, etc. (Battery Emulator). |
| **API** | **GET /solark_data** – Same JSON as solar/solark. |
| **API** | **GET /sensor/sunsynk_&lt;id&gt;** – ESPHome-style per-sensor (e.g. sunsynk_battery_soc, sunsynk_grid_power). Response: `{"id":"sensor/...", "state": "51 %"}`. |
| **Web** | **Solark** (or **Solark debug**) in the menu – live view of RS485 data. |

Solark is on **solar/solark** (same as today); no server change needed.

---

## 6. Verify

1. **Web:** Open the board → **Solark** (or **Solark debug**). Check that RS485 link is “Yes” and values update.
2. **MQTT:** Subscribe to **solar/solark** on your broker (same host/port/user as in Settings). You should see JSON every few seconds.
3. **API:** `curl -u user:pass http://<board-ip>/solark_data` and `curl -u user:pass http://<board-ip>/sensor/sunsynk_battery_soc` (use the board’s web login if enabled).

---

## 7. References

| Topic | Doc |
|-------|-----|
| MQTT fields, NVM keys, access | **`docs/mqtt-server-and-access.md`** |
| Solark integration, register map, MQTT/API details | **`docs/solark-rs485-integration.md`** |
| Parity vs 10.10.53.32, topics | **`docs/solark-10.10.53.32-parity.md`** |
| Broker / solar server (change server later) | **`docs/mqtt-solar-server-reference.md`** |
| No credentials in repo | **`docs/local-config-and-secrets.md`** |

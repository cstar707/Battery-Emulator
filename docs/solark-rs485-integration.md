# Solark RS485 Integration (Battery-Emulator)

This document describes the integration of **Solark inverter data over RS485** into the modified Battery-Emulator firmware (e.g. branch `t-can485-solark`).

## Replace 10.10.53.32

**The plan is to replace the ESP32 at 10.10.53.32 with this firmware.** The replacement board must do **everything** the 10.10.53.32 device does (all sensors read, all switches write, primary and slave, web/HA parity). See **`docs/solark-10.10.53.32-parity.md`** for the full parity checklist and status.

**ESP setup (focused):** **`docs/esp-setup-replacement-10.10.53.32.md`** – build, flash, Settings (WiFi, MQTT, web auth), Solark wiring, and verification.

One board then:

- Runs Battery-Emulator (Tesla battery over CAN, emulates battery to inverter over CAN when using Sol-Ark LV).
- **Uses the same single RS485 port** (previously the only port on 10.10.53.32) to read **Solark** over Modbus RTU.

**Bus topology:** The replacement device has **1 CAN** and **1 RS485** to the Solark and batteries:

- **1 CAN bus** — Shared with the Tesla batteries and the Sol-Ark inverter. This bus carries battery BMS traffic, inverter traffic, and this board (emulator) talking to both. Battery and inverter control use CAN only.
- **1 RS485** — To the **Solark** (Modbus RTU, read and write capable). This firmware uses that port to poll the inverter for monitoring data when `FEATURE_SOLARK_ON_MAIN_RS485` is set; writes (e.g. for inverter switches) can be added when needed.

Build with **`FEATURE_SOLARK_ON_MAIN_RS485`** (e.g. `lilygo_330` env in this repo) and select a **CAN inverter** (e.g. Sol-Ark LV). The main RS485 is used only for Solark; no second RS485 hardware is required.

### Device name and settings (match 10.10.53.32)

When built with **`FEATURE_SOLARK_ON_MAIN_RS485`**, the firmware uses **defaults that match the ESPHome device** so Home Assistant and network naming stay consistent:

| Setting | Default (this build) | ESPHome equivalent |
|--------|------------------------|---------------------|
| **WiFi hostname** | `esphome-web-7a7e60` (if HOSTNAME is not set) | `devicename: "esphome-web-7a7e60"` |
| **HA device name** | `sunsynk` | `friendly_name: "sunsynk"` |
| **HA device ID** | `sunsynk` | — |
| **MQTT object ID prefix** | `sunsynk_` | — |
| **Solark entity names** | `sunsynk Battery SOC`, `sunsynk Grid Power`, etc. | `${friendly_name} Battery SOC` |

Enable **Customized MQTT topics** in Settings so these defaults are used (MQTT topic name, object ID prefix, HA device name, HA device ID). You can override any of them in the web UI. The MQTT topic name stays **BE** unless you change it.

**MQTT:** There is a single MQTT configuration (Settings → MQTT). The same broker and credentials are used for both Battery Emulator (status, cells, etc.) and Solark (e.g. **solar/solark**, HA discovery). See **`docs/mqtt-server-and-access.md`** for the full configuration section and how to access the broker.

**Full device inventory:** A catalog of every sensor and switch on the live device (http://esphome-web-7a7e60.local/) and what the replacement firmware covers is in **`docs/solark-10.10.53.32-device-inventory.md`**.

## Goals

- **Read Solark inverter data over RS485** (Modbus RTU), same as the 10.10.53.32 device.
- Expose that data in the firmware **datalayer** (and optionally MQTT / web UI) for microgrid coordination, SOC management, and Solis↔Solark tier-charging logic.
- Keep **Sol-Ark LV CAN** protocol unchanged (emulator still talks to Sol-Ark over CAN as battery); RS485 is used for **monitoring** the Solark unit (e.g. the 64 kWh asset in the off-grid microgrid).

## Sol-Ark RS485 / Modbus (reference)

- **Protocol:** Modbus RTU. The link is **read and write capable**. The 10.10.53.32 device already exposes **write switches** in its web UI (see http://esphome-web-7a7e60.local/ and `docs/solark-10.10.53.32-device-inventory.md`): Toggle Grid Charge, Generator Charge, Force Generator, Solar sell, System Timer, Priority Load, and Prog1–6 Grid Charge (primary and slave). The replacement firmware currently only **reads** (FC 0x03); write support (FC 0x06 / 0x10) can be added to replicate those switches.
- **Settings:** 9600 baud, 8N1, **Slave ID 0x01** (fixed on Sol-Ark).
- **Function code (read):** 0x03 (Read Holding Registers). **Write (when implemented):** 0x06 (single register), 0x10 (multiple registers) for bitmask/setpoint writes.
- **Hardware:** RS485 A+/B-; 120 Ω termination on master side; GND required between inverter and master.
- **Note:** Inverter must be in "BMS Lithium Batt" mode; Modbus/RS485 battery communication cannot be used at the same time (CAN battery is compatible).

Reference: Sol-Ark Modbus RTU Protocol documentation (e.g. V1.1/V1.4); see also [chuyskywalker/solark-esphome](https://github.com/chuyskywalker/solark-esphome) for an ESPHome example of reading Sol-Ark over Modbus.

## Firmware design

### Datalayer

- **`datalayer_extended.solark_rs485`** holds all Solark data read over RS485:
  - Power (charge/discharge), voltage, current, SOC (if provided by inverter).
  - Timestamp of last successful read; validity flag for use by microgrid logic.

### Solark RS485 reader

- **Module:** `communication/solark_rs485/` (e.g. `solark_rs485.h`, `solark_rs485.cpp`).
- **Role:** Modbus RTU **client** (FC 0x03) to poll Solark at a configurable interval (e.g. every 2–5 s).
- **Serial – two modes:**
  1. **Replace 10.10.53.32 (main RS485):** When `FEATURE_SOLARK_ON_MAIN_RS485` is defined and the selected inverter is **CAN** (e.g. Sol-Ark LV), the reader uses the **main RS485** (Serial2) for Solark. No second port needed.
  2. **Dedicated second RS485:** If HAL exposes non-NC **Solark RS485** pins, the reader uses that UART (e.g. Serial1); main RS485 stays for inverter Modbus/BYD/KOSTAL.
- **Conflict avoidance:** Solark on main RS485 is only enabled when `inverter->interface_type() == Can`, so it is never shared with a Modbus/RS485 inverter.

### Register map (10.10.53.32 ESPHome / SunSynk–Sol-Ark compatible)

The device at 10.10.53.32 runs **ESPHome** (config in `docs/esphome-10.10.53.32-solark.yaml`). All are **holding registers**; scaling below matches that YAML. The firmware reads one block **start 167, count 25** (addresses 167–191) and maps into the datalayer.

| Addr | Index | Use | Type | Scaling / notes |
|------|-------|-----|------|------------------|
| 167 | 0 | Grid LD Power | S_WORD | W |
| 168 | 1 | Grid L2 Power | S_WORD | W |
| 169 | 2 | Grid Power | S_WORD | W → `grid_power_W` |
| 172 | 5 | Grid CT Power | S_WORD | W |
| 175 | 8 | Inverter Power | S_WORD | W |
| 176–178 | 9–11 | Load L1, L2, Load Power | S_WORD | W → `load_power_W` from 178 |
| 182 | 15 | Battery Temperature | S_WORD | offset -1000, ×0.1 °C |
| 183 | 16 | Battery Voltage | U_WORD | ×0.01 V → `battery_voltage_dV` = reg/10 |
| 184 | 17 | Battery SOC | U_WORD | % → `battery_soc_pptt` = reg×100 |
| 186–187 | 19–20 | PV1 Power, PV2 Power | U_WORD | W → `pv_power_W` = 186+187 |
| 190 | 23 | Battery Power | S_WORD | W → `battery_power_W` |
| 191 | 24 | Battery Current | S_WORD | ×0.01 A → `battery_current_dA` = reg/10 |

Other registers in the ESPHome YAML (read separately if needed): 79 Grid freq (×0.01 Hz), 150 Grid V (×0.1), 160 Grid I (×0.01), 154/164/193 inverter V/I/freq, 194 grid connected, 70–78/81/84–85/96/108 energy, 250–279 settings. The firmware currently does not read the second inverter (slave 0x02); only primary 0x01 is polled.

### Integration in main loop

- **Init:** `solark_rs485_init()` from `setup()` if Solark RS485 pins are configured; init the chosen UART and DE pin if used.
- **Poll:** From the main loop (or a dedicated task), call `solark_rs485_poll()` at the chosen interval. The poll sends FC 0x03 requests, reads responses, and updates `datalayer_extended.solark_rs485`.
- **No RS485 receiver registration:** The Solark path is a **client** (we send requests and read responses in the poll function), so it does not use the existing `receive_rs485()` / `Rs485Receiver` chain (that is for the Modbus **server** responding to the inverter).

## Hardware (T-Connect / multi-RS485)

- T-Connect (e.g. LILYGO T-CAN485) may expose multiple RS485 channels. Wiring and pinout are kept **local only** (see project wiring doc, not in GitHub).
- To enable Solark RS485 on a board with a second transceiver, override in the board HAL (e.g. `hw_lilygo.h` or a T-CAN485–specific HAL) the optional pins:
  - `SOLARK_RS485_TX_PIN()`, `SOLARK_RS485_RX_PIN()`, `SOLARK_RS485_DE_PIN()` (return the GPIOs for the second RS485 transceiver). Default in base `hal.h` is `GPIO_NUM_NC` (feature disabled).
- When all three are NC, `solark_rs485_init()` and `solark_rs485_poll()` are no-ops and no second UART is used.

## MQTT and API: same data, both interfaces

When Solark RS485 is enabled, the firmware exposes Solark data via **both MQTT and HTTP API**, so you can replace 10.10.53.32 whether your stack uses MQTT, HTTP scraping, or both.

### MQTT

Data is published to MQTT on the same schedule as the rest of the emulator (e.g. every 5 s). See **`docs/mqtt-server-and-access.md`** for broker config (board Settings → MQTT).

- **Topic:** **solar/solark** (same as today so Solark sensors appear unchanged).
- **Payload:** JSON with `available`, `last_read_millis`, parsed fields (`battery_power_W`, `battery_soc_pptt`, `battery_voltage_dV`, `battery_current_dA`, `grid_power_W`, `load_power_W`, `pv_power_W`) and **`raw_registers`** (array of holding registers 167–191).

### HTTP API

- **GET /solark_data** — Returns the same JSON as **solar/solark** (all parsed fields + `raw_registers`). Use for dashboards or one-shot clients.
- **GET /sensor/sunsynk_\<id\>** — ESPHome-style per-sensor endpoints so the solar server can keep using the same URLs as 10.10.53.32 (e.g. `http://<board-ip>/sensor/sunsynk_battery_soc`). Response: `{"id":"sensor/sunsynk_...", "state": "51 %"}`. Supported ids: `sunsynk_battery_soc`, `sunsynk_battery_voltage`, `sunsynk_battery_current`, `sunsynk_battery_power`, `sunsynk_total_battery_power`, `sunsynk_grid_power`, `sunsynk_load_power`, `sunsynk_total_solar_power`, `sunsynk_pv1_power`, `sunsynk_pv2_power`, `sunsynk_slave_pv1_power`, `sunsynk_slave_pv2_power` (slave returns 0 until slave poll is implemented). Same web auth as the rest of the UI.

### Home Assistant (replacing 10.10.53.32 ESPHome)

The **original 10.10.53.32 device runs ESPHome**, not MQTT: entities are exposed via the **ESPHome native API**. After replacing it with Battery-Emulator:

1. **Remove** the ESPHome device/integration for 10.10.53.32.
2. **Add MQTT sensors** (or use firmware MQTT discovery) with `state_topic: "solar/solark"` and value_templates that match the units below. Raw registers are in `value_json.raw_registers` (indices 0–24 = addresses 167–191).

| Entity | Unit | value_template / note |
|--------|------|------------------------|
| Battery SOC | % | `{{ value_json.battery_soc_pptt \| float / 100 }}` |
| Battery Voltage | V | `{{ value_json.battery_voltage_dV \| float / 10 }}` |
| Battery Current | A | `{{ value_json.battery_current_dA \| float / 10 }}` |
| Battery Power | W | `{{ value_json.battery_power_W }}` |
| Grid Power | W | `{{ value_json.grid_power_W }}` |
| Load Power | W | `{{ value_json.load_power_W }}` |
| PV Power | W | `{{ value_json.pv_power_W }}` |

- **MQTT discovery:** Enable MQTT and **Home Assistant autodiscovery** in Settings so Solark sensors are created under the same device as the Battery Emulator.
- **Reference:** **`docs/home-assistant-10.10.53.32-solark.yaml`** (migration notes); **`docs/esphome-10.10.53.32-solark.yaml`** (original ESPHome register map).

## Matching 10.10.53.32

The device at **10.10.53.32** runs **ESPHome** (Solark/SunSynk RS485 logger). It is the reference for “how we get Solark data on RS485”. To match it:

1. Use the same **protocol** (Modbus RTU, 9600 8N1, slave 0x01, FC 0x03).
2. Use the same **register set** and scaling (if you have access to that device’s config or logs).
3. **Send all Modbus data to MQTT** – parsed fields plus `raw_registers` (indices 0–24) on **solar/solark** so HA or other consumers can mirror the previous entity semantics.
4. **Poll interval:** ESPHome uses 20 s and 150 ms command throttle; firmware uses `SOLARK_POLL_INTERVAL_MS` (default 3 s). Increase to 20 s in platformio.ini or hal if you need to match exactly.

## Files touched / added

- `docs/solark-rs485-integration.md` (this file).
- `Software/src/datalayer/datalayer_extended.h`: struct `DATALAYER_SOLARK_RS485`, member in `DataLayerExtended`.
- `Software/src/communication/solark_rs485/solark_rs485.h`, `solark_rs485.cpp`: Modbus client, poll, init.
- HAL (e.g. `Software/src/devboard/hal/hal.h`, board HAL): optional `SOLARK_RS485_TX_PIN`, `SOLARK_RS485_RX_PIN`, `SOLARK_RS485_DE_PIN()` (default NC).
- `Software/Software.cpp`: call `solark_rs485_init()` in `setup()`, `solark_rs485_poll()` in the main loop at the chosen interval.
- `Software/src/devboard/mqtt/mqtt.cpp`: `publish_solark_rs485()` – publishes **solar/solark** (all Modbus data + raw_registers) and optional Home Assistant sensor discovery for Solark.
- **Solark debug page (web UI):** From the main menu, **Solark debug** opens a live view of all Solark RS485 data (like 10.10.53.32). The page polls `/solark_data` every 2 s and shows status, parsed values (battery power/SOC/voltage/current, grid/load/PV power), and raw Modbus registers. Files: `Software/src/devboard/webserver/solark_html.h`, `solark_html.cpp`; routes `/solark` and `/solark_data` in `webserver.cpp`.

The register map is taken from the 10.10.53.32 ESPHome config; see the table in "Register map (10.10.53.32 ESPHome)" above.

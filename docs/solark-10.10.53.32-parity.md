# 10.10.53.32 full parity – replacement board must do everything the device does

**Goal:** Everything the 10.10.53.32 device does (http://esphome-web-7a7e60.local/) must be done by the replacement board (Battery-Emulator + Solark RS485 on the same CAN/RS485 setup).

Reference: **`docs/solark-10.10.53.32-device-inventory.md`** (full list of entities from the live UI).

---

## Topic parity with 10.10.53.32 – are we covering the same topics?

**Short answer: 10.10.53.32 does not publish MQTT.** It uses **ESPHome** (native API + web). So there are no “MQTT topics from .32” to match. We publish **solar/solark** (same topic as today), so Solark sensors appear unchanged; anything that expects **solar/solark** or **solar/solark/sensors/*** gets data from the board.

| Source | Transport | Topics / endpoints |
|--------|-----------|--------------------|
| **10.10.53.32** | ESPHome native API + web (HTTP) | No MQTT. Entities like `sunsynk_battery_soc` are exposed via ESPHome API; the solar server may scrape e.g. `http://10.10.53.32/sensor/sunsynk_battery_soc`. |
| **Solar server** (e.g. unified API) | MQTT **publish** | Gets Solark data from .32 via **HTTP**, then publishes **solar/dashboard**, **solar/solark**, **solar/solark/sensors/<name>**, **solar/solark/summary**. So the “same data as .32” appears on **solar/*** topics, published by the server. |
| **Replacement (BE board)** | MQTT **publish** | Publishes **solar/solark** (same topic as today; one JSON: `battery_power_W`, `battery_soc_pptt`, `battery_voltage_dV`, `battery_current_dA`, `grid_power_W`, `load_power_W`, `pv_power_W`, `raw_registers`). Solark sensors appear as they are today. |

The board publishes **solar/solark** (same topic as today), so Solark sensors appear unchanged; no server change needed.


**Data/sensor coverage vs .32:** We publish the **core** subset (battery/grid/load/PV power, SOC, voltage, current + raw_registers 167–191). We do **not** yet publish: slave inverter data, energy (day/total), temperatures, frequencies, status bits, prog times, or switches. So we do not yet cover **all** the same **entities** (sensors/switches) as .32; see the checklist below.

---

## Parity checklist

### 1. Device identity and naming ✅

| Item | 10.10.53.32 | Replacement | Status |
|------|--------------|-------------|--------|
| Hostname | esphome-web-7a7e60 | Default when HOSTNAME unset (FEATURE_SOLARK_ON_MAIN_RS485) | ✅ |
| HA device name | sunsynk | Default sunsynk | ✅ |
| HA entity names | sunsynk Battery SOC, etc. | Discovery uses device_name + suffix | ✅ |
| MQTT topic / object ID | — | BE, sunsynk_ | ✅ |

### 2. Primary inverter – read (sensors)

| Item | 10.10.53.32 | Replacement | Status |
|------|--------------|-------------|--------|
| Core power/SOC/V/I | Battery, Grid, Load, PV power, SOC, V, I | One block 167–191 → MQTT + raw_registers | ✅ |
| Extra sensors | Energy (day/total), temps, frequencies, status bits, prog times, AUX, Essential/Nonessential, Rated Power, System Health, raw regs 16–17, 274–280 | Not yet read or published | ❌ |
| Expose same list to MQTT/HA | All primary sensors | Only core + raw_registers today | ❌ |

**Work:** Extend Modbus reads (more blocks or addresses), extend datalayer/MQTT with all primary sensor fields so HA/UI see the same entities.

### 3. Slave inverter – read (sensors)

| Item | 10.10.53.32 | Replacement | Status |
|------|--------------|-------------|--------|
| Poll address 0x02 | Full duplicate sensor set (Slave Battery, Grid, Load, etc.) | Not polled | ❌ |
| Publish slave data | All “sunsynk Slave …” entities | None | ❌ |

**Work:** Add slave poll (0x02), same register map; add **solar/solark_slave** (or nested in solar/solark) and HA entities for slave.

### 4. Primary inverter – write (switches)

| Item | 10.10.53.32 | Replacement | Status |
|------|--------------|-------------|--------|
| Toggle Grid Charge | 232 bitmask 1 | No write | ❌ |
| Toggle Generator Charge | 231 bitmask 1 | No write | ❌ |
| Toggle Force Generator | 326 bitmask 8192 | No write | ❌ |
| Toggle Solar sell | 247 bitmask 1 | No write | ❌ |
| Toggle System Timer | 248 bitmask 1 | No write | ❌ |
| Toggle Priority Load | 243 bitmask 1 | No write | ❌ |
| Prog1–Prog6 Grid Charge | 274–279 bitmask 1 each | No write | ❌ |

**Work:** Implement Modbus write (FC 0x06/0x10) in `solark_rs485`; expose switches via web UI and/or MQTT (and HA switch discovery).

### 5. Slave inverter – write (switches)

| Item | 10.10.53.32 | Replacement | Status |
|------|--------------|-------------|--------|
| Same 12 toggles for address 0x02 | Slave Toggle Grid Charge, etc. | No write, no slave | ❌ |

**Work:** Same as primary but target slave 0x02; expose “sunsynk Slave …” switches in UI/MQTT/HA.

### 6. Web UI

| Item | 10.10.53.32 | Replacement | Status |
|------|--------------|-------------|--------|
| Single table: Name \| State \| Actions | All sensors + switches with Off/On | Solark debug page: live data, no full table, no switch toggles | ❌ |
| Theme (☀️ 🌒) | Optional | Optional | — |

**Work:** Solark page (or equivalent) with full sensor list and switch toggles matching 10.10.53.32 (primary + slave), or document that MQTT/HA provide parity and web is secondary.

### 7. Home Assistant

| Item | 10.10.53.32 | Replacement | Status |
|------|--------------|-------------|--------|
| All sensors as entities | ESPHome native API | MQTT discovery for core Solark; rest manual or not yet | ⚠️ |
| All switches as entities | ESPHome native API | No switch discovery yet | ❌ |

**Work:** MQTT discovery (and/or REST) for all sensors and switches (primary + slave) so HA has the same entity set after migration.

---

## Summary

| Area | Status | Action |
|------|--------|--------|
| Naming / identity | ✅ Done | — |
| Primary read (core) | ✅ Done | — |
| Primary read (full sensors) | ❌ | Add reads + datalayer + MQTT for energy, temps, frequencies, status, prog times, etc. |
| Slave read | ❌ | Add poll 0x02 + datalayer + MQTT |
| Primary write (12 switches) | ❌ | Add Modbus write + web/MQTT/HA switches |
| Slave write (12 switches) | ❌ | Add writes to 0x02 + web/MQTT/HA switches |
| Web UI (full table + toggles) | ❌ | Extend Solark page or match via MQTT/HA |
| HA (all entities) | ⚠️ Partial | Extend discovery for full sensor list + all switches |

**Implementation order (suggested):**  
1) Slave poll + slave data to MQTT.  
2) Modbus write for primary switches + expose in web UI and MQTT.  
3) Modbus write for slave switches.  
4) Extended primary (and slave) reads for remaining sensors; then full web table and HA discovery.

---

## Gap check – ensure nothing is missing

Cross-check against the live UI (http://esphome-web-7a7e60.local/) and ESPHome config so we don’t miss any capability.

### Entity types on 10.10.53.32

| Type | On device / in config | In parity checklist? | Notes |
|------|------------------------|------------------------|--------|
| **Sensors** (read-only) | All primary + slave (battery, grid, load, inverter, PV, energy, temps, frequencies, prog times, AUX, Essential/Nonessential, Rated Power, System Health, totals) | ✅ Yes (sections 2, 3) | Full list in device inventory. |
| **Binary sensors** | Grid Connected (194), Gen Peak Shaving (280 bit 0x10), Grid Peak Shaving (280 bit 0x100); primary + slave | ✅ Yes (part of “sensors” / status bits) | Explicitly include in “extra sensors” and slave: read register 194, 280; expose as on/off. |
| **Switches** (write) | 12 primary (232, 231, 326, 247, 248, 243, 274–279) + 12 slave | ✅ Yes (sections 4, 5) | — |
| **Raw/internal** | grid_peak_shaving_raw, reg_16_raw, reg_17_raw, reg_274_raw … reg_279_raw (primary + slave) | ⚠️ Partial | We publish raw_registers for 167–191. Device also exposes 16, 17, 274–279, 280 raw. For full parity, either extend raw block(s) or derive from same reads as switches. |
| **number** | ESPHome config comment mentioned “number” section | ❓ Confirm | If the original YAML has number components (e.g. setpoints), they may appear in HA or web as inputs. Check HA entity list for “number” entities; add to parity if present. |
| **text_sensor** | ESPHome config comment mentioned “text_sensor” | ❓ Confirm | Check HA for text_sensor entities; add if present. |
| **select** | ESPHome config comment mentioned “select” | ❓ Confirm | Check HA for select/dropdown entities; add if present. |

### Other capabilities

| Capability | 10.10.53.32 | Replacement | In parity? |
|------------|-------------|-------------|------------|
| **Web server** (auth, port 80) | Yes | Yes (Battery-Emulator web UI) | ✅ |
| **API / HTTP** | ESPHome API + custom services: `get_system_status`, `get_all_sensor_data` | We have `/solark_data` JSON; no named “services” | ⚠️ Optional: add GET endpoints or MQTT that mirror “all sensor data” / “system status” if HA or other clients rely on them. |
| **WiFi + AP fallback** | Yes | Yes | ✅ |
| **OTA** | ESPHome OTA | Firmware update by other means | ✅ (different mechanism, not Solark parity) |
| **Time** | homeassistant + SNTP | Board has NTP elsewhere | ✅ |
| **Captive portal** | Yes (fallback AP) | Board has AP | ✅ |

### Action items from gap check

1. **Binary sensors:** When implementing “extra sensors”, include register 194 (Grid Connected) and 280 (Gen/Grid Peak Shaving bits) as explicit on/off entities for primary and slave.
2. **Raw registers:** Extend reads or MQTT so we expose the same raw registers the device shows (16, 17, 274–279, 280; primary + slave), or document that 167–191 raw is sufficient.
3. **number / text_sensor / select:** In Home Assistant (or the device’s web UI), check whether any entities are type number, text_sensor, or select. If yes, add them to the inventory and parity checklist and implement equivalent (e.g. MQTT number for setpoints, or select for mode).
4. **API services:** If anything calls `get_system_status` or `get_all_sensor_data`, the replacement can document that `GET /solark_data` (and MQTT **solar/solark** / solar/solark_slave when implemented) provide the same data.

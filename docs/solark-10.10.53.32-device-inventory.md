# 10.10.53.32 device inventory (from live ESPHome web UI)

This document catalogs what appears at **http://esphome-web-7a7e60.local/** so the Battery-Emulator replacement can match it. **Goal: the replacement board must do everything this device does** (all reads, all writes, primary + slave, web/HA). See **`docs/solark-10.10.53.32-parity.md`** for the parity checklist and implementation status.

## Device

- **Title:** esphome-web-7a7e60 ❤  
- **Subtitle:** Solark Inverter RS485 Logger  
- **Entities:** One large table with columns **Name** | **State** | **Actions**
- **API (ESPHome):** Native API for HA; custom services in YAML: `get_system_status`, `get_all_sensor_data`. Replacement can mirror via `GET /solark_data` and MQTT **solar/solark** (and slave when implemented).

---

## Write switches (Actions column: Off/On toggle on 10.10.53.32)

All of these appear on the live 10.10.53.32 interface at **http://esphome-web-7a7e60.local/** in the table’s **Actions** column as Off/On toggles. They are **Modbus holding-register writes** (bitmask or value). The Modbus link is read/write capable; the replacement firmware should replicate this set when Modbus write (FC 0x06/0x10) is implemented. Today the replacement **only reads** and does not send writes. To match the device, we’d need MQTT or web UI buttons that trigger FC 0x06/0x10 to these registers.

### Primary inverter (sunsynk)

| Entity name | Register | Notes |
|-------------|----------|--------|
| sunsynk Toggle Grid Charge | 232 | bitmask 1 |
| sunsynk Toggle Generator Charge | 231 | bitmask 1 |
| sunsynk Toggle Force Generator | 326 | bitmask 8192 (bit 13) |
| sunsynk Toggle Solar sell | 247 | bitmask 1 |
| sunsynk Toggle System Timer | 248 | bitmask 1 |
| sunsynk Toggle Priority Load | 243 | bitmask 1 |
| sunsynk Prog1 Grid Charge | 274 | bitmask 1 |
| sunsynk Prog2 Grid Charge | 275 | bitmask 1 |
| sunsynk Prog3 Grid Charge | 276 | bitmask 1 |
| sunsynk Prog4 Grid Charge | 277 | bitmask 1 |
| sunsynk Prog5 Grid Charge | 278 | bitmask 1 |
| sunsynk Prog6 Grid Charge | 279 | bitmask 1 |

### Slave inverter (sunsynk Slave …)

Same set of toggles for address **0x02**: Slave Toggle Grid Charge, Slave Toggle Generator Charge, Slave Toggle Force Generator, Slave Toggle Solar sell, Slave Toggle System Timer, Slave Toggle Priority Load, Slave Prog1–Prog6 Grid Charge.

---

## Binary sensors (read-only, ON/OFF state)

On the device these show as State ON/OFF with no Actions toggle:

- **sunsynk Grid Connected Status** (register 194)
- **sunsynk Gen Peak Shaving Status** (register 280, bit 0x10)
- **sunsynk Grid Peak Shaving Status** (register 280, bit 0x100)
- **sunsynk Slave …** same three for address 0x02

For parity, expose these as binary/on-off entities (read register 194 and 280; apply bitmasks for slave too).

---

## Sensors (read-only, no Actions)

### Primary (sunsynk) – we map a subset to MQTT

- **Battery:** Battery Power, SOC, Voltage, Current, Temperature; Charge/Discharge Limit Current; Equalization/Absorption/Float voltage  
- **Grid:** Grid Power, Grid LD Power 167, Grid L2 Power 168, Grid CT Power, Grid Voltage, Grid Current, Grid Frequency; Grid Connected Status; Gen/Grid Peak Shaving Status  
- **Load:** Load Power, Load L1/L2 Power, Load Frequency  
- **Inverter:** Inverter Power, Voltage, Current, Frequency; Rated Power  
- **PV:** PV1/PV2 Power, Voltage, Current; Solar Power (PV1+PV2)  
- **AUX / Essential / Nonessential:** AUX Power, Essential Power, Essential Power 1, Nonessential Power, Nonessential Power 1  
- **Energy:** Day/Total Battery Charge/Discharge, Day/Total Grid Import/Export, Day/Total Load Energy, Day/Total PV Energy  
- **Temperature:** DC Transformer Temperature, Radiator Temperature  
- **Program times:** Prog1–Prog6 Time (read)  
- **System:** System Health  
- **Raw (internal):** grid_peak_shaving_raw, reg_16_raw, reg_17_raw, reg_274_raw … reg_279_raw  

**Replacement today:** We read 167–191 (one block), publish battery/grid/load/PV power, SOC, voltage, current, and `raw_registers` on **solar/solark**. We do **not** yet publish the extra sensors above (energy, temps, frequencies, status bits, prog times) or any **slave** data.

### Slave (sunsynk Slave …)

Full duplicate set of sensors for the second inverter (address 0x02): Slave Battery/Grid/Load/Inverter/PV/AUX/Energy/Temperatures/Prog times/raw registers.  

**Replacement today:** We only poll **primary 0x01**. Slave 0x02 is not polled; no slave data in MQTT.

---

## UI-only

- **☀️ 🌒 Scheme** – theme toggle (light/dark) at bottom of page. Not an inverter entity.

---

## Possible additional entity types (confirm in HA or full YAML)

The original ESPHome config referred to **number**, **text_sensor**, and **select** sections. The live web UI at 10.10.53.32 shows one table (sensors + switches only). If in Home Assistant you have any **number**, **text_sensor**, or **select** entities under the sunsynk device, list them here and add to the parity checklist in **`docs/solark-10.10.53.32-parity.md`** so the replacement can implement equivalents (e.g. MQTT number for setpoints, select for mode).

---

## Replacement coverage summary

| Category | On 10.10.53.32 | In replacement firmware |
|----------|-----------------|-------------------------|
| **Primary sensors (core)** | Battery P/SOC/V/I, Grid P, Load P, PV P, temps, energy, etc. | ✅ Battery/Grid/Load/PV power, SOC, V, I + `raw_registers` (167–191). No energy, temps, frequencies, status bits, or prog times yet. |
| **Slave sensors** | Full duplicate set for 0x02 | ❌ Not implemented (single-inverter poll only). |
| **Primary switches** | 12 toggles (Grid/Gen/Force Gen/Solar sell/Timer/Priority Load, Prog1–6 Charge) | ❌ Read-only; no Modbus write support. |
| **Slave switches** | Same 12 toggles for 0x02 | ❌ Not implemented. |
| **Device name / HA** | sunsynk, esphome-web-7a7e60 | ✅ Defaults when built with `FEATURE_SOLARK_ON_MAIN_RS485`. |

To reach full parity (everything 10.10.53.32 does): add **slave inverter poll** (0x02), **full primary (and slave) sensor set** (energy, temps, frequencies, status, prog times, etc.), **Modbus write** for all switch registers (231, 232, 243, 247, 248, 274–279, 326) for primary and slave, and expose everything via **web UI** and **MQTT/HA**. Track progress in **`docs/solark-10.10.53.32-parity.md`**.

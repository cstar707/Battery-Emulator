# Tesla 12V DC Rail: CAN Message Spec & Web UI Display

This spec documents how the Tesla PCS (Power Conversion System) reports 12V DC rail voltage and current over CAN, and how to display these values in the web UI.

---

## 1. CAN Message Source

| Field | Value |
|-------|-------|
| **CAN ID** | `0x2B4` (692 decimal) |
| **DBC Name** | `PCS_dcdcRailStatus` |
| **Source** | Tesla PCS (in the pack) – transmitted when DCDC is active |
| **Direction** | Battery → Vehicle (received by our firmware) |

The PCS sends this message periodically when the 12V support / DCDC converter is active. If the car is asleep or DCDC is off, the message may stop and values will remain at last-known (or zero).

---

## 2. Signal Layout (0x2B4 PCS_dcdcRailStatus)

Byte layout and scaling from the Tesla DBC:

| Signal | Start bit | Length | Scale | Offset | Unit | Range | Raw → Physical |
|--------|-----------|--------|-------|--------|------|-------|----------------|
| **PCS_dcdcLvBusVolt** (12V voltage) | 0 | 10 bits | 0.0390625 | 0 | V | 0–39.96 | `raw × 0.0390625` |
| **PCS_dcdcHvBusVolt** (HV voltage) | 10 | 12 bits | 0.146484 | 0 | V | 0–599.85 | `raw × 0.146484` |
| **PCS_dcdcLvOutputCurrent** (12V current) | 24 | 12 bits | 0.1 | 0 | A | 0–400 | `raw × 0.1` |

### Byte extraction (little-endian)

```
Byte 0–1: LV bus voltage (10 bits)
  raw = ((data.u8[1] & 0x03) << 8) | data.u8[0]
  voltage_V = raw * 0.0390625

Byte 1–2: HV bus voltage (12 bits, overlaps byte 1)
  raw = ((data.u8[2] & 0x3F) << 6) | ((data.u8[1] & 0xFC) >> 2)
  voltage_V = raw * 0.146484

Byte 3–4: LV output current (12 bits)
  raw = ((data.u8[4] & 0x0F) << 8) | data.u8[3]
  current_A = raw * 0.1
```

### Example

- 12V = 15.43 V → raw ≈ 395  
- HV = 347.02 V → raw ≈ 2370  
- 12V current = 78.10 A → raw ≈ 781  

---

## 3. Data Flow (Implementation)

```
CAN 0x2B4 (rx frame)
    ↓
TESLA-BATTERY.cpp :: handle_incoming_can_frame() case 0x2b4
    ↓
battery_dcdcLvBusVolt, battery_dcdcHvBusVolt, battery_dcdcLvOutputCurrent (raw uint16)
    ↓
TESLA-BATTERY.cpp :: update_values() → datalayer_extended.tesla.*
    ↓
datalayer_extended.tesla.battery_dcdcLvBusVolt
datalayer_extended.tesla.battery_dcdcHvBusVolt
datalayer_extended.tesla.battery_dcdcLvOutputCurrent
    ↓
TESLA-HTML.h :: get_status_html() → scaled floats + HTML
    ↓
Web UI (status page)
```

---

## 4. Datalayer Storage

| Datalayer field | Type | Storage | Notes |
|-----------------|------|---------|-------|
| `datalayer_extended.tesla.battery_dcdcLvBusVolt` | uint16_t | Raw | 12V bus voltage |
| `datalayer_extended.tesla.battery_dcdcHvBusVolt` | uint16_t | Raw | HV bus voltage |
| `datalayer_extended.tesla.battery_dcdcLvOutputCurrent` | uint16_t | Raw | 12V output current |

**Scaling in UI:**

```cpp
float dcdcLvBusVolt = battery_dcdcLvBusVolt * 0.0390625f;   // 12V
float dcdcHvBusVolt = battery_dcdcHvBusVolt * 0.146484f;    // HV
float dcdcLvOutputCurrent = battery_dcdcLvOutputCurrent * 0.1f;  // 12V current
```

---

## 5. Current Web UI Display

**Location:** Tesla battery status HTML (`TESLA-HTML.h` → `get_status_html()`)

**Current labels (lines 282–284):**

```
PCS Lv Output: <current> A
PCS Lv Bus: <voltage> V
PCS Hv Bus: <voltage> V
```

These appear in the battery-specific status section, grouped with other PCS/BMS data.

---

## 6. Suggested UI Improvements

To make the 12V DC rail more visible and user-friendly:

1. **Clearer labels**
   - "PCS Lv Bus" → "12V DC Rail Voltage"
   - "PCS Lv Output" → "12V DC Rail Current"
   - "PCS Hv Bus" → "HV Bus Voltage" (optional, for context)

2. **Optional: main dashboard**
   - Add 12V voltage and current to the main status/dashboard page if one exists, so users see them without drilling into the Tesla battery page.

3. **Optional: computed power**
   - 12V power (W) = voltage × current.

4. **Optional: low-voltage warning**
   - Already implemented: `EVENT_12V_LOW` when `dcdcLvBusVolt < 11.7 V` (see `TESLA-BATTERY.cpp` lines 507–512).

---

## 7. File Reference

| File | Purpose |
|------|---------|
| `Software/src/battery/TESLA-BATTERY.cpp` | CAN parsing (case 0x2b4), datalayer update, 12V low check |
| `Software/src/battery/TESLA-BATTERY.h` | Variable declarations |
| `Software/src/datalayer/datalayer_extended.h` | Datalayer struct definitions |
| `Software/src/battery/TESLA-HTML.h` | Web UI HTML generation |

---

## 8. Summary

| Item | Value |
|------|-------|
| CAN ID | 0x2B4 |
| 12V voltage | Raw × 0.0390625 → V |
| 12V current | Raw × 0.1 → A |
| HV voltage | Raw × 0.146484 → V |
| Display | Tesla battery status page (PCS Lv/Hv section) |

The data path is already implemented and displayed. The spec above documents the CAN format and scaling for future reference and for any UI changes (e.g. clearer labels or dashboard placement).

---

## Appendix: Getting Data Like the Tesla Python Connector

The **Tesla Model 3/Y Python Connector** (and similar tools like ScanMyTesla, tesLAX, S3XY-candump) display the same underlying data. Here's how it compares to our setup.

### Data source: same CAN bus

All of these tools read from the **Tesla CAN bus**. The BMS and PCS inside the pack broadcast the same messages regardless of who is listening.

| Tool | Hardware | Connection point |
|------|----------|------------------|
| **Tesla Python Connector** | OBD-II adapter (e.g. OBDLink, Panda) | Tesla diagnostic port (center console, behind rear seat) |
| **Our firmware** | ESP32 devboard + CAN transceiver | Pack CAN (direct to pack in stationary setup, or via diagnostic port) |

### CAN IDs we already parse (mapped to Python Connector fields)

| Python Connector field | Our CAN ID | Our datalayer / display |
|------------------------|------------|------------------------|
| `low_voltage` (12V) | 0x2B4 | `battery_dcdcLvBusVolt` → "PCS Lv Bus" |
| `output_current` (12V) | 0x2B4 | `battery_dcdcLvOutputCurrent` → "PCS Lv Output" |
| `high_voltage` (HV) | 0x2B4 | `battery_dcdcHvBusVolt` → "PCS Hv Bus" |
| `nominal_full_pack_energy` | 0x352 | `battery_nominal_full_pack_energy` |
| `nominal_energy_remaining` | 0x352 | `battery_nominal_energy_remaining` |
| `ideal_energy_remaining` | 0x352 | `battery_ideal_energy_remaining` |
| `energy_to_charge_complete` | 0x352 | `battery_energy_to_charge_complete` |
| `energy_buffer` | 0x352 | `battery_energy_buffer` |
| `total_charge` | 0x3D2 | `total_charged_battery_Wh` |
| `total_discharge` | 0x3D2 | `total_discharged_battery_Wh` |
| `soc_ave`, `soc_min`, `soc_max`, `soc_ui` | 0x292 | `battery_soc_*` |
| `bms_bat_temp_pct` | 0x292 | `battery_battTempPct` |
| Contactor states | 0x20A, 0x7AA, etc. | Various HVP/BMS status |

### What the Python Connector has that we may not

- **Per-module energy** (`charge_total_module1`–4, etc.): May come from separate BMS module CAN IDs or a different DBC structure. Our 0x3D2 gives pack-level totals only.
- **Per-cell voltages** (`[01]`–`[96]`): Tesla uses 0x332 (BMS_bmbMinMax) mux 1 for min/max brick voltages and brick numbers, but **not** all 96 individual cell voltages on CAN. Full cell-by-cell data may require UDS/diagnostic requests or a different protocol.
- **AC/DC charge breakdown** (`ac_charge_total`, `dc_charge_total`): May come from charger/inverter CAN or separate IDs.

### How to get a similar display

1. **Use our web UI** – The Tesla battery status page already shows most of this data. Improve labels and layout to match the Python Connector style if desired.
2. **Add a dashboard view** – Create a dedicated page that shows 12V rail, HV, SOC, energy totals, and contactor status in a compact, Python-Connector-like layout.
3. **Run the Python Connector** – When the pack is in a car with the diagnostic port accessible, connect an OBD adapter and run the Python tool for a live terminal view.
4. **Add missing parsers** – If you need per-module energy or other signals, capture CAN with a logger (e.g. SavvyCAN, candump) and map the DBC for those IDs, then add parsing in `TESLA-BATTERY.cpp`.

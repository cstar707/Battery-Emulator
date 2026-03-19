# Solis Modbus Register Map + How the App Uses It

This document is a single “source of truth” for the Solis Modbus registers that the `solis_s6_app` **reads** (telemetry) and **writes** (control / curtailment / remote import-export).

It also explains how those registers connect into the UI:
- **External meter power** (what you’re watching as import/export)
- **Inverter mode bits** (what the dashboard labels as “Self-use / Feed-in / grid charge”)
- **Power commands** (what the app sends when you switch between charging/export behaviors)

## 0) Where the app talks to Solis

- Modbus host/port: from `scripts/solis_s6_app/config.py` (`get_solis_host()`, `get_solis_port()`)
- Modbus reads: input registers (FC 0x04) using `_read_input_registers`
- Modbus writes: holding registers (FC 0x06) using `_write_holding`
- Addressing base: the code supports an optional zero-based mode via `SOLIS_MODBUS_ZERO_BASED`

## 1) Power sign conventions (critical)

### 1.1 32-bit signed power registers (export vs import)

In `solis_modbus.py`:
- `_reg_s32_signed()` returns a signed 32-bit value where:
  - **negative** means **export** (for grid meter power) or **battery discharge** (for battery power)
  - **positive** means **import** (for grid meter power) or **battery charge** (for battery power)

### 1.2 External meter power (grid CT)

The app’s external meter power for the Solis inverter is:
- `meter_power_W` = **signed** register data from **input block 33126**, parsed from **register 33130**
- In the code this is described as:
  - `out["meter_power_W"] = _reg_s32_signed(b3, 4)  # 33130 signed (+ import, - export)`

So:
- `meter_power_W > 0` = **import** (grid -> house/battery)
- `meter_power_W < 0` = **export** (house/battery -> grid)

The dashboard then labels export when `grid_power_W < 0`.

### 1.3 Internal PV “active power” vs PV-only register

The app uses:
- `pv_power_W` from the DC/PV register (`33049` block offsets)
- It does **not** fall back to `active_power_W` for “PV” display because at night that value can represent battery export behavior.

## 2) Input register telemetry blocks the app reads

The app reads these input register blocks (see `_BLOCKS` in `solis_modbus.py`):

1. **Block 33000 (41 registers)**
   - Energy totals and metadata (product model, totals)
   - Used for: PV energy totals (e.g. `energy_today_pv_kWh`, `total_pv_energy_kWh`)

2. **Block 33049 (36 registers)**
   - PV and AC measurements
   - Used for:
     - `pv_power_W` = `_reg_s32(b1, 8)` from 33057-33058 (unsigned total DC)
     - `active_power_W` = `_reg_s32_signed(b1, 30)` from 33079-33080 (signed “export negative”)

3. **Block 33091 (5 registers)**
   - Working mode, temperature, grid frequency, inverter state
   - Used for:
     - `grid_freq_Hz`, `inverter_temp_C`, `inverter_state`

4. **Block 33126 (25 registers)**
   - External meter power, battery V/I/SOC, house load, and battery power
   - Used for (key fields):
     - `meter_power_W` (grid CT power)
       - from **register 33130** (signed, +import / -export)
     - `house_load_W`
       - from a house/load power offset inside block 33126 (parsed as `_reg(b3, 21)`)
     - `battery_power_W`
       - from **register 33149-33150** (signed charge/discharge)
     - `battery_soc_pct`, `battery_soh_pct`
       - from offsets inside block 33126

5. **Block 33161 (20 registers)**
   - Energy counters (charge/discharge/import/export/load totals for the day)
   - Used for:
     - `energy_today_grid_import_kWh`, `energy_today_grid_export_kWh`
     - and similar “day_*” energy KPIs

### 2.1 Decoded mirrors of control registers (readback)

After reading the telemetry blocks, the app also reads holding registers:
- `43110` (storage control)
- `43483` (hybrid control)

Then it decodes them into:
- `storage_bits` (bitmask names like `self_use`, `allow_grid_charge`, `feed_in_priority`, etc.)
- `hybrid_bits` (bitmask names like `allow_export`)

## 3) Holding registers the app writes (controls)

The app writes a small set of holding registers for control:

### 3.1 Storage work-mode bitmask: `43110`

Register:
- `43110` = 16-bit bitmask controlling storage/inverter operating modes

The app decodes named bits in `get_storage_control_bits()` and uses bit-level writes in:
- `set_storage_control_bits(changes)`
- `set_storage_control_bit(bit_index, on)`

App-referenced bits (names used in the app):
- `self_use` (bit 0)
- `time_of_use` (bit 1)
- `off_grid` (bit 2)
- `reserve_battery` (bit 4)
- `allow_grid_charge` (bit 5)
- `feed_in_priority` (bit 6)
- `batt_ovc` (bit 7)
- `forcecharge_peakshaving` (bit 8)
- `battery_current_correction` (bit 9)
- `battery_healing` (bit 10)
- `peak_shaving` (bit 11)

The dashboard derives a human readable mode from these bits in `_storage_mode_label()`.

#### Mutual exclusion enforced by the app
The inverter automatically clears one when you set the other, and the app also tries to mirror that:
- `self_use` and `feed_in_priority` are treated as mutually exclusive.

### 3.2 Hybrid function bitmask: `43483`

Register:
- `43483` = 16-bit bitmask controlling hybrid features

The app cares most about:
- `allow_export` (bit 3)

Example:
- “Use all solar” preset clears `allow_export` (loads are prioritized; surplus should not be exported to grid)

### 3.3 PV curtailment (export curtailment): `43070` + `43052`

Register pair:
- `43070` = power limit switch
  - `_POWER_LIMIT_ENABLE` = `0xAA` (170)
  - `_POWER_LIMIT_DISABLE` = `0x55` (85)
- `43052` = power limit percentage
  - `limit_pct = desired_percent * 100` (scale factor in code)
  - `0` => full curtailment (0%)
  - `10000` => 100% (limit disabled / full power restored)

App API:
- `set_active_power_limit(limit_pct)`

Main automation uses it:
- When Solark SOC is high, app sets `limit_pct=0` (PV output limited to 0%)
- When SOC is safe again, app sets `limit_pct=100` to restore full output

### 3.4 Grid charge capability + limits: `43110`, `43117`, `43130`, `43027`, plus remote control `43132/43128`

Grid charge is implemented in two steps in `solis_modbus.py`:

#### Step A: Arm / enable “Allow grid charge”
- `arm_grid_charge(...)` writes:
  - `43110`: enables `allow_grid_charge` by OR-ing `0x20` into the current 43110 value
  - `43135`: writes `0` to avoid RC force charge/discharge overriding remote control
  - `43117`: max charge current (0.1 A units)
  - `43130`: charge limit in 10 W units
  - `43027`: force-charge power limitation in W

#### Step B: Refresh the remote import power (dead-man)
- `_set_remote_import_watts_locked(import_watts)` writes:
  - `43132 = 2` (remote control mode)
  - `43128 = power_val` where:
    - `power_val = -(import_watts // 10) & 0xFFFF`
    - `43128` uses the inverter’s signed convention such that **negative values correspond to import**

App behavior:
- The app normally refreshes remote import periodically (dead-man) so the inverter continues obeying the remote command.

### 3.5 Remote export command: `43110` + `43074` + `43132/43128`

Export is implemented via:
- `set_export_target(export_watts)`

Key registers:
- `43110`: adjusts mode bits so inverter is in a “Self-use + Feed-in” style configuration:
  - It modifies the 43110 register with a mask:
    - `new_mode = (mode & ~0x10) | _MODE_SELF_USE_FEEDIN`
    - `_MODE_SELF_USE_FEEDIN = 0x0041` (self-use + feed-in priority)
- `43074`: EPM export cap
  - `epm_cap = max(114, (export_watts + 99) // 100)`
  - “100 W units” in code comments
- `43135`: written `0` (ensures not force charge/discharge)
- `43132`: written `2` (remote control)
- `43128`: written **positive export power**:
  - `power_val = (export_watts // 10) & 0xFFFF`

So:
- remote export uses `43128` positive
- remote import uses `43128` negative (via `_set_remote_import_watts_locked`)

## 4) How the app connects telemetry to mode + power commands

### 4.1 Telemetry -> dashboard “what is happening”

From input registers:
- `meter_power_W` (signed) becomes `grid_power_W`

The dashboard uses:
- `grid_power_W < 0` => EXPORT label
- `grid_power_W >= 0` => IMPORT label

It also shows:
- `house_load_W`
- `pv_power_W` / `battery_power_W`

So if you see “exporting and feeding the load” you typically have:
- PV/battery power driving AC bus and either the grid meter is in export direction (`meter_power_W < 0`)
- OR the battery is discharging while the inverter runs in a feed-in / export-allowed mode (depending on your exact topology)

### 4.2 “Mode we are in” (43110) vs “what the app is forcing”

There are two different concepts:

1) Inverter mode bits readback (`43110`, `43483`)
   - Used for display and some automation decisions

2) Power command mode (`/api/power-control`, `power_control.json`)
   - Used by `_background_power_control_refresher()` to repeatedly call:
     - `set_grid_charge_limits(...)` for import/charging
     - `set_export_target(...)` for export/feeding the grid
     - or `set_power_control_off()` when mode is `off`

Important:
- The app can show allow_grid_charge/feed-in bits ON, but if `/api/power-control` is `off`, it will not keep the remote power command alive.

## 5) Practical “how to interpret what you see”

When troubleshooting “why did it start exporting”:
1. Check the **meter direction** (Solis external meter):
   - `meter_power_W (33130) > 0` => import
   - `meter_power_W (33130) < 0` => export
2. Check the **mode bits**:
   - `43110` decoded into:
     - `self_use` vs `feed_in_priority`
     - plus `allow_grid_charge`
3. Check the **power-control command**:
   - `/api/power-control` tells you if remote import/export is actively being commanded


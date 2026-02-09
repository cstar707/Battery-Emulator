# Solis Hybrid Inverter – Modbus/RS485 Register Reference

This document is a reference for controlling and monitoring Solis S5/S6 EH1P hybrid inverters (e.g. **S6-EH1P11.4k-H-US**) over **RS485 Modbus RTU**. Use it when implementing Solis inverter control from Battery-Emulator or other controllers.

**Source:** Register map derived from [GrugBus Solis S5 EH1P 6K 2020 CSV](https://github.com/peufeu2/GrugBus/blob/main/grugbus/devices/Solis_S5_EH1P_6K_2020.csv).  
Protocol: **RS485_MODBUS (ESINV-33000ID) energy storage inverter protocol**.  
The inverter typically ships with an RS485 meter cable; connect A/B to your adapter (e.g. USB-RS485 or ESP32 serial).

---

## Overview

- **Read-only (input) registers:** function code **0x04**, base addresses **33xxx**, **35xxx** (model, status, power, battery from BMS, faults).
- **Read/write (holding) registers:** function code **0x03** (read), **0x06** (single write), **0x10** (multi-write); base **43xxx** (settings, force charge/discharge, timed charge/discharge, limits).
- **Coils (discrete status):** function **0x02**, addresses **12501–12594** (fault and status bits; see appendix in official Solis Modbus manual for bit definitions).

All register addresses below are decimal unless noted. Units (0.1 V, 10 W, etc.) are as in the official Solis protocol.

---

## Registers critical for battery control

These are the registers you will use first when adding Solis RS485 control.

### Force charge / discharge (holding, write)

| Addr  | Type | Unit | Name | Description |
|-------|------|------|------|-------------|
| **43135** | U16 | - | Remote Control Force Battery Charge/Discharge | **0** = OFF, **1** = Force charge, **2** = Force discharge. Dead-man: resets to 0 after 5 min unless re-written. When set, Remote Control Grid Adjustment (43132) is set to OFF. |
| **43136** | U16 | 10 W | Remote Control Force Battery Charge Power | Used when 43135 = 1. 10 W steps up to device backup power. Default 0. |
| **43129** | U16 | 10 W | Remote Control Force Battery Discharge Power | Used when 43135 = 2. 10 W steps. Limit registers 43130/43131 override force. |
| **43130** | U16 | 10 W | Battery Charge Power Limit | Limit charge power; 0 = invalid. Takes priority over force registers. Not saved in flash. |
| **43131** | U16 | 10 W | Battery Discharge Power Limit | Limit discharge power; 0 = invalid. Takes priority over force registers. Not saved in flash. |

### Energy storage mode (holding)

| Addr  | Type | Name | Description |
|-------|------|------|-------------|
| **43110** | U16 bitfield | Energy storage mode | BIT00 Self use, BIT01 Time-charging, BIT02 Off-grid, BIT03 Battery wakeup, BIT04 Reserve, BIT05 Allow grid to charge. Example: **35** = Timed charge mode (for 43143 schedule). |
| **43143** | U16 | Timed charge start hour | Hour (0–23). Write **43143–43150** as a block for charge/discharge schedule. |
| **43144** | U16 | Timed charge start minute | |
| **43145** | U16 | Timed charge end hour | |
| **43146** | U16 | Timed charge end minute | |
| **43147** | U16 | Timed discharge start hour | Example: charge 18:00–08:00, discharge 08:00–18:00 → `write_registers(43143, [18,0, 8,0, 8,0, 18,0])`. |
| **43148** | U16 | Timed discharge start minute | |
| **43149** | U16 | Timed discharge end hour | |
| **43150** | U16 | Timed discharge end minute | |

### Battery DCDC enable (holding)

| Addr  | Type | Name | Description |
|-------|------|------|-------------|
| **33203** | U16 | Battery charge/discharge Enable | 1 = Enable, 0 = Disable. (Read from input 33xxx.) |
| **33204** | U16 | Battery charge/discharge Direction | 0 = Charge, 1 = Discharge. |
| **43114**–**43118** | (var) | Battery DCDC enable/direction/current (holding) | Some firmware versions; 43117–43118 = max charge/discharge current (0.1 A), default 70 A. |

### Max charge/discharge current (holding)

| Addr  | Type | Unit | Name | Description |
|-------|------|------|------|-------------|
| **43117** | U16 | 0.1 A | Battery charge current maximum setting | Max 70 A, default 70 A. |
| **43118** | U16 | 0.1 A | Battery discharge current maximum setting | Max 70 A, default 70 A. |
| **43141** | U16 | 0.1 A | Timed charge current | Default 50 A. |
| **43142** | U16 | 0.1 A | Timed discharge current | Default 50 A. |

---

## Export control (grid port power)

Use these registers to **control how much power is exported to the grid** (or imported from it). Enable “Remote Control Grid Adjustment” first, then set active power. Export = positive power to grid; import = negative (power from grid).

### Enable grid port remote control (holding, write)

| Addr  | Type | Name | Description |
|-------|------|------|-------------|
| **43132** | U16 | Remote Control Grid Adjustment | **0** = OFF (no remote power control). **1** = ON at **system grid connection point**. **2** = ON at **inverter AC grid port**. Dead-man: resets to 0 after 5 min unless re-written. When set to ON, Force Battery Charge/Discharge (43135) is set to OFF. |
| **43128** | S16 | 10 W | Remote Control Active Power on Grid Port | **Target active power at grid port.** Positive = **export** to grid, negative = **import** from grid. Unit: 10 W (e.g. 500 = 5000 W export). Only effective when **43132** = 1 or 2. |
| **43133** | S16 | 10 W | Remote Control Active Power on System Grid Connection Point | When **43132 = 1** (system grid point), use this for active power setpoint: + export, − import. |
| **43134** | S16 | 10 W | Remote Control Reactive Power on System Grid Connection Point | When 43132 = 1, reactive power setpoint (10 Var steps). |

So: set **43132 = 1** or **2**, then set **43128** (or **43133** if using system grid point) to control export/import. Re-write within 5 minutes to keep control active.

### EPM (Export Power Management) – cap export (holding)

| Addr  | Type | Unit | Name | Description |
|-------|------|------|------|-------------|
| **43073** | U16 | - | EPM settings | Bitfield: BIT04 = Enable EPM (1=ON), BIT05 = EPM FailSafe, BIT06 = 3-phase balanced vs per-phase, etc. EPM must be enabled for 43074 to limit export. |
| **43074** | U16 | 100 W | EPM Export Power Limit | **Maximum export to grid** (backflow limit). Unit: 100 W. Set in GUI or via Modbus; limits how much the inverter can export. |
| **43070** | U16 | - | Power limit switch | 0xAA = enable power limit, 0x55 = disable (revert to 100%). |
| **43081** | S16 | 10 W | Actual power limit adjustment value | Power limit adjustment; see 43070. Range ±327680 W (10 W steps). |

Use **43074** to cap export (e.g. 50 = 5000 W max export). Use **43128** (with 43132 ON) to set a **target** export or import.

### Per-phase export (3-phase S6 HV only, holding)

| Addr  | Type | Unit | Name | Description |
|-------|------|------|------|-------------|
| **43124** | U16 | 100 W | Reserved Backflow power setting | When 43073 BIT06 = 1 (per-phase control). |
| **43125**–**43127** | U16 | 100 W | Backflow power phase A / B / C | Per-phase export limit (100 W steps). |

### Monitor export (input, read-only)

| Addr  | Type | Unit | Name | Description |
|-------|------|------|------|-------------|
| **33151**–**33152** | S32 | 1 W | Grid port power | **Actual** active power at grid port. Convention: + export to grid, − import from grid (confirm sign in your firmware). Use to close the loop when controlling export. |
| **33247** | S16 | 100 W | EPM backflow power | EPM-measured backflow; + to grid, − from grid. |
| **33249** | S16 | 100 W | EPM real-time backflow power | Real-time backflow. |

**Summary for export control:** Enable remote grid control (**43132** = 1 or 2), set target export with **43128** (or **43133**), optionally cap export with **43074** (EPM). Re-write 43132/43128 within 5 min to keep control. Monitor with **33151–33152**.

---

## Input registers (read-only) – status and measurements

### Model and identity (4, 33xxx / 35xxx)

| Addr  | Type | Name | Description |
|-------|------|------|-------------|
| 35000 | U16 | Inverter model definition | High byte = protocol version, low = model (e.g. 2040 = 1-phase HV hybrid, 2060 = 3-phase HV hybrid). |
| 33000 | U16 | Product model | See Solis appendix (hex). |
| 33001–33003 | U16 | DSP / LCD / Protocol software version | Hex display. |
| 33004–33019 | U16 | Inverter serial number | ASCII in 16-bit words. |
| 33020 | U16 | Initial startup setting flag | 1 = done, 0 = not done. |

### Real-time and power (4, 33xxx)

| Addr  | Type | Unit | Name | Description |
|-------|------|------|------|-------------|
| 33022–33027 | U16 | - | System time | Year, month, day, hour, minute, second. |
| 33049–33056 | U16 | 0.1 V / 0.1 A | MPPT1–4 voltage and current | |
| 33057–33058 | U32 | 1 W | PV power | |
| 33071–33072 | U16 | 0.1 V | DC bus voltage, DC bus half | |
| 33073–33078 | U16 | 0.1 V / 0.1 A | Phase A/B/C voltage and current | |
| 33079–33084 | S32 | 1 W / 1 Var / 1 VA | Active, reactive, apparent power | |
| 33093 | S16 | 0.1 °C | Inverter temperature | |
| 33094 | U16 | 0.01 Hz | Grid frequency | |
| 33095 | U16 | Inverter status | Bitfield – see Solis Appendix 2. |

### Battery (from BMS / inverter) (4, 33xxx)

| Addr  | Type | Unit | Name | Description |
|-------|------|------|------|-------------|
| **33133** | U16 | 0.1 V | Battery voltage | Measured by inverter. |
| **33134** | S16 | 0.1 A | Battery current (BMS) | Magnitude; direction in 33135. Smoothed. |
| **33135** | U16 | - | Battery current direction | 0 = charging, 1 = discharging. |
| **33139** | U16 | 1% | BMS Battery SOC | 100 = 100%. |
| **33140** | U16 | 1% | BMS Battery SOH | 100 = 100%. |
| **33141** | U16 | 0.01 V | BMS Battery voltage | |
| **33142** | S16 | 0.1 A | BMS Battery current | Always positive; lags a few seconds. |
| **33143**–**33144** | U16 | 0.1 A | BMS charge / discharge current limit | |
| **33145**–**33146** | U16 | bitfield | BMS Battery fault 01/02 | Over/under voltage, temp, overcurrent, etc. |
| **33149**–**33150** | S32 | 1 W | Battery power | From BMS. |
| **33151**–**33152** | S32 | 1 W | Grid port power | + export, − import (check unit sign in your integration). |
| **33160** | U16 | - | Battery model | 0x0000 none, 0x0100 PYLON_HV, 0x0200 User, 0x0300 BYD HV, etc. |
| **33161**–**33168** | U32/U16 | kWh / 0.1 kWh | Battery charge/discharge energy | Total, today, yesterday. |
| **33203** | U16 | - | Battery DCDC enable | 1 = Enable, 0 = Disable. |
| **33204** | U16 | - | Battery DCDC direction | 0 = Charge, 1 = Discharge. |
| **33205** | U16 | 0.1 A | Battery DCDC current | Fast; can have noise. |
| **33217** | U16 | 0.1 A | Battery current (inverter) | Measured by inverter; fast update. |

### Fault and operating status (4, 33xxx)

| Addr  | Type | Name | Description |
|-------|------|------|-------------|
| 33116 | U16 | Fault status 1 – Grid | Bitfield (no grid, over/under voltage/freq, etc.). |
| 33117 | U16 | Fault status 2 – Backup | |
| 33118 | U16 | Fault status 3 – Battery | Battery not connected, over/undervoltage, etc. |
| 33119 | U16 | Fault status 4 – Inverter | DC/grid/IGBT/temp, etc. |
| 33120 | U16 | Fault status 5 – Inverter | CAN COM FAIL (bit 14), DSP COM FAIL (bit 15), etc. |
| 33121 | U16 | Operating status | Normal, Initializing, Standby, Limit operation, Battery fault, etc. |
| 33132 | U16 | Energy storage mode | Mirror of mode bitfield (time-charge, off-grid, reserve, etc.). |

### Coils (discrete status, function 0x02)

Addresses **12501–12594** map to individual fault/status bits, e.g.:

- 12501 No grid, 12502 Grid over voltage, … 12533 Battery not connected, 12534 Battery overvoltage, 12535 Battery undervoltage, …
- 12579 **CAN communication failed**, 12580 DSP communication failed,
- 12581 Normal operation, 12582 Initializing, 12583 Controlled shutdown, 12584 Shutdown due to fault, 12585 Standby, 12586 Derating, 12587 Limit operation,
- 12588 Backup overload, 12589 Load status, 12590 Grid status, **12591 Battery status**, 12592 Grid surge warning, 12593 Fan fault.

Exact bit layout is in the official Solis RS485 Modbus manual (Appendix 5/6).

---

## Holding registers (read/write) – settings

### Real-time clock and basic (3, 43xxx)

| Addr  | Type | Name | Description |
|-------|------|------|-------------|
| 43000–43005 | U16 | RTC year, month, day, hour, minute, second | |
| 43006 | U16 | Slave address | 1–99. |
| 43007 | U16 | Power on/off | 0xBE = boot, 0xDE = shutdown. |

### Battery settings (3, 43xxx)

| Addr  | Type | Unit | Name | Description |
|-------|------|------|------|-------------|
| 43010 | U16 | 1% | Battery over charge SOC | |
| 43011 | U16 | 1% | Battery over discharge SOC | Storage menu; can stop discharging. |
| 43012–43013 | U16 | 0.1 A | Battery max charge / discharge current | Often overwritten by BMS/battery type. |
| 43018 | U16 | 1% | Battery Force charge SOC | |
| 43119–43122 | U16 | 0.1 V | Battery under/float/charge/over voltage protection | |
| 43111 | U16 | - | Backup output enabled | 0 = not enabled, 1 = enabled. |
| 43112 | U16 | 0.1 V | Backup reference voltage | Default 230. |
| 43113 | U16 | 0.01 Hz | Backup reference frequency | Default 50. |

### Remote control and limits (3, 43xxx)

| Addr  | Type | Unit | Name | Description |
|-------|------|------|------|-------------|
| **43128** | U16 | 10 W | Remote Control Active Power on Grid Port | + export, − import. |
| **43132** | U16 | - | Remote Control Grid Adjustment | 0 = OFF, 1 = ON (system grid point), 2 = ON (inverter AC port). Dead-man 5 min. |
| **43133**–**43134** | U16 | 10 W | Remote Control Active/Reactive Power (system grid point) | When 43132 ON. |
| **43135**–**43136**, **43129**–**43131** | (see table above) | Force charge/discharge and limits | |

### EPM and power limit (3, 43xxx)

| Addr  | Type | Name | Description |
|-------|------|------|-------------|
| 43070 | U16 | Power limit switch | 0xAA enable, 0x55 disable. |
| 43071 | U16 | Reactive power switch | 0x55 off, 0xA1 reactive ratio, 0xA2 PF. |
| 43073 | U16 | EPM settings | Bitfield: EPM enable, FailSafe, CT, etc. |
| 43074 | U16 | 100 W | EPM export power limit | |
| 43081 | S16 | 10 W | Actual power limit adjustment | |
| 43083 | S16 | 10 Var | Reactive power adjustment | |

### Grid protection (3, 43xxx)

| Addr  | Type | Unit | Name | Description |
|-------|------|------|------|-------------|
| 43090–43097 | U16 | V / 100 ms | Grid over/under voltage and delays | Primary and secondary. |
| 43098–43105 | U16 | 0.1 Hz / 100 ms | Grid over/under frequency and delays | |
| 43106–43107 | U16 | 1 s | Power-on startup time, Failure recovery time | Default 60 s. |

---

## Notes for implementation

1. **Slave address:** Default is often 1; check inverter or Solis manual. Use the same in your Modbus master.
2. **Baud rate / parity:** Common: 9600 8N1 or  Modbus 9600 8E1; confirm in Solis manual for your model.
3. **Dead-man:** 43135 (force charge/discharge) and 43132 (grid adjustment) reset after 5 minutes unless rewritten.
4. **Limits override force:** 43130 and 43131 (charge/discharge power limit) take priority over 43135/43136/43129.
5. **Timed charge:** Set 43110 to timed charge mode (e.g. 35), then write 43143–43150 as one block (e.g. charge 18:00–08:00, discharge 08:00–18:00).
6. **Battery model 33160:** 0x0100 = PYLON_HV; useful to confirm inverter sees a compatible “battery” when using CAN emulation alongside RS485 monitoring.
7. **Export control:** To control export/import: set **43132** = 1 (or 2), then **43128** = target power in 10 W (positive = export). Re-write within 5 min. Optionally cap export with **43074** (EPM, 100 W units). Monitor with **33151–33152**.

---

## References

- [GrugBus Solis S5 EH1P 6K 2020 CSV](https://github.com/peufeu2/GrugBus/blob/main/grugbus/devices/Solis_S5_EH1P_6K_2020.csv) – source for this register list.
- Official Solis/Ginlong **RS485_MODBUS(ESINV-33000ID) energy storage inverter protocol** PDF – for appendices (fault bits, model codes, national standards).
- [Solis S6 control battery discharge via Modbus and openHAB](https://diysolarforum.com/threads/solis-s6-control-battery-discharge-via-modbus-and-openhab.115237/) – community examples.

This project’s inverter: **Solis S6-EH1P11.4k-H-US** (11.4 kW HV, US). Register set is compatible with S5/S6 EH1P series; confirm any model-specific differences in the official protocol document for your firmware version.

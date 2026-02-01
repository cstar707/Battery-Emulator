# Battery Emulator ↔ Tesla BMS: CAN Bus & Contactors

This document consolidates what we know about how Battery Emulator (BE) communicates with Tesla packs over CAN, which messages are used, how contactors are engaged, and how the DC fast charge (FC) contactor path could be controlled. It applies to Tesla Model 3/Y and, where noted, Legacy (Model S/X).

---

## 1. Overview

- **BE’s role:** BE emulates the vehicle side (and optionally the pack side for standalone inverter use). It **sends** the CAN frames the pack/inverter expect from the car, and **receives** the frames the pack/inverter send.
- **Physical interface:** The CAN bus that carries this traffic is the one that in a real car goes to the **X098 connector** (the pack/BMS harness connector). BE connects to that same bus; “through the X098” means the same CAN bus BE is already driving and listening on.
- **No “x098” in code:** The codebase does not reference “x098” or “X098”; that is a hardware/wiring designation.

### 1.1 BE designed for pack out of the car

Battery Emulator is **designed for having the battery out of the car**: pack repurposed for stationary storage, connected to a home/solar inverter. BE emulates the vehicle (or inverter protocol) so the pack and inverter work together. The README and install flow assume: connect BE to the EV battery, connect BE to the inverter, wire HV between inverter and battery — i.e. pack is a separate unit, not inside a running vehicle.

### 1.2 Battery still in the car

If the **battery is still in the car**, you can still use BE in certain ways; the main issue is **who talks on CAN**.

- **Pack in car, car off (e.g. crashed car, or pack in place for testing):** From CAN’s point of view this is the same as “pack out of car”: there is no other node sending vehicle frames. So BE can run as designed — send 0x221, 0x118, etc., and talk to the pack over CAN (e.g. via X098). The car’s gateway/VCU are either disconnected or off.

- **Pack in car, car on / other controllers on CAN:** If the real car (gateway, VCU, etc.) is **also** on the same CAN and sending 0x221, 0x118, 0x232, etc., then **two nodes** would be sending the same IDs. That can cause bus conflicts, inconsistent state, and undefined behaviour. BE is **not** designed for “run in parallel with the full car on the same bus.”

  - **Receive-only (monitoring):** You can use BE **only to read** CAN (receive 0x132, 0x20A, 0x212, etc.) and not send, or only send on a different bus/interface. Then BE doesn’t conflict with the car; you get information from the BMS/pack while the car runs. That’s a monitoring/diagnostic use. **Implemented:** Settings → Hardware config → “Read-only CAN (monitor only, no TX)” — when enabled, BE does not send any CAN messages; it only receives (e.g. 0x132, 0x20A, 0x212). Takes effect after save and reboot.

  - **BE replaces part of the car:** If the car’s gateway or relevant vehicle controllers are **disconnected** and BE is the only node sending vehicle frames to the pack, then again the pack is effectively “standalone” from CAN’s perspective — even though it’s still physically in the car. So “battery still in car” with BE **in charge** of CAN is the same as pack-out-of-car from a protocol standpoint.

**Summary:** BE is intended for pack **out of the car** (stationary storage). With the battery **still in the car**, use BE only when the car’s controllers are off/disconnected (so BE has the bus to itself), or use BE in receive-only mode for monitoring; do not run BE and the full car on the same CAN at the same time.

### 1.3 BMS over CAN: information and control

**Yes — you can get information from and control the original (master) BMS over CAN.**

- **Serial/isoSPI** is only used **inside** the pack: cell boards → master BMS (in the penthouse). That link is not exposed at the pack connector.
- The **interface between the pack and the vehicle** (and BE) is **CAN**. The master BMS (and HVP) in the penthouse speak **CAN** on that bus.

So over CAN you can:

- **Get information from the BMS:** The BMS sends voltage, current, SOC, contactor state, limits, energy, brick min/max, BMS status, HVP info, fault/alert matrices, serial number, etc. — all on CAN (see [§3 CAN Messages: What BE Receives (RX)](#3-can-messages-what-be-receives-rx)). That is how the car (and BE) reads pack state.
- **Control or influence the BMS:** The vehicle sends drive state (0x221), HVIL (0x118), contactor requests (0x232 for FC, implied for main via 0x221/0x118), charge request (0x333), vehicle status (0x3A1), etc. The BMS receives those on CAN and acts (e.g. close contactors, go to charge). See [§2 CAN Messages: What BE Sends (TX)](#2-can-messages-what-be-sends-tx).

What you do **not** get over CAN is the raw isoSPI traffic between cell boards and the master BMS. The BMS **aggregates** that and sends pack-level data on CAN (e.g. 0x132, 0x212, 0x332, 0x401). So: **original BMS ↔ vehicle (or BE) = CAN for both information and control.**

---

## 2. CAN Messages: What BE Sends (TX)

These are the CAN frames BE transmits toward the pack/inverter (the bus that would go to the X098 connector).

### 2.1 Model 3/Y (TeslaBattery)

| CAN ID (hex) | Name / purpose |
|--------------|----------------|
| 0x082 | UI_tripPlanning |
| 0x102 | VCLEFT_doorStatus |
| 0x103 | VCRIGHT_doorStatus |
| 0x118 | DI_systemStatus (HVIL; critical for contactor allow) |
| 0x1CF | Digital HVIL (multi-frame, when user_selected_tesla_digital_HVIL) |
| 0x213 | UI_cruiseControl |
| 0x221 | VCFRONT_LVPowerState (Drive / Accessory / Going down / Off) |
| 0x229 | SCCM_rightStalk |
| 0x241 | VCFRONT_coolant |
| 0x2A8 | CMPD_state |
| 0x2D1 | VCFRONT_okToUseHighPower |
| 0x2E1 | VCFRONT_status (multiple muxes) |
| 0x2E8 | EPBR_status |
| 0x284 | UI_vehicleModes |
| 0x293 | UI_chassisControl |
| 0x313 | UI_powertrainControl |
| 0x321 | VCFRONT_sensors |
| 0x333 | UI_chargeRequest (charge enable, termination %, etc.) |
| 0x334 | UI request |
| 0x3A1 | VCFRONT_vehicleStatus (e.g. bmsHvChargeEnable) |
| 0x3B3 | UI_vehicleControl2 |
| 0x3C2 | VCLEFT_switchStatus |
| 0x39D | IBST_status |
| 0x55A | Unknown; always sent |
| 0x602 | BMS UDS diagnostic response |
| 0x7FF | GTW_carConfig (gateway config, Gen3; user-configurable) |

**0x25D CP_status** (charge port status) is defined in BE but not sent for Model 3/Y (comment: not necessary for standalone pack operation).

### 2.2 Legacy (Model S/X — TeslaLegacyBattery)

| CAN ID (hex) | Name / purpose |
|--------------|----------------|
| 0x21C | Charger status |
| 0x25C | Fast charge status |
| 0x2C8 | GTW status |
| 0x20E | Charge port status |
| 0x408 | Keep alive |

---

## 3. CAN Messages: What BE Receives (RX)

These are the CAN frames BE listens for (from pack/inverter on the same bus).

### 3.1 Model 3/Y

| CAN ID (hex) | Name / purpose |
|--------------|----------------|
| 0x132 | HVBattAmpVolt (pack voltage/current) |
| 0x20A | HVP_contactorState (pack + FC contactor state, HVIL, etc.) |
| 0x212 | BMS_status (contactor state, BMS_hvState, BMS_chargeRequest, etc.) |
| 0x224 | PCS_dcdcStatus |
| 0x252 | BMS_powerAvailable (limits) |
| 0x292 | BMS_socStatus |
| 0x2A4 | PCS_thermalStatus |
| 0x2B4 | PCS_dcdcRailStatus |
| 0x2D2 | BMSVAlimits |
| 0x2C4 | PCS_logging |
| 0x300 | BMS_info |
| 0x310 | HVP_info |
| 0x312 | BMS_thermalStatus |
| 0x320 | BMS_alertMatrix |
| 0x332 | BMS_bmbMinMax (brick min/max) |
| 0x352 | BMS_energyStatus |
| 0x392 | BMS_packConfig |
| 0x3C4 | PCS_info |
| 0x3D2 | BMS_kwhCounter |
| 0x401 | BrickVoltages (cell stats) |
| 0x612 | UDS requests (BE replies on 0x602) |
| 0x72A | BMS_serialNumber |

### 3.2 Legacy

Same general set where applicable; BE also receives 0x212 (BMS_status), 0x20A equivalent behavior via Legacy frames, etc.

---

## 4. Contactor Engagement (Main Pack Contactors)

BE does **not** send a single “close contactors” CAN command. Engagement is determined by vehicle state and HVIL (and optionally GPIO).

### 4.1 With a real Tesla pack (CAN only)

The **pack (BMS/HVP)** closes the contactors when it sees valid vehicle state and HVIL. BE sends:

- **0x221 (VCFRONT_LVPowerState)** — BE sends **Drive** when:
  - `inverter_allows_contactor_closing == true`
  - BMS not in FAULT
  - No equipment stop
- **0x118 (DI_systemStatus)** — Sent every 10 ms so the pack sees valid system status (HVIL satisfied).
- **Digital HVIL (S/X 2024+):** When `user_selected_tesla_digital_HVIL` is true, BE also sends **0x1CF** and **0x118** (multi-frame).

The pack then closes its contactors and reports state on **0x20A (HVP_contactorState)** and **0x212 (BMS_status)**. BE only **receives** those; it does not send 0x20A or 0x212.

**Summary:** The effective “command” to make the main contactors engage (with a real pack) is **sending 0x221 DRIVE + 0x118** (and 0x1CF if digital HVIL) while the inverter allows closing.

### 4.2 With BE driving contactors via GPIO

When contactor control is done by BE over GPIO:

- BE sets **`battery_allows_contactor_closing`** when the same conditions hold (inverter allows, no fault, no equipment stop).
- **`comm_contactorcontrol.cpp`** drives **precharge**, **positive**, and **negative** contactor pins when both `battery_allows_contactor_closing` and `inverter_allows_contactor_closing` are true.

So the “command” that makes contactors engage in this case is **GPIO** toggled by the contactor control module when both flags are true.

---

## 5. DC Fast Charge (FC) Contactor

### 5.1 Current state in BE

- **FC contactor state** is **reported by the pack** in **0x20A (HVP_contactorState)**. BE only **receives** 0x20A; it does not send it.
- **0x212 (BMS_status)** includes **BMS_hvState** (e.g. DOWN, COMING_UP, GOING_DOWN, UP_FOR_DRIVE, UP_FOR_CHARGE, **UP_FOR_DC_CHARGE**, UP). BE only receives 0x212.
- BE does **not** send any CAN message that directly commands the FC contactors. There is no dedicated “DC fast charge contactor” GPIO in BE today.

### 5.2 Model 3/Y: The missing command — 0x232 BMS_contactorRequest

Research (see [tesla-3y-dc-charge-contactor-can-research.md](tesla-3y-dc-charge-contactor-can-research.md)) shows:

- The **vehicle** tells the pack to open/close the **FC contactors** by sending **0x232 BMS_contactorRequest** on the Vehicle bus.
- Key signal: **BMS_fcContactorRequest** (bits 0–2, values 0–5). The pack uses this to close/open the DC fast charge contactors.
- **BE does not currently send 0x232** for Model 3/Y. To support “DC charge path on/off” from BE, we would add TX of 0x232 and set **BMS_fcContactorRequest** (and related signals) accordingly.
- The exact value encoding (which of 0–5 = open vs closed) is not documented in public DBCs; it would need to be captured from a real car during DC charge or bench testing.
- Fault **BMS_a086_SW_FC_Contactor_Mismatch** occurs when **BMS_fcContactorRequest** (commanded) does not match **HVP_fcContactorSetState** (actual); DC charging is then terminated.

### 5.3 Legacy (Model S/X)

BE **sends** **0x25C (fast charge status)** and **0x20E (charge port status)**. If the Legacy pack uses those to decide “go to DC charge” and close FC contactors, then sending the right 0x25C/0x20E could indirectly influence the FC path. That is pack firmware dependent.

### 5.4 Implementation outline for 0x232 (Model 3/Y)

1. Add 0x232 TX in Tesla battery code: define frame **BMS_contactorRequest** (ID 0x232, DLC 8), map **BMS_fcContactorRequest** (bits 0–2), **BMS_packContactorRequest** (bits 3–5), **BMS_fcLinkOkToEnergizeRequest** (bits 32–33), and other signals as needed.
2. Drive from a “DC charge path requested” setting or state: when “on”, set **BMS_fcContactorRequest** to the value for “request FC closed” (to be determined by logging); when “off”, set to “open”.
3. Optionally send **0x25D CP_status** when DC charge is “on” and keep **0x333** / **0x3A1** consistent with charging.
4. Validate with 0x232/0x20A logs and watch for BMS_a086_SW_FC_Contactor_Mismatch.

Full detail, signal layout, and internet sources: **[tesla-3y-dc-charge-contactor-can-research.md](tesla-3y-dc-charge-contactor-can-research.md)**.

---

## 6. Does the BMS talk to the controller in the trunk?

**Yes** — but not over a dedicated link. They share the same CAN (and possibly gateway).

- **BMS location:** The “BMS” that speaks CAN is the **master BMS** in the pack **penthouse** (with the contactors, HVP, etc.). Cell modules talk to it over **serial/isoSPI**, not CAN.
- **Trunk/rear controller:** In Model 3/Y the **rear drive inverter (DIR)** is in the rear (trunk area). It’s one of the main powertrain controllers.
- **How they talk:** BMS and rear drive inverter are both on the **same powertrain CAN bus** (or reach each other via the **Gateway (GTW)**). So the BMS and the “controller in the trunk” (rear inverter) **do** talk over CAN — by both being on the vehicle CAN network. There is no separate BMS‑to‑trunk wire; it’s the shared bus (the one that goes to X098 from the pack side).
- **In BE terms:** The CAN bus BE drives (e.g. to X098) is that same network. So when BE sends 0x221, 0x118, etc., it’s emulating what the rest of the car (including gateway, rear inverter, etc.) would see; when BE receives 0x20A, 0x212, 0x132, etc., that’s what the pack (and thus the BMS) sends to the vehicle, including to controllers like the rear inverter.

**References:** Community docs (e.g. Tinkla Tesla CAN Bus) describe a Powertrain CAN with BMS, DI (drive inverter), CHG/FC, DCDC, GTW. DIY Electric Car / openinverter threads describe CAN from the master BMS in the penthouse to the vehicle and isoSPI/serial from cell boards to the master BMS.

---

## 7. References

### Codebase

- **TX:** `Software/src/battery/TESLA-BATTERY.cpp` — `TeslaBattery::transmit_can()`
- **RX:** `Software/src/battery/TESLA-BATTERY.cpp` — `TeslaBattery::handle_incoming_can_frame()`
- **Contactor logic:** `Software/src/battery/TESLA-BATTERY.cpp` (vehicleState, 0x221, 0x118), `Software/src/communication/contactorcontrol/comm_contactorcontrol.cpp` (GPIO)
- **Legacy TX:** `Software/src/battery/TESLA-LEGACY-BATTERY.cpp` — `TeslaLegacyBattery::transmit_can()`

### External

- [joshwardell/model3dbc](https://github.com/joshwardell/model3dbc) — Model3CAN.dbc (Model 3/Y CAN IDs and signals, including 0x232).
- [Tessie – BMS_a086_SW_FC_Contactor_Mismatch](https://stats.tessie.com/alerts/BMS_a086_SW_FC_Contactor_Mismatch) — Fault when BMS_fcContactorRequest and HVP_fcContactorSetState disagree.
- [Model 3 CAN bus IDs and data (Google Sheets)](https://docs.google.com/spreadsheets/d/1ijvNE4lU9Xoruvcg5AhUNLKr7xYyHcxa8YSkTxAERUw) — Community CAN ID reference.
- Tesla Service Bulletin CD-20-17-001 — Model 3/Y third-party CAN interface (X181, 500 kbps).

---

## 8. Related docs

- **[tesla-3y-dc-charge-contactor-can-research.md](tesla-3y-dc-charge-contactor-can-research.md)** — Detailed 0x232 BMS_contactorRequest research, signal layout, implementation outline, and internet sources for DC fast charge contactor control.

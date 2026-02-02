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

### 1.4 X098 connector (penthouse)

X098 is the **pack-side** connector on the penthouse harness (vehicle harness to pack). **CAN bus:** cavity **6** = CAN high (black, 0.13 mm²), cavity **7** = CAN low (green, 0.13 mm²) → X935M; 500 kbps. Full pinout, wire colors, and destinations: **[tesla-x098-connector-pinout.md](tesla-x098-connector-pinout.md)**.

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

## 7. Bridge / split-bus design (pack in car)

When the pack is still in the car and you insert a **bridge** (two CAN channels: pack side and gateway side), you can control what the pack sees and what the car sees. This section covers **breaking HVIL** so only BE can allow contactors, **what the other gateway signals are used for**, and **never sending or forwarding a pyro fuse blow command**.

### 7.1 Break HVIL and keep it BE-only

- **Goal:** So the **car cannot open (or close) the contactors** — only BE controls when contactors are allowed.
- **How:** **Break the HVIL path** from the car to the pack. The BMS closes contactors only when it sees HVIL closed. If HVIL is broken on the car side, the car can’t satisfy HVIL. On the **pack side**, close HVIL only from **BE** (e.g. BE drives a relay or jumper that completes the HVIL loop to the pack when BE wants contactors allowed). So:
  - Car side: HVIL open (or not connected to pack).
  - Pack side: HVIL closed only when BE closes it → BMS sees “HVIL OK” only when BE allows → **only BE controls contactor allow**; the car cannot open contactors by pulling HVIL.
- **Note:** HVIL on Model 3/Y is not on X098 in the pinout we have; it runs through the vehicle harness (e.g. multipin/X0 to drive unit). You need to identify the HVIL wires in the harness that goes to the pack and break them on the **car side** of the bridge, and drive the pack-side HVIL from BE only.

### 7.2 Other signals from the gateway — what they’re used for

We don’t have Tesla’s schematic signal names for each X098 cavity. From connector destinations and typical EV harnesses, the **other** (non-CAN) signals from X098 are likely used for:

| Likely use | X098 cavity / destination | Notes |
|------------|---------------------------|--------|
| **12 V power** | 8 → X050B (YE 0.75 mm²) | Pack or gateway 12 V feed. |
| **Ground** | 9 → G032 (BK 0.75) | Pack/vehicle ground. |
| **Gateway signals** | 12 → X051; 3, 18 → X908M | Wake, presence, or other logic to/from gateway. |
| **Other signals** | 1 → X952M; 10, 11, 13 → X936M; 15, 16 → X050A | Unknown without schematic; could be wake, HVIL sense, or other. |
| **HVIL** | Not identified on X098 | HVIL usually in harness to drive unit / multipin; break and drive from BE as in §7.1. |
| **Pyro trigger** | Unknown — could be hardwire or CAN | See §7.3; must never reach pack from car. |

**Recommendation:** When building the bridge, **pass through** only what you need for the car to “see” the pack (e.g. 12 V, ground, and any wake/presence the gateway expects). For any wire that could be **pyro trigger** or **HVIL from car**, **do not connect** car side to pack side — break it and drive pack side from BE only (HVIL) or leave open (pyro). Trace Tesla’s schematic for your build to identify which cavity/wire is pyro and HVIL before final wiring.

### 7.3 Do not send or forward pyro fuse blow command

- **Pyro fuse:** Tesla packs have a **pyrotechnic disconnect** (pyro fuse) that **permanently** opens the HV path when fired (crash or severe fault). It is **one-time** and **irreversible**.
- **BE today:** BE only **receives** pyro-related **status** from the pack (e.g. 0x20A, 0x212: `pyroFuseBlown`, `pyroFuseFailedToBlow`, `passivePyroDeploy`, `HVP_gpioPassivePyroDepl`, etc.). BE **does not send** any CAN message that commands the pack to blow the pyro.
- **Bridge rules:**
  1. **Never forward** from car → pack any CAN message that could command a pyro blow (if such an ID exists in Tesla’s network, it must be **blocked** on the bridge when forwarding to the pack).
  2. **Never send** from BE (or the bridge) any CAN frame that could trigger the pyro. BE’s TX list (0x082, 0x118, 0x221, 0x232, 0x333, etc.) does not include a “blow pyro” command in the open/community docs; keep it that way and do not add one.
  3. **Hardwire:** If the pyro squib is triggered by a **hardwired** signal from the car (e.g. crash/airbag module), that wire must **not** be connected from car to pack across the bridge — **break it** on the car side so the car can never fire the pack’s pyro. Only the pack’s own BMS/fault logic could then fire it (e.g. internal fault); we do not modify that.
- **Summary:** Ensure the car cannot blow the pyro (block CAN and break hardwire if present), and BE/bridge never send a pyro blow command.

### 7.4 How the crash signal is sent

The **crash signal** (the trigger that can lead to pyro fuse fire or contactor open) is sent **to** the pack from the vehicle. From the codebase and typical automotive practice:

- **Pack reports crash detected:** The pack (HVP) **sends** on CAN **0x212** (BMS_status) a bit **HVP_gpioCrashSignal** (byte 1, bit 3). That is a **status** bit: “HVP has detected a crash signal on its GPIO.” So the HVP has a **hardware input** (GPIO) that is driven by the vehicle when a crash is detected.
- **Conclusion:** The crash signal **to** the pack is almost certainly **hardwired** from the **Restraint Control Module (RCM)** / airbag / crash module to the pack (HVP). The RCM drives that line when a collision is detected; the HVP reads it as a GPIO and then reports **HVP_gpioCrashSignal** on 0x212 and may open contactors or fire the pyro. It is **not** sent as a CAN command from car to pack in the open/community docs we have; the trigger path is **hardwire**.
- **For the bridge:** To prevent the car from ever triggering the pack’s pyro or crash logic, **break the crash hardwire** between the car (RCM/crash module) and the pack. Do not connect that wire across the bridge. The pack will then never see a crash signal from the car; only the pack’s own internal fault logic could still fire the pyro (e.g. severe internal fault), which we do not modify.

### 7.5 Crash signal: which pin, 12V or GND, and can we safely cut the wire?

- **Official BE wiki:** The [Battery: Tesla Model S 3 X Y](https://github.com/dalathegreat/Battery-Emulator/wiki/Battery:-Tesla-Model-S-3-X-Y) wiki only lists required X098 connections: Pins 1, 3, 8, 9, 15, 16, 18. It does **not** mention Pin 13 or the crash signal; in the standard BE setup Pin 13 is therefore **left unconnected** (not wired to BE). The text below is an **additional recommendation** for bridge/harness builds or when Pin 13 would otherwise float.

- **Which pin:** On the **pack/harness connector pinout** you provided, the crash signal is **Pin 13: DI: CRASH SIGNAL** (Digital Input). Same connector: Vehicle CAN = Pins 15 (CANH), 16 (CANL); Charge Port CAN = Pins 7 (CANH), 6 (CANL); Pin 12 = DI: PCS LOCKOUT; Pin 11 = DO: CHARGE PORT LATCH EN; Pin 10 = DI: CHARGE PORT FAULT; Pin 18 = VIN: CONTACTOR POWER 12V; Pin 9 = GND: BAT; Pin 8 = VIN: BAT 12V; Pins 1/3 = AO/AI: REAR INVERTER. This may be the harness that mates with X098 or a related pack connector. (Previously we had not identified the crash pin in our X098 cavity table.) For X098 cavity correspondence, see Tesla's Electrical Reference (e.g. a dedicated RCM‑to‑pack or pyro harness; Tesla service notes describe the pyro disconnect as having its own connector). To know for sure you must use **Tesla’s Electrical Reference** for your Model/YOP (e.g. HV Battery & HVIL section) and trace the crash/pyro signal from RCM to pack.

- **12V or GND:** The pinout labels Pin 13 as **DI: CRASH SIGNAL** (Digital Input) but does **not** state whether it is active‑high (e.g. 12 V = crash) or active‑low (e.g. GND = crash). You need the schematic or measurement to confirm. We do not recommend terminating the pack side; the correct level is unknown without the schematic or measurement.

- **Can we safely just cut the wire?** **Not recommended** to only cut and leave the **pack side** of the wire **floating**:
  - If the HVP expects “no crash” = **low** (e.g. pull‑down), floating might be read as undefined or, on some inputs, as **high** → possible **false crash** detection.
  - If the HVP expects “no crash” = **high** (e.g. pull‑up), floating might be read as **low** → again possible misinterpretation.
  - Leaving the pack side floating can **cause a false crash** (or fault) and is not safe.

- **Bridge wiring:** **Break** the crash wire so the **car side** (RCM) is **disconnected** from the pack — the car can never drive the line. Do **not** connect that wire across the bridge to the pack. The pack side of Pin 13 is left as per BE (unconnected).

---

## 8. References

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

## 9. Related docs

- **[tesla-3y-dc-charge-contactor-can-research.md](tesla-3y-dc-charge-contactor-can-research.md)** — Detailed 0x232 BMS_contactorRequest research, signal layout, implementation outline, and internet sources for DC fast charge contactor control.

# Tesla X098 connector — penthouse pinout (Model 3 / Model Y)

This document describes the **X098** connector on the **penthouse** of the Tesla Model 3 / Model Y high-voltage battery pack. It is the **pack-side** connector: the vehicle harness plugs into the pack here. Battery Emulator (BE) connects to the same CAN bus that this connector carries.

**Primary reference:** For full wiring, safety, HVIL, pyro fuse, pre-charge, and troubleshooting, use the official **[Battery: Tesla Model S 3 X Y](https://github.com/dalathegreat/Battery-Emulator/wiki/Battery:-Tesla-Model-S-3-X-Y)** wiki. This doc adds a detailed cavity/pin table, crash signal (Pin 13), and pack-specific resistor notes (Model 3 2020 RWD / Model Y 2023 AWD) that complement the wiki.

**Scope:** Model 3 and Model Y; pinout below is from Tesla’s **Model 3** Electrical Reference (prog-56). Model Y uses the same or very similar connector; for a specific Model Y year/build, confirm in the [Model Y Electrical Reference](https://service.tesla.com/docs/ModelY/ElectricalReference/) (choose your SOP).

---

## 1. Connector identification

| Item | Value |
|------|--------|
| **Designator** | X098 |
| **Location** | Penthouse (pack); vehicle harness to pack |
| **Connector** | SUMITOMO 6189-7077 |
| **Tesla part number** | 1092471-00-A |
| **Connector color** | GY (gray) |
| **Cavities** | 26 |
| **Terminals** | SUMITOMO (see table) |
| **Wire seals** | SUMITOMO 7165-1312, 7165-1423, 7165-0342, 7165-0343 as per cavity |

**Source:** [Tesla Model 3 Electrical Reference – X098](https://service.tesla.com/docs/Model3/ElectricalReference/prog-56/connector/x098/).

---

## 2. Pinout (cavity, wire, destination)

| Cavity | Terminal P/N | Terminal size | Wire color | Wire size (mm²) | Wire destination | Dest. cavity |
|--------|--------------|---------------|-----------|------------------|-------------------|--------------|
| 1 | 8100-3455 | 0.64×0.64FS | RD (red) | 0.35 | X952M | 8 |
| 2 | — | — | — | — | *unused* | — |
| 3 | 8100-3455 | 0.64×0.64FS | TN (tan) | 0.35 | X908M | 19 |
| 4 | — | — | — | — | *unused* | — |
| 5 | — | — | — | — | *unused* | — |
| **6** | **8240-0336** | **0.64×0.64FS** | **BK (black)** | **0.13** | **X935M** | **2** |
| **7** | **8240-0336** | **0.64×0.64FS** | **GN (green)** | **0.13** | **X935M** | **1** |
| 8 | 8100-0461 | 2.3×0.64FS | YE (yellow) | 0.75 | X050B | 4 |
| 9 | 8100-0461 | 2.3×0.64FS | BK (black) | 0.75 | G032 | 1 |
| 10 | 8240-0336 | 0.64×0.64FS | YE (yellow) | 0.13 | X936M | 17 |
| 11 | 8240-0336 | 0.64×0.64FS | TN (tan) | 0.13 | X936M | 13 |
| 12 | 8100-3455 | 0.64×0.64FS | GN (green) | 0.35 | X051 | 42 |
| 13 | 8100-3455 | 0.64×0.64FS | WH (white) | 0.35 | X936M | 25 |
| 14 | — | — | — | — | *unused* | — |
| 15 | 8100-3455 | 0.64×0.64FS | GN (green) | 0.35 | X050A | 10 |
| 16 | 8100-3455 | 0.64×0.64FS | VT (violet) | 0.35 | X050A | 9 |
| 17 | — | — | — | — | *unused* | — |
| 18 | 8100-0461 | 2.3×0.64FS | TN (tan) | 1.00 | X908M | 12 |
| 19 | — | — | — | — | *unused* | — |
| 20 | — | — | — | — | *unused* | — |
| 21 | — | — | — | — | *unused* | — |
| 22 | — | — | — | — | *unused* | — |
| 23 | — | — | — | — | *unused* | — |
| 24 | — | — | — | — | *unused* | — |
| 25 | — | — | — | — | *unused* | — |
| 26 | — | — | — | — | *unused* | — |

**Wire color abbreviations:** BK = black, BN = brown, GN = green, GY = gray, RD = red, TN = tan, VT = violet, WH = white, YE = yellow.

---

## 3. CAN bus on X098

The **powertrain CAN** bus that the BMS and vehicle use is on X098 at:

| Cavity | Signal | Wire color | Wire size | Destination |
|--------|--------|------------|-----------|-------------|
| **6** | **CAN high** | **Black (BK)** | **0.13 mm²** | **X935M cavity 2** |
| **7** | **CAN low** | **Green (GN)** | **0.13 mm²** | **X935M cavity 1** |

- **Speed:** 500 kbps (typical for this bus).
- **Role:** This is the CAN bus that Battery Emulator connects to when talking to the pack (send 0x221, 0x118, etc.; receive 0x132, 0x20A, 0x212, etc.). In the car, the same bus goes to the gateway (X935M) and other powertrain nodes.

**For BE wiring:** Connect your CAN transceiver **CAN_H** to cavity **6** (black) and **CAN_L** to cavity **7** (green); reference ground as needed (e.g. cavity 9 is G032 ground, but confirm grounding strategy for your setup).

---

## 4. Other used cavities — what they are and where they go

All used cavities except 6 and 7 (CAN) are listed below with wire color, size, destination connector, and destination cavity. Where known from Tesla’s connector docs, the **destination role** is noted (e.g. G032 = ground splice; X051 = gateway). Exact signal names (e.g. 12 V, HVIL, wake) require Tesla’s full schematic for each destination.

| X098 cavity | Wire color | Wire size (mm²) | Destination | Dest. cavity | Destination role / notes |
|-------------|------------|-----------------|-------------|--------------|---------------------------|
| 1 | RD (red) | 0.35 | X952M | 8 | X952M is an intermediate harness; cavity 8 goes to X098 cavity 1 (this wire). X952M also feeds X908M, X050A, X051, G032. Likely signal or low-current power path. |
| 3 | TN (tan) | 0.35 | X908M | 19 | X908M: gateway/BMS harness. Signal path to vehicle. |
| 8 | YE (yellow) | 0.75 | X050B | 4 | X050B: distribution connector (SUMITOMO 6098-8947). Cavity 4 receives from X098 cavity 8. 0.75 mm² suggests power feed (e.g. 12 V). X050B also goes to X926M, X954M, X562, X272, X297, X494, X561. |
| 9 | BK (black) | 0.75 | G032 | 1 | **G032: ground splice.** All wires on G032 are black; it’s a common ground node. X098 cavity 9 is **pack/vehicle ground**. Use for BE ground reference if needed. |
| 10 | YE (yellow) | 0.13 | X936M | 17 | X936M: signal harness. Thin wire = signal (not main power). |
| 11 | TN (tan) | 0.13 | X936M | 13 | X936M: signal harness. |
| 12 | GN (green) | 0.35 | X051 | 42 | **X051: gateway connector.** Signal from pack to gateway. |
| 13 | WH (white) | 0.35 | X936M | 25 | X936M: signal harness. |
| 15 | GN (green) | 0.35 | X050A | 10 | X050A: distribution connector. Signal path. |
| 16 | VT (violet) | 0.35 | X050A | 9 | X050A: distribution connector. Signal path. |
| 18 | TN (tan) | 1.00 | X908M | 12 | X908M: gateway/BMS harness. 1.00 mm² = heavier current (e.g. 12 V power or HVIL feed). |

**Summary by destination**

- **G032** — Ground splice. **X098 cavity 9 (BK 0.75)** = pack/vehicle ground.
- **X050B** — Distribution; **X098 cavity 8 (YE 0.75)** = power feed (likely 12 V).
- **X051** — Gateway; **X098 cavity 12 (GN 0.35)** = signal to gateway.
- **X908M** — Gateway/BMS harness; **X098 cavities 3 (TN 0.35)** and **18 (TN 1.00)** = signal and heavier feed.
- **X936M** — Signal harness; **X098 cavities 10, 11, 13** (YE, TN, WH) = signals.
- **X050A** — Distribution; **X098 cavities 15, 16** (GN, VT) = signals.
- **X952M** — Intermediate harness; **X098 cavity 1 (RD 0.35)** = from pack to X952M, then to X908M and others.

**HVIL:** HVIL is not identified by cavity on X098 in this reference. It typically runs through the vehicle harness (e.g. multipin/X0 connectors to the drive unit). For HVIL bypass or measurement, use Tesla’s schematic for the harness that connects to X908M, X050A, or the drive-unit connectors.

**Source for destination roles:** Tesla Model 3 Electrical Reference prog-56 (X098, X952M, X050B, G032). X051 and X908M are commonly referenced as gateway/BMS-related in community and service docs.

---

## 5. Pack/harness connector pinout (alternate view — crash signal, CAN, 12V, GND)

An alternate pinout for the pack/harness connector (same or related to X098) labels pins by function. Use this to locate the **crash signal** and other signals when wiring a bridge or isolating the pack.

| Pin | Function | Notes |
|-----|----------|--------|
| 1 | AO: REAR INVERTER | Analog output |
| 3 | AI: REAR INVERTER | Analog input |
| 6 | CANL: CHARGE PORT | Charge port CAN low |
| 7 | CANH: CHARGE PORT | Charge port CAN high |
| 8 | VIN: BAT 12V | Battery 12 V input |
| 9 | GND: BAT | Battery ground |
| 10 | DI: CHARGE PORT FAULT | Digital input |
| 11 | DO: CHARGE PORT LATCH EN | Digital output |
| 12 | DI: PCS LOCKOUT | Digital input |
| **13** | **DI: CRASH SIGNAL** | **Digital input — crash signal from RCM; break and terminate pack side to “no crash” when using a bridge.** |
| 15 | CANH: VEHICLE CAN | Vehicle CAN high |
| 16 | CANL: VEHICLE CAN | Vehicle CAN low |
| 18 | VIN: CONTACTOR POWER 12V | Contactor power 12 V |

- **Crash signal:** **Pin 13** is **DI: CRASH SIGNAL**. It is a **digital input** to the pack (HVP); the pinout does not state whether it is active-high (e.g. 12 V = crash) or active-low (e.g. GND = crash). Do not simply cut and leave the pack side floating — break the wire so the car (RCM) is disconnected, and terminate the pack side to the “no crash” level (GND or 12 V per schematic). See [tesla-battery-emulator-can-and-contactors.md §7.5](tesla-battery-emulator-can-and-contactors.md).
- **Vehicle CAN:** Pins 15 (CANH) and 16 (CANL) — this is the powertrain CAN (same as X098 cavities 6 and 7 in the other view; pin numbering may differ by connector face).
- **12V and GND:** Pin 8 = BAT 12V, Pin 18 = CONTACTOR POWER 12V, Pin 9 = GND.

**Source:** User-provided connector pinout image (pack/harness connector).

---

## 6. Required connections for BE / standalone pack

For Battery Emulator (BE) or standalone pack use, the connector above needs the following connections. (Apart from these, you may need to ground certain holes if the penthouse lid is open — follow pack safety procedures.)

| Connection | Wiring |
|------------|--------|
| **Pin 1 and Pin 3** | **60 Ω resistor** between these pins on **RWD** packs (68 Ω also works). **120 Ω** between Pin 1 and Pin 3 on **AWD** packs. See pack-specific note below. |
| **Pin 8 and Pin 18** | To **+12 V** (BAT 12V and CONTACTOR POWER 12V). |
| **Pin 9** | To **GND** (battery ground). |
| **Pin 15 and Pin 16** | To LilyGo **CAN-H** and **CAN-L** (Vehicle CAN). Pin 15 = CANH, Pin 16 = CANL. You may need to add a **120 Ω** termination resistor here, depending on CAN network structure (e.g. if BE is the only other node, or if the bus already has termination). |

**Pack-specific (Pin 1–3 resistor)**

- **Model 3 2020 (RWD):** Use **60 Ω** between Pin 1 and Pin 3 (68 Ω also works).
- **Model Y 2023 (AWD):** Use **120 Ω** between Pin 1 and Pin 3.

**Summary**

- **Pins 1–3:** 60 Ω (RWD, e.g. Model 3 2020) or 120 Ω (AWD, e.g. Model Y 2023) between Pin 1 and Pin 3.
- **Pins 8, 18:** +12 V.
- **Pin 9:** GND.
- **Pins 15–16:** CAN to LilyGo (CANH, CANL); add 120 Ω if required by bus topology.

### 6.1 Split wiring (pack in car: BE on Vehicle CAN, charge port left on car)

When the pack is still in the car and you **only** unplug X098 from the car and connect BE, you can **leave the charge port pins connected to the car** and take only Vehicle CAN, 12 V, GND, and the Pin 1–3 resistor to BE. That way the charge port ECU keeps talking to the pack on Charge Port CAN and latch/fault work normally; BE is the only node on Vehicle CAN and can send AC charge commands (0x221, 0x333, etc.) with no gateway conflict.

| Pins | Leave connected to car (charge port ECU / harness) | Role |
|------|----------------------------------------------------|------|
| **6** | **Yes** | CANL: CHARGE PORT — charge port CAN low |
| **7** | **Yes** | CANH: CHARGE PORT — charge port CAN high |
| **10** | **Yes** | DI: CHARGE PORT FAULT — digital input (fault from charge port) |
| **11** | **Yes** | DO: CHARGE PORT LATCH EN — digital output (pack enables latch) |

| Pins | Connect to BE (unplug from car) | Role |
|------|--------------------------------|------|
| **1 & 3** | **Yes** | 60 Ω (RWD) or 120 Ω (AWD) between Pin 1 and Pin 3 |
| **8 & 18** | **Yes** | +12 V |
| **9** | **Yes** | GND |
| **15 & 16** | **Yes** | Vehicle CAN → LilyGo CAN-H, CAN-L |

**Summary**

- **Stay on car:** Pins **6, 7, 10, 11** (Charge Port CAN, charge port fault, charge port latch enable). Pack keeps charge port handshake and latch/fault.
- **Move to BE:** Pins **8, 9, 15, 16** (12 V, GND, Vehicle CAN) and **resistor between 1 and 3**. Gateway stays disconnected; BE sends vehicle-side and AC charge commands on Vehicle CAN.

---

## 7. References

- **Battery-Emulator wiki — Tesla Model S/3/X/Y (primary wiring & safety):**  
  [github.com/dalathegreat/Battery-Emulator/wiki/Battery:-Tesla-Model-S-3-X-Y](https://github.com/dalathegreat/Battery-Emulator/wiki/Battery:-Tesla-Model-S-3-X-Y) — Low voltage (X098) wiring, PCS 12V, HVIL, pyro fuse, pre-charge capacitor, HV wiring, HVIL troubleshooting, penthouse components, balancing.
- **Tesla Model 3 Electrical Reference – X098:**  
  [service.tesla.com/docs/Model3/ElectricalReference/prog-56/connector/x098/](https://service.tesla.com/docs/Model3/ElectricalReference/prog-56/connector/x098/)
- **Model Y Electrical Reference (year/SOP):**  
  [service.tesla.com/docs/ModelY/ElectricalReference/](https://service.tesla.com/docs/ModelY/ElectricalReference/)
- **BE CAN and contactors (message list, usage):**  
  [tesla-battery-emulator-can-and-contactors.md](tesla-battery-emulator-can-and-contactors.md)

# Tesla pack-in-car learnings — summary

This document summarizes what we learned about using Battery Emulator (BE) with a Tesla Model 3/Y pack **still in the car** (or with the car’s wiring partly connected): X098 wiring, bridge design, HVIL, crash signal, AC charging with BE, and split wiring so BE and the charge port can coexist.

**Primary references:** [Battery: Tesla Model S 3 X Y](https://github.com/dalathegreat/Battery-Emulator/wiki/Battery:-Tesla-Model-S-3-X-Y) (wiki), [tesla-x098-connector-pinout.md](tesla-x098-connector-pinout.md), [tesla-battery-emulator-can-and-contactors.md](tesla-battery-emulator-can-and-contactors.md).

---

## 1. X098 — what we really need to connect

- **Not just CAN.** The pack needs **12 V**, **GND**, **CAN**, and the **Pin 1–3 resistor** (60 Ω RWD / 120 Ω AWD) to power up and allow contactors.
- **HVIL** is **not** on X098; it’s on the **HV connectors** (jumpers on unused HV connectors, pyro fuse). So “satisfy HVIL at X098” is wrong — HVIL is satisfied in the penthouse/HV side.
- **If we only unplug X098** (leave HV and rest of car intact): we still connect to the pack’s X098 — 12 V (Pin 8, 18), GND (Pin 9), CAN (Pin 15, 16) to BE, and 60/120 Ω between Pin 1 and 3.

---

## 2. Pack-specific resistor (Pin 1–3)

- **Model 3 2020 (RWD):** 60 Ω between Pin 1 and Pin 3 (68 Ω also works).
- **Model Y 2023 (AWD):** 120 Ω between Pin 1 and Pin 3.
- Without this resistor the pack can report “Check high voltage connectors and interlock circuit!” even when HVIL is OK.

---

## 3. Crash signal (Pin 13)

- **Pin 13 = DI: CRASH SIGNAL** (digital input to pack from RCM). Crash trigger is **hardwired** from RCM to pack; pack reports **HVP_gpioCrashSignal** on CAN 0x212.
- **Do not simply cut** the wire — leave pack side floating can cause false crash. **Break** car side and **terminate pack side** to “no crash” level (GND or 12 V per schematic).
- For bridge: never forward or send pyro blow command; break crash hardwire so car can’t fire pack’s pyro.

---

## 4. Bridge / split-bus design (pack in car, car on same bus)

- **Two CAN channels:** Pack on one channel, gateway/car on the other. Middle device **forwards** (blacklist or whitelist) so car sees “battery OK” and pack sees only BE (or forwarded traffic). No two nodes on same wire.
- **Break HVIL** on car side; drive pack-side HVIL **only from BE** so only BE controls contactor allow.
- **Block pyro:** Never forward or send pyro blow command; break crash hardwire if needed.

---

## 5. AC charging with BE (gateway disconnected)

- If **gateway (and charge port ECU) stay disconnected** from the pack’s CAN, **BE is the only “vehicle”** and **BE can send AC charge commands** (0x221, 0x333, etc.). Pack then allows charging when AC is present at charge port (OBC path intact).
- **Charge port pins on X098:** Pin 6, 7 = Charge Port CAN; Pin 10 = DI: CHARGE PORT FAULT; Pin 11 = DO: CHARGE PORT LATCH EN.

---

## 6. Split wiring (pack in car: BE + charge port)

- **Leave connected to car (charge port):** Pin **6, 7, 10, 11** (Charge Port CAN, charge port fault, charge port latch enable). Latch and fault work normally.
- **Connect to BE (unplug from car):** Pin **8, 9, 15, 16** (12 V, GND, Vehicle CAN) and **resistor between Pin 1 and 3**.
- Gateway stays disconnected; BE sends vehicle-side and AC charge commands on Vehicle CAN; charge port ECU keeps talking to pack on Charge Port CAN.

---

## 7. Connector pinout (alternate view)

- **Vehicle CAN:** Pin 15 (CANH), Pin 16 (CANL).
- **Charge Port CAN:** Pin 7 (CANH), Pin 6 (CANL).
- **12 V / GND:** Pin 8 (BAT 12V), Pin 18 (CONTACTOR POWER 12V), Pin 9 (GND).
- **Crash:** Pin 13 (DI: CRASH SIGNAL).
- **Rear inverter resistor:** Pin 1 & 3 (60 Ω RWD, 120 Ω AWD).

---

## 8. Read-only CAN mode

- BE has **Settings → Hardware config → “Read-only CAN (monitor only, no TX)”**. When enabled, BE only receives (e.g. 0x132, 0x20A, 0x212); no TX. Use to monitor BMS/pack while car runs without bus conflict.

---

## 9. Doc index

| Doc | Contents |
|-----|----------|
| [tesla-x098-connector-pinout.md](tesla-x098-connector-pinout.md) | X098 cavity/pin table, CAN, required connections, pack-specific resistor, **split wiring** (§6.1). |
| [tesla-battery-emulator-can-and-contactors.md](tesla-battery-emulator-can-and-contactors.md) | CAN TX/RX, bridge design, HVIL, crash signal, pyro safety. |
| [tesla-3y-dc-charge-contactor-can-research.md](tesla-3y-dc-charge-contactor-can-research.md) | 0x232 DC fast charge contactor. |
| [README.md](README.md) | Docs index. |

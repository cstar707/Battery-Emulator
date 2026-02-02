# Documentation

## Tesla Battery Emulator — CAN & Contactors

Reference for how Battery Emulator (BE) talks to Tesla packs over CAN, contactor engagement, and DC fast charge (FC) contactor control.

| Document | Contents |
|----------|----------|
| **[tesla-battery-emulator-can-and-contactors.md](tesla-battery-emulator-can-and-contactors.md)** | **Master reference:** BE ↔ Tesla CAN overview, X098 connector summary, BMS over CAN (information and control), full TX/RX message list (Model 3/Y and Legacy), main contactor engagement (0x221 + 0x118, GPIO), DC fast charge contactor summary, BMS and trunk controller, references. |
| **[tesla-x098-connector-pinout.md](tesla-x098-connector-pinout.md)** | **X098 connector:** Full penthouse pinout for Model 3/Y — cavity table, wire colors, sizes, destinations; CAN at cavities 6 (CAN_H, black) and 7 (CAN_L, green); BE wiring; Tesla Electrical Reference links. |
| **[tesla-3y-dc-charge-contactor-can-research.md](tesla-3y-dc-charge-contactor-can-research.md)** | **DC charge path:** 0x232 BMS_contactorRequest research, signal layout (BMS_fcContactorRequest, etc.), implementation outline for FC contactor on/off in BE, related frames, internet sources (Tessie, DBC, Google Sheets, service bulletin). |
| **[tesla-pack-in-car-learnings.md](tesla-pack-in-car-learnings.md)** | **Pack-in-car summary:** X098 wiring (not just CAN), HVIL vs X098, Pin 1–3 resistor (Model 3 2020 RWD / Model Y 2023 AWD), crash signal (Pin 13), bridge design, AC charge with BE, split wiring (charge port on car, BE on Vehicle CAN), read-only mode. |

All files are in this `docs/` folder. For **Tesla Model 3/Y wiring, safety, HVIL, and pyro**, the canonical guide is the official wiki: **[Battery: Tesla Model S 3 X Y](https://github.com/dalathegreat/Battery-Emulator/wiki/Battery:-Tesla-Model-S-3-X-Y)**. Start with **tesla-battery-emulator-can-and-contactors.md** for CAN/messages; use **tesla-x098-connector-pinout.md** for X098 cavity/pin detail and pack-specific (Model 3 2020 / Model Y 2023) resistor notes; use the DC charge doc for 0x232 and FC contactor details.

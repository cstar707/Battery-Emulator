# Documentation

## Tesla Battery Emulator — CAN & Contactors

Reference for how Battery Emulator (BE) talks to Tesla packs over CAN, contactor engagement, and DC fast charge (FC) contactor control.

| Document | Contents |
|----------|----------|
| **[tesla-battery-emulator-can-and-contactors.md](tesla-battery-emulator-can-and-contactors.md)** | **Master reference:** BE ↔ Tesla CAN overview, X098 connector, BMS over CAN (information and control), full TX/RX message list (Model 3/Y and Legacy), main contactor engagement (0x221 + 0x118, GPIO), DC fast charge contactor summary, BMS and trunk controller, references. |
| **[tesla-3y-dc-charge-contactor-can-research.md](tesla-3y-dc-charge-contactor-can-research.md)** | **DC charge path:** 0x232 BMS_contactorRequest research, signal layout (BMS_fcContactorRequest, etc.), implementation outline for FC contactor on/off in BE, related frames, internet sources (Tessie, DBC, Google Sheets, service bulletin). |

Both files are saved in this `docs/` folder. Start with **tesla-battery-emulator-can-and-contactors.md** for the full picture; use the second for 0x232 and DC fast charge details.

# LilyGo RS485/CAN ↔ MCP2515 module – connection table

Use this table to wire the **LilyGo T-CAN485** pin header to your **MCP2515 CAN module** (8 MHz crystal, SPI header J4). The firmware expects this pinout (`hw_lilygo.h`).

## Connection table

| # | **Wire color** | **LilyGo header** | **LilyGo pin** | **MCP2515 module (J4)** | **Notes** |
|---|----------------|-------------------|----------------|-------------------------|-----------|
| 1 | **Black**      | GND               | GND            | GND                     | Ground (use same GND for both boards). |
| 2 | **Red**        | VDD               | 3.3 V          | VCC                     | **Use 3.3 V only.** Do not connect to 5 V. |
| 3 | **Yellow**     | IO12              | GPIO 12        | SCK                     | SPI clock. |
| 4 | **Orange**     | IO5               | GPIO 5         | SI                      | SPI MOSI (data to MCP2515). |
| 5 | **Green**      | IO34              | GPIO 34        | SO                      | SPI MISO (data from MCP2515). |
| 6 | **Blue**       | IO18              | GPIO 18        | CS                      | Chip select. |
| 7 | **Purple**     | IO35              | GPIO 35        | INT                     | Interrupt from MCP2515. |

**Total: 7 wires** (2 power, 5 signals).

## LilyGo header (your board – top to bottom)

| Row | Left pin | Right pin |
|-----|----------|-----------|
| 1   | GND      | IO25     |
| 2   | IO32     | IO33     |
| 3   | **IO5**  | **IO12** |
| 4   | **IO34** | **IO35** |
| 5   | **IO18** | VDD      |
| 6   | GND      | VDD      |

Use **GND** and **VDD** from any of the rows above (e.g. row 5 right = VDD, row 1 left or row 6 left = GND).

## MCP2515 module (J4 – left to right)

| Pin | Label | Connect to LilyGo |
|-----|--------|--------------------|
| 1   | INT   | IO35 (Purple)      |
| 2   | SCK   | IO12 (Yellow)      |
| 3   | SI    | IO5 (Orange)      |
| 4   | SO    | IO34 (Green)      |
| 5   | CS    | IO18 (Blue)       |
| 6   | GND   | GND (Black)       |
| 7   | VCC   | VDD 3.3 V (Red)   |
| 8   | (—)   | Not used           |

## Checklist

- [ ] **3.3 V only** on MCP2515 VCC (LilyGo VDD). Do not use 5 V.
- [ ] **CANFREQ = 8** in the web UI (Settings → CAN addon crystal), matching your 8.000 MHz crystal.
- [ ] In Settings, set the battery or inverter interface to **“Add-on CAN via GPIO MCP2515”**.
- [ ] BMS power: if you use the add-on, set BMS power to **GPIO 25** (not 18) so it doesn’t conflict with CS.
- [ ] CAN bus: connect CAN_H and CAN_L from the screw terminal to your inverter/battery CAN bus; add 120 Ω termination at one end of the bus if needed (your module has a jumper for this).

## Unused wires (for reference)

You have more colors than needed for this link. Spares: **White**, **Brown**, and extra **Orange/Blue** can be used for other things (e.g. CAN_H/CAN_L to the rest of the system, or future expansion).

---

## Saved working configuration (LilyGo T-CAN485 + MCP2515 add-on)

Use these settings in the web UI **Settings** to match the working setup (battery on native CAN, inverter on MCP2515).

| Setting | Value |
|--------|--------|
| **Battery** | Tesla Model 3/Y (or your pack) |
| **Battery interface** | CAN (Native) |
| **Inverter protocol** | BYD Battery-Box Premium HVS over C (or your inverter) |
| **Inverter interface** | **CAN (MCP2515 add-on)** |
| **CAN addon crystal (Mhz)** | **8** |
| **BMS Power pin** | **Pin 25** |
| **SMA enable pin** | **Pin 33** |
| **Equipment stop button** | Not connected (or as needed) |

- **Power:** LilyGo from USB; MCP2515 from LilyGo **3.3 V (VDD)** only.
- **Module:** DAOKI-style MCP2515 (8.000 MHz crystal, 6-pin SPI + INT). Wire by label: VCC, GND, CS, SI, SO, SCK, INT.
- After changing settings, **Save** and **reboot** (or use Restart CAN). Serial should show **"Can ok"** and the CAN logger can show **TX5** / **RX4** for MCP2515 traffic.

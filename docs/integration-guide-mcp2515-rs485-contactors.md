# Integration Guide: MCP2515 CAN Add-on, RS485/Modbus Inverter, and Contactor/Precharge GPIO

This guide covers wiring and configuration for:

1. **MCP2515 add-on board** – second CAN bus (e.g. for Solis inverter / Pylon protocol).
2. **RS485 for Modbus** – connecting the inverter over RS485 (e.g. BYD Modbus, Kostal).
3. **GPIO contactor control** – positive/negative contactors, precharge, and optional BMS power.

Board-specific pin tables are in [waveshare7b-pins.md](waveshare7b-pins.md) (ESP32-S3 7B) and [waveshare-p4-7b-pins.md](waveshare-p4-7b-pins.md) (ESP32-P4 7B).

---

## 1. MCP2515 add-on board (second CAN bus)

The Battery Emulator can use **native CAN** (e.g. Tesla battery) and a **second CAN bus** via an MCP2515 module for inverters that speak Pylon/Solis-style CAN (e.g. Solis, Sofar, Growatt).

### 1.1 What you need

- **MCP2515 CAN module** (with 8 MHz or 16 MHz crystal – see below).
- **SPI connection:** SCK, MOSI, MISO, CS. Optionally **INT** (interrupt) and **RST** (reset) if your HAL defines them.
- **CAN bus:** CANH, CANL, 120 Ω termination if required by your inverter.

### 1.2 Crystal frequency (CANFREQ)

The MCP2515 crystal must match the setting in the firmware:

| Crystal on module | Settings → CANFREQ |
|-------------------|---------------------|
| **8 MHz**         | 8 (default on 7B)  |
| **16 MHz**        | 16                  |

Set **CANFREQ** in the web Settings (or NVM) to the crystal frequency in MHz. Wrong value causes wrong CAN bit rate and communication failure.

### 1.3 Pin wiring by board

#### Waveshare ESP32-S3-Touch-LCD-7B (7B)

MCP2515 **shares the SPI bus with the SD card**. Pins are fixed in the HAL:

| Signal | GPIO | Notes |
|--------|------|--------|
| SCK    | 12   | Shared with SD card |
| MOSI   | 11   | Shared with SD card |
| MISO   | 13   | Shared with SD card |
| **CS** | **6**| MCP2515 chip select (SD card uses IO expander EXIO4) |
| **INT**| *not connected* | Optional: wire to a free GPIO and add it to `hw_waveshare7b.h` → `MCP2515_INT()` |

- **CS** is GPIO 6 (dedicated to MCP2515).
- **INT** is not wired by default; you can use a spare GPIO and update the HAL if you want interrupt-driven RX.
- Do not use GPIO 43 or 44 for INT if you use them for contactors; the HAL has no other free GPIOs. Alternatively use the CH422G expander (EXIO0/EXIO7) for other functions and keep INT on a GPIO you add to the HAL.

**Wiring summary (7B):** Connect MCP2515 SCK→GPIO12, MOSI→GPIO11, MISO→GPIO13, CS→GPIO6, VCC→3.3V, GND→GND. CANH/CANL to inverter with 120 Ω if needed.

#### Waveshare ESP32-P4-WIFI6-Touch-LCD-7B (P4 7B)

The P4 7B HAL does **not** define MCP2515 pins (all return NC). The board has no dedicated MCP2515 header.

To add MCP2515 on P4 7B you must:

1. **Choose an SPI bus.** The board uses SDMMC for the TF card (not SPI), so you can use a free SPI-capable GPIO set. Check the [P4 7B wiki](https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-7B) for which GPIOs are free and support SPI.
2. **Wire** MCP2515 SCK, MOSI, MISO, CS (and optionally INT) to those GPIOs.
3. **Edit** `Battery-Emulator/Software/src/devboard/hal/hw_waveshare_p4_7b.h`: set `MCP2515_SCK()`, `MCP2515_MOSI()`, `MCP2515_MISO()`, `MCP2515_CS()`, and optionally `MCP2515_INT()` to the GPIOs you used.
4. **Rebuild** with env `waveshare_p4_7b`.

Until then, only **native CAN** (GPIO21/22) is available on P4 7B for CAN communication.

### 1.4 Software configuration

- In the web **Settings**, set **Inverter** to a protocol that uses the add-on CAN (e.g. Solis, Sofar, Growatt Pylon-style).
- Set **Communication interface** for the inverter to **CAN (MCP2515 - Solis Inverter)** (or equivalent label for your board).
- Set **CANFREQ** to 8 or 16 to match the MCP2515 crystal.

---

## 2. RS485 for Modbus connection to the inverter

The board’s RS485 port (UART) is used for **Modbus RTU** or **proprietary RS485** protocols to the inverter (e.g. BYD 11kWh HVM over Modbus, BYD battery via Kostal RS485).

### 2.1 Hardware wiring

| Board    | RS485 TX | RS485 RX | DE/RE pin |
|----------|----------|----------|-----------|
| **7B**   | GPIO 15  | GPIO 16  | Not defined (NC) |
| **P4 7B**| GPIO 26  | GPIO 27  | Not defined (NC) |

- **A/B (or D+/D−):** Connect to the inverter’s RS485 A/B (or equivalent) and GND. Match polarity to the inverter manual (e.g. A→A, B→B).
- **Termination:** Add 120 Ω between A and B at the end of the line if the inverter manual requires it.
- **DE/RE (direction):** The Waveshare 7B and P4 7B HALs do not define `RS485_EN_PIN` or `RS485_SE_PIN` (both NC). The firmware can still work with half-duplex RS485 if the transceiver is auto-direction or if the inverter tolerates it. For full half-duplex control you would need a free GPIO wired to DE/RE and the HAL updated to define that pin.

### 2.2 Supported inverter protocols (RS485/Modbus)

Examples of protocols that use RS485 or Modbus:

- **BYD 11kWh HVM battery over Modbus RTU** – Modbus server ID 21.
- **BYD battery via Kostal RS485** – Kostal proprietary protocol (57600 baud).

Select the inverter type in the web **Settings** (e.g. “BYD 11kWh HVM battery over Modbus RTU” or “BYD battery via Kostal RS485”). The firmware uses the same RS485 TX/RX pins; baud and protocol are fixed per inverter type.

### 2.3 Software configuration

- In **Settings**, set **Inverter** to the correct RS485/Modbus type (e.g. BYD Modbus or Kostal RS485).
- Set **Communication interface** for the inverter to **RS485** (or **Modbus** where shown).
- No DE/RE setting is exposed for these boards unless you add a GPIO and HAL support.

---

## 3. GPIO connections for contactor control and precharge

The firmware can drive **positive contactor**, **negative contactor**, and **precharge** (and optionally **BMS power**) via GPIOs. Use a **relay board or optocoupler/MOSFET** between GPIO and contactor coil; do not drive coils directly from the ESP32.

### 3.1 HAL pins (what the firmware uses)

| Function            | HAL method                   | 7B (ESP32-S3) | P4 7B (ESP32-P4) |
|---------------------|-----------------------------|----------------|-------------------|
| Positive contactor  | `POSITIVE_CONTACTOR_PIN()`  | GPIO 43        | GPIO 14           |
| Negative contactor  | `NEGATIVE_CONTACTOR_PIN()` | GPIO 44        | GPIO 15           |
| Precharge           | `PRECHARGE_PIN()`           | NC*            | NC*               |
| BMS power           | `BMS_POWER()`               | NC*            | NC*               |

\*NC = not connected in the default HAL. Precharge/BMS power can be added via an IO expander (7B: EXIO0/EXIO7) or by assigning free GPIOs and updating the HAL (see board pin docs).

### 3.2 Contactor sequence (summary)

When **Contactor control** is enabled in Settings:

1. **DISCONNECTED** – All contactor and precharge outputs off.
2. **START_PRECHARGE** – If inverter allows and no fault: proceed to precharge.
3. **PRECHARGE** – Precharge output ON for the configured **Precharge time** (e.g. 100–500 ms) to limit inrush.
4. **POSITIVE** – Negative contactor ON, then positive contactor ON.
5. **PRECHARGE_OFF** – Precharge output OFF after a short delay.
6. **COMPLETED** – Contactors held; optional **PWM economizer** (hold coil with reduced duty) if enabled.

If **PWM contactor control** is enabled, the firmware uses PWM on the positive and negative contactor pins to economize after closing. Precharge is always digital (no PWM).

### 3.3 Settings (web UI)

- **Contactor control** – Enable GPIO contactor control.
- **Precharge time (ms)** – Duration the precharge output is on (e.g. 100–500 ms depending on inverter capacitance).
- **Contactor control inverted logic** – Use if your hardware uses normally closed (NC) contactors.
- **PWM contactor control** – Use PWM to hold contactors (economizer).
- **BMS power / periodic BMS reset** – Only used if `BMS_POWER()` is not NC; some batteries need a power cycle.

### 3.4 Wiring (generic)

- **GPIO → relay/optocoupler:** GPIO high = contactor close (or open if inverted). Use 3.3V-safe input (relay board or optocoupler). Do not connect GPIO directly to contactor coils.
- **Precharge:** Resistor in series with precharge path; precharge relay driven by `PRECHARGE_PIN()` when defined.
- **Power:** Use 3.3V and GND from the board (e.g. from the same PH2.0 or header that exposes the contactor GPIOs). See board docs for exact pinout.

### 3.5 7B: extra outputs via CH422G (EXIO0, EXIO7)

On the **ESP32-S3 7B**, the CH422G IO expander has **EXIO0** and **EXIO7** free. The display code can **mirror** contactor/precharge state to these pins:

| Pin   | Meaning (HIGH when) |
|-------|----------------------|
| EXIO0 | Main contactors engaged |
| EXIO7 | Precharge in progress, precharge completed, or contactors engaged |

You can wire EXIO0/EXIO7 to relays or LEDs (e.g. “contactors closed”, “precharge/safe to close”). Do not drive contactor coils directly from the expander; use a relay or driver. See [waveshare7b-pins.md](waveshare7b-pins.md).

The **P4 7B** has no CH422G; only the GPIOs defined in the HAL (e.g. 14, 15) are available unless you add an external I2C GPIO expander and firmware support.

### 3.6 Precharge control (advanced)

The **precharge control** feature (separate from the simple precharge output above) uses:

- `HIA4V1_PIN()` – PWM precharge resistor control.
- `INVERTER_DISCONNECT_CONTACTOR_PIN()` – Inverter disconnect contactor.

These are **NC** on both 7B and P4 7B HALs. They are for inverters that support intermediate voltage sensing and timed precharge. To use them, you need free GPIOs and HAL changes; see `precharge_control.cpp` and `hal.h`.

---

## 4. Quick reference

| Topic           | 7B (ESP32-S3)        | P4 7B (ESP32-P4)      |
|----------------|----------------------|------------------------|
| MCP2515 SPI    | GPIO 11/12/13, CS=6  | Not in HAL; add GPIOs and edit HAL |
| MCP2515 INT    | NC (optional wire)   | NC                     |
| RS485 TX/RX    | 15, 16               | 26, 27                 |
| RS485 DE/RE    | NC                   | NC                     |
| Positive ctrl  | GPIO 43              | GPIO 14                |
| Negative ctrl  | GPIO 44              | GPIO 15                |
| Precharge      | NC (or EXIO0)        | NC                     |
| BMS power      | NC (or EXIO7)        | NC                     |
| Extra outputs  | EXIO0, EXIO7 (mirror)| None                   |

For exact pin locations and schematic references, see:

- [waveshare7b-pins.md](waveshare7b-pins.md) – ESP32-S3-Touch-LCD-7B  
- [waveshare-p4-7b-pins.md](waveshare-p4-7b-pins.md) – ESP32-P4-WIFI6-Touch-LCD-7B  

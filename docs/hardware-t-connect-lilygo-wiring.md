# T-Connect, LILYGO 4-Channel, and MCP2515/MCP2518 Wiring

Hardware overview and wiring for the **T-Connect** (ESP32-S3 + CAN FD + RS485), the **LILYGO 4-channel differential board**, and add-on **MCP2515** (classic CAN) / **MCP2518** (CAN FD) modules for the Tesla Model Y stationary power plant project.

---

## System context: off-grid microgrid

This understanding applies to the **entire power plant system** and should be taken into consideration for all design and integration work:

- **Off-grid microgrid:** There is no backup grid connection. Energy demand must match production.
- **Microgrid Controller** coordinates:
  - **Battery energy storage system (BESS)** — e.g. Tesla pack(s) and the battery emulator.
  - **Solar and other generation** — including the **Solark inverter system** and **two Enphase systems**.
- The controller operates **all storage and generation assets in parallel as needed** so that supply and demand stay balanced.

In this setup, the T-Connect (and Battery-Emulator firmware) form part of the control and monitoring layer: they talk to the battery (CAN), to inverters and meters (CAN/RS485), and can report/accept setpoints via MQTT or the web UI for coordination with a higher-level microgrid controller or energy management logic. Wiring, CAN channels, RS485, and MQTT logging choices should support coordination among BESS, Solark, and both Enphase systems.

**Full system context and asset list:** [off-grid-microgrid-context.md](off-grid-microgrid-context.md)

---

## 1. Board overview

### 1.1 T-Connect (custom board, V1.1 / V3)

| Item | Description |
|------|-------------|
| **MCU** | ESP32-S3-WROOM-1 (Wi‑Fi, Bluetooth, dual-core) |
| **CAN** | 1× MORNSUN TD501MCANFD (isolated CAN FD on-board) |
| **Serial** | 3–4× K-CUT TD501D485H-A (isolated RS485) |
| **Connectivity** | USB-C (power + programming/serial), 2×10 GPIO header |
| **RTC** | 32.768 kHz crystal |

Terminal blocks (typical silkscreen):

- **CAN FD**: one 3–4 pin block (e.g. SGA / DGA or CAN_H, CAN_L, GND).
- **RS485**: one 3-pin block per channel — e.g. **SGD**, **DGD**, **L** (or A/B, GND) for channels A–D.

GPIO header gives access to ESP32-S3 pins (e.g. IO1, IO2, IO11–14, IO21, IO35–48, +5V, GND, V3, RESET). **The T-Connect has its own pinout**—use your T-Connect schematic or silkscreen for which GPIO is which. The Battery-Emulator firmware currently targets the **LilyGo T_2CAN** board (different hardware); to run on T-Connect you need a T-Connect-specific HAL or to wire add-ons to the pins your T-Connect actually exposes.

### 1.2 LILYGO 4-channel differential board

Passive breakout (no MCU). Exposes four differential channels (A–D) plus USB for power/interface on some variants.

**Top edge (4 groups × 4 pins each):**

| Channel | Signal ground | Low (CAN_L) | High (CAN_H) | Digital ground |
|---------|----------------|-------------|--------------|----------------|
| **A**   | SGNDA / SGA    | L-A         | V-H (or H-A) | DGNDA / DGA    |
| **B**   | SGNDB / SGB    | L-B         | H-B          | DGNDB / DGB    |
| **C**   | SGNDC / SGC    | L-C         | H-C          | DGNDC / DGC    |
| **D**   | SGNDD / SGD    | L-D         | H-D          | DGNDD / DGD    |

**Bottom-left:** USB — D-, D+, VBUS, GND (if used for power or data).

Use one channel per CAN bus: **L** = CAN_L, **H** = CAN_H; tie **SGND** (and optionally **DGND**) as needed for your isolation scheme.

---

## 2. Wiring

### 2.1 T-Connect on-board CAN FD ↔ one LILYGO channel

Use the T-Connect CAN FD terminal block and one channel (e.g. A) on the LILYGO:

| T-Connect CAN FD | LILYGO channel A |
|------------------|------------------|
| CAN_H (or H)     | H-A              |
| CAN_L (or L)     | L-A              |
| GND              | SGNDA (and/or DGNDA) |

Twisted pair for CAN_H / CAN_L; 120 Ω termination at each end of the bus if required by your bus length/speed.

### 2.2 MCP2515 / MCP2518 (SPI) ↔ T-Connect (ESP32-S3)

Add-on modules use **SPI** plus one **CS** (and optionally **INT**) per chip. Shared lines: **SCK**, **MOSI**, **MISO**; each chip gets its own **CS** (and **INT** if used).

**For the T-Connect:** Wire each signal to the **GPIO header pins that your T-Connect schematic assigns** to SPI (SCK, MOSI, MISO), CS, INT, and RST. The tables below are **one example** (LilyGo T_2CAN, which the current firmware uses); your T-Connect may use different GPIO numbers—check your board’s pinout.

**MCP2515 (classic CAN, 8 MHz crystal typical):**

| Function | T-Connect: use your schematic | MCP2515 module |
|----------|-------------------------------|----------------|
| SCK      | e.g. GPIO 12 (T_2CAN example) | SCK            |
| MOSI     | e.g. GPIO 11                  | SI (MOSI)      |
| MISO     | e.g. GPIO 13                  | SO (MISO)      |
| CS       | e.g. GPIO 10                  | CS             |
| INT      | e.g. GPIO 8                   | INT (optional) |
| RST      | e.g. GPIO 9                   | RST (optional) |
| 3.3V     | 3.3V                          | VCC            |
| GND      | GND                           | GND            |

**MCP2517/MCP2518 (CAN FD, 20/40 MHz crystal):**

| Function | T-Connect: use your schematic | MCP2517/2518 module |
|----------|-------------------------------|----------------------|
| SCK      | e.g. GPIO 38 (T_2CAN example) | SCK                  |
| SDI (MOSI)| e.g. GPIO 42                 | SI                   |
| SDO (MISO)| e.g. GPIO 37                 | SO                   |
| CS       | e.g. GPIO 41                  | CS                   |
| INT      | e.g. GPIO 39                  | INT (optional)       |
| 3.3V     | 3.3V                          | VCC                  |
| GND      | GND                           | GND                  |

**MCP2515 ↔ LILYGO (one CAN bus):**  
Connect the MCP2515 transceiver pins (CAN_H, CAN_L) to one LILYGO channel (e.g. B):  
MCP2515 **CAN_H** → LILYGO **H-B**, **CAN_L** → **L-B**, **GND** → **SGNDB** (and optionally DGNDB).

**MCP2518 ↔ LILYGO (one CAN bus):**  
Same idea: MCP2518 **CAN_H** → e.g. **H-C**, **CAN_L** → **L-C**, **GND** → **SGNDC**.

So you can use:

- T-Connect built-in CAN FD → LILYGO channel A  
- MCP2515 → LILYGO channel B  
- MCP2518 → LILYGO channel C  
- (Optional) More add-ons → LILYGO channel D, etc.

---

## 3. How many MCP2515 and MCP2518 can we add?

### 3.1 Current firmware (Battery-Emulator)

- **One** physical **MCP2515** (one CS, one set of SPI pins).
- **One** physical **MCP2517/2518** (one CS, one set of SPI pins).
- Plus **one native CAN** (or CAN FD) on the T-Connect (on-board MORNSUN CAN FD).

So out of the box: **1× MCP2515**, **1× MCP2518** (and 1× native CAN). The LILYGO board’s four channels can carry: onboard CAN FD + MCP2515 + MCP2518 + one more bus if you add another transceiver later.

### 3.2 Hardware limit (ESP32-S3)

- **SPI:** ESP32-S3 has multiple SPI buses (e.g. FSPI, HSPI). Many chips can share the **same** bus (same SCK, MOSI, MISO); each chip needs a **unique CS** (and optionally INT).
- **GPIO:** Dozens of usable GPIOs; each extra MCP2515 or MCP2518 needs **1× CS** and optionally **1× INT** (and RST for 2515 if used).
- **Practical:** Without changing the board layout, 2–4 of each (MCP2515 and MCP2518) is realistic; beyond that you run out of convenient GPIOs or need a GPIO expander.

So in theory you can add **several** of each; the limit is **GPIO for CS (and INT)** and **firmware support**.

### 3.3 Firmware limit (to support more chips)

The current code assumes **one** MCP2515 and **one** MCP2517/2518:

- Single `ACAN2515* can2515` and one `esp32hal->MCP2515_CS()` (and one INT, RST).
- Single `ACAN2517FD* canfd` and one `esp32hal->MCP2517_CS()` (and one INT).

To support **more** MCP2515 or MCP2518:

1. **HAL:** Define extra CS (and INT, RST) pins per chip (e.g. `MCP2515_CS_2()`, `MCP2517_CS_2()`, or an array).
2. **comm_can:** For each extra chip, create another `ACAN2515` / `ACAN2517FD` instance, call `begin()` with the right CS, and in `receive_can()` poll all instances and map frames to the right `CAN_Interface` (e.g. extend the enum with `CAN_ADDON_MCP2515_2`, `CANFD_ADDON_MCP2518_2`, etc.).
3. **Settings/UI:** Allow user to assign which logical interface (battery, inverter, etc.) uses which physical port (e.g. “Battery = MCP2515 #2”).

**Summary table**

| Item | MCP2515 | MCP2518 (2517) |
|------|---------|-----------------|
| **Supported today** | 1 | 1 |
| **Same SPI bus** | Yes (shared SCK/MOSI/MISO, unique CS) | Yes (same idea) |
| **Practical max (hardware)** | 2–4+ (GPIO-limited) | 2–4+ (GPIO-limited) |
| **To add more** | Extra CS/INT in HAL + second instance in `comm_can` | Same approach |

---

## 4. Pin reference: T-Connect vs firmware (T_2CAN)

**T-Connect** has its own GPIO header and schematic. The exact pins for native CAN, MCP2515, MCP2518, and RS485 are **defined by the T-Connect design**, not by this doc. Use your T-Connect schematic or silkscreen for the real pinout.

The Battery-Emulator firmware currently uses a HAL for **LilyGo T_2CAN** (a different board). That HAL uses the pins in the table below. **If you are running that firmware on a T-Connect**, you either need a **T-Connect-specific HAL** that assigns the correct GPIOs from your schematic, or you must wire add-ons to match whatever the T_2CAN HAL expects and ensure your T-Connect header exposes those same GPIOs.

| Role | GPIO in T_2CAN HAL (for reference) | Note |
|------|-----------------------------------|------|
| **Native CAN** | TX=7, RX=6 | On-board transceiver (on T-Connect: MORNSUN CAN FD) |
| **MCP2515** | SCK=12, MOSI=11, MISO=13, CS=10, INT=8, RST=9 | HSPI |
| **MCP2517/2518** | SCK=38, SDI=42, SDO=37, CS=41, INT=39 | FSPI |
| **RS485** | TX=43, RX=44 | T-Connect has 3–4× RS485; pins per channel from your schematic |

For **T-Connect**, get the actual pinout from your board’s documentation or schematic and, if needed, add or adapt a HAL so the firmware drives the correct pins.

---

## 5. Suggested wiring summary

1. **T-Connect CAN FD** → LILYGO **channel A** (CAN_H→H-A, CAN_L→L-A, GND→SGNDA).
2. **MCP2515** (SPI on T-Connect) → transceiver CAN_H/CAN_L → LILYGO **channel B**; 3.3V/GND to T-Connect.
3. **MCP2518** (SPI on T-Connect) → transceiver CAN_H/CAN_L → LILYGO **channel C**; 3.3V/GND to T-Connect.
4. **LILYGO channel D** left for a future bus (e.g. second MCP2515 or MCP2518 once firmware supports extra CS/INT).

Use twisted pairs for each CAN bus, 120 Ω at both ends if needed, and keep RS485 and CAN wiring separate to avoid coupling.

---

## 6. Does the board have enough inputs? ADC and other peripherals

### 6.1 Short answer

**For the current design (battery CAN, inverter CAN, contactors, RS485):** Yes, the T-Connect has enough. **For adding lots of analog sensing or extra CAN/SPI devices:** It depends which GPIOs your T-Connect exposes on the header and how many are already used for CAN FD, RS485, etc. You can add extra inputs via spare ADC pins (if any), I2C sensors, or RS485.

### 6.2 ESP32-S3 resources (summary)

| Resource | Count / range | Notes |
|----------|----------------|--------|
| **GPIO** | 0–21, 26–48 (45 pins; 22–25 don’t exist) | On T-Connect, many are used for on-board CAN FD, RS485, and the header; exact usage from your schematic. |
| **ADC** | 20 channels total | **ADC1:** GPIO1–10. **ADC2:** GPIO11–20. 12-bit, 0–~3.1 V (11 dB atten). **ADC2 not reliable when Wi‑Fi is on.** |
| **DAC** | None on-chip | Use PWM for simple analog out, or an external I2C/SPI DAC. |
| **I2C** | Any GPIO (SW or HW) | Typically 2 pins (SDA, SCL). Used for display (e.g. SSD1306) when enabled. |
| **UART** | Multiple (GPIO matrix) | USB serial + bootloader use some pins; T-Connect’s RS485 channels use board-specific GPIOs. |
| **Touch** | Some GPIOs | Capacitive touch; rarely needed for a power plant. |

### 6.3 What might be free on the T-Connect (GPIO / ADC)

Which pins are free depends **entirely on your T-Connect schematic**: the on-board CAN FD and RS485 blocks already use some ESP32-S3 pins; the header exposes the rest.

- **ADC on ESP32-S3:** ADC1 = GPIO1–10, ADC2 = GPIO11–20. **ADC2 is unreliable when Wi‑Fi is active.** Any of these that your T-Connect brings out and doesn’t use for CAN, SPI, or RS485 can be used as analog inputs (e.g. voltage divider, NTC).
- **GPIO 19, 20** – Often used for USB-JTAG on ESP32-S3; if your T-Connect exposes them and you don’t need JTAG, they can be **ADC2** inputs.
- **Strapping / special pins** – e.g. GPIO0, 3, 45, 46 can affect boot; check the T-Connect design before using them as general I/O.
- **Flash/PSRAM** – GPIO26–37 are often reserved for flash/PSRAM; only use if your schematic shows them on the header.

So in practice: see which ADC-capable GPIOs (1–20) your T-Connect header actually has, then use any that aren’t assigned to CAN FD or RS485. You may have **0–several** ADC pins available depending on the layout.

### 6.4 What you can use ADC for (and how)

If you get one or two ADC pins (or add an external ADC):

| Use | How |
|-----|-----|
| **DC voltage** | Resistor divider to bring voltage into 0–3.3 V (e.g. 12 V or 48 V bus → divider → ADC). Calibrate in software. |
| **Temperature** | NTC thermistor in divider to ADC; or I2C temp sensor (no ADC needed). |
| **Current (low-side)** | Shunt + op-amp to scale output into 0–3.3 V; ADC reads the scaled voltage. (High-side or isolated current needs appropriate conditioning.) |
| **Battery pack voltage (isolated)** | Isolated amplifier or isolated ADC module → 0–3.3 V → ADC, or use existing BMS/CAN data instead. |

The firmware today does **not** read on-chip ADC for voltage/current; those values come from CAN, Modbus, or RS485. To use ADC you’d add a small driver (e.g. ESP-IDF `adc1_get_raw()` / attenuation) and feed the result into the datalayer or your own logic.

### 6.5 What else you can use (other than ADC)

| Peripheral / feature | What to use it for |
|----------------------|--------------------|
| **RS485 (3–4 channels on T-Connect)** | Modbus RTU energy meters, Solis/inverter comms, BMS, other industrial devices. Firmware already supports RS485/Modbus. |
| **I2C (any free GPIO pair on T-Connect)** | OLED (SSD1306), I2C temp/humidity, I2C ADC (e.g. ADS1115 for more/better analog channels), RTC. |
| **Spare digital GPIO** | Status LEDs, alarm outputs, dry-contact inputs (e.g. “generator run”, “grid loss”), PWM for fan or simple DAC-like output. |
| **On-board CAN FD + MCP2515 + MCP2518 (add-on)** | Battery, inverter, charger, shunt, or logging; assign in Settings. |
| **Wi‑Fi / BLE** | Remote monitoring, OTA, phone app; ADC2 is unreliable while Wi‑Fi is active. |
| **USB** | Power, programming, serial console, CAN logging to PC. |
| **32.768 kHz crystal** | RTC for timestamps and scheduling. |

### 6.6 Practical “do I have enough inputs?”

- **Digital:** Enough for contactors, CAN, RS485, LED, and a few optional GPIOs (e.g. ESTOP, display, CHAdeMO). Adding many more CAN chips will consume more CS/INT pins.
- **Analog:** On the **T-Connect**, how many ADC pins are free depends on your schematic (which GPIOs the CAN FD and RS485 blocks use and what the header exposes). For more analog channels, use an **I2C ADC** (e.g. ADS1115: 4 channels, 16-bit) or devices that send data over **RS485/Modbus** or **CAN** (e.g. energy meter, BMS).
- **Summary:** The T-Connect has enough for the standard battery-emulator + inverter + contactors + RS485. For extra analog or many extra digital inputs, use your T-Connect pinout to find spare GPIOs/ADCs, and plan on I2C sensors/ADCs or RS485/Modbus devices as needed.

---

## 7. 3 CAN + 3 RS485 + I2C 4‑channel relay — do we have enough?

### 7.1 Yes: 3 CAN and 3 RS485

The T-Connect hardware supports this:

| Resource | Count | How |
|----------|--------|-----|
| **CAN** | **3 channels** | 1× on-board MORNSUN CAN FD + 1× MCP2515 (add-on) + 1× MCP2518 (add-on). Wire to LILYGO channels A, B, C (or direct to your buses). |
| **RS485** | **3 channels** | The board has 3–4× K-CUT TD501D485H-A; use three terminal blocks (e.g. channels A, B, C) for three independent RS485 buses (Modbus inverters, meters, BMS, etc.). |

So **3 CAN + 3 RS485** is within what the board provides. No extra GPIOs needed for the RS485 channels—they’re already on the T-Connect; you just use three of the screw terminals.

**Reserving room for Solis inverter #2:** This system will add a second Solis inverter later. Plan RS485 so that **one channel is for Solis 1** (Tesla battery 1) and **one channel is reserved for Solis 2** (same emulator, or a separate battery emulator). If both Solis units are on this emulator, use two of the T-Connect’s RS485 blocks (e.g. ch1 = Solis 1, ch2 = Solis 2). If Solis 2 will use a separate battery emulator, the first emulator only needs one RS485 channel for Solis 1. Leave CAN headroom or a spare CAN channel for a second inverter if you plan to drive both from this emulator; otherwise a second emulator has its own CAN.

### 7.2 Adding an I2C 4‑channel relay board

**Wires:** An I2C relay board needs **4 wires** to the T-Connect:

| Wire | Connection |
|------|------------|
| **SDA** | One GPIO (from T-Connect header) |
| **SCL** | One GPIO (from T-Connect header) |
| **3.3 V** (or 5 V) | Power (check relay board spec; many are 3.3 V or 5 V) |
| **GND** | Ground |

So you need **2 GPIOs** (SDA, SCL) plus power and GND. The same two GPIOs form one **I2C bus**; you can hang multiple I2C devices on it (relay board, OLED, temp sensor, etc.) as long as each has a different I2C address.

**GPIO pins:** The ESP32-S3 has 45 GPIOs. On the T-Connect, some are used by:

- On-board CAN FD (TX, RX)
- 3× RS485 (TX/RX per channel → 6 pins if half-duplex)
- MCP2515 (6 pins: SCK, MOSI, MISO, CS, INT, RST)
- MCP2518 (5 pins: SCK, SDI, SDO, CS, INT)
- Contactors, BMS power, LED, etc. (board-dependent)

That’s on the order of **~20–25 pins** for CAN, RS485, and add-on CAN. The rest are available on the header for I2C, spare digital I/O, etc. So **you need 2 free GPIOs for I2C**; the T-Connect’s 2×10 header typically exposes enough spare pins that **2 pins for SDA/SCL are realistic**. Confirm on your T-Connect schematic which header pins are free and assign SDA/SCL there (or use the same pair as an existing I2C port if the board already has one for a display).

**Summary:** For **3 CAN + 3 RS485 + I2C 4‑channel relay** you have enough **wires** (4 for I2C) and very likely enough **GPIO** (2 for I2C); just pick a free SDA/SCL pair from your T-Connect pinout. If the relay board is 5 V, use the T-Connect’s 5 V header pin (if present) and ensure the I2C logic levels are compatible (many 5 V relay boards are 3.3 V I2C-tolerant).

---

## 8. Recommended GPIO assignment and full wiring diagram

### 8.1 Recommended GPIO pins (one place)

Use this set if your T-Connect header exposes these GPIOs and they are not used by on-board CAN FD / RS485. If a pin is already taken on your board, use the **Alternate** or pick another free pin from your schematic.

| Function | GPIO | Alternate | Note |
|----------|------|------------|------|
| **CAN FD (on-board)** | TX=7, RX=6 | — | Usually fixed by T-Connect design. |
| **RS485 ch 1** | TX, RX per schematic | — | Check T-Connect terminal block labels (e.g. SGA/DGA/L). |
| **RS485 ch 2** | TX, RX per schematic | — | Same. |
| **RS485 ch 3** | TX, RX per schematic | — | Same. |
| **MCP2515** | SCK=12, MOSI=11, MISO=13, CS=10, INT=8, RST=9 | — | One SPI bus (HSPI). |
| **MCP2518** | SCK=38, SDI=42, SDO=37, CS=41, INT=39 | — | Other SPI bus (FSPI). |
| **I2C SDA** | **1** | 19 | For relay, OLED, sensors. |
| **I2C SCL** | **2** | 20 | Same bus as SDA. |
| **Power for add-ons** | 3.3 V, GND | 5 V if relay needs it | From T-Connect header. |

**Why GPIO 1 and 2 for I2C:** They are commonly used for I2C on ESP32 boards and often on the expansion header. If your T-Connect uses 1/2 for wake-up or something else, use **GPIO 19 (SDA)** and **GPIO 20 (SCL)** instead (they are ADC2 and often free; avoid if you need USB-JTAG).

---

### 8.2 Block diagram (3 CAN + 3 RS485 + I2C relay)

```
                    ┌─────────────────────────────────────────────────────────────────┐
                    │                        T-CONNECT (ESP32-S3)                       │
                    │  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐   │
                    │  │ CAN FD      │  │ RS485 x3    │  │ GPIO header              │   │
                    │  │ (MORNSUN)   │  │ (K-CUT x3)  │  │ 1=SDA 2=SCL 6=RX 7=TX    │   │
                    │  │             │  │             │  │ 8=INT 9=RST 10=CS        │   │
                    │  │ H, L, GND   │  │ Ch1,Ch2,Ch3 │  │ 11=MOSI 12=SCK 13=MISO   │   │
                    │  └──────┬──────┘  └──────┬──────┘  │ 37=SDO 38=SCK 39=INT     │   │
                    │         │                │        │ 41=CS 42=SDI 3.3V GND     │   │
                    └─────────┼────────────────┼────────┼─────────────┬─────────────┘   │
                              │                │        │             │
         ┌────────────────────┼────────────────┼────────┼─────────────┼────────────────────┐
         │                     │                │        │             │                    │
         ▼                     ▼                ▼        ▼             ▼                    ▼
  ┌──────────────┐    ┌──────────────┐   ┌──────────┐ ┌──────────────┐ ┌──────────────┐   ┌──────────────┐
  │ LILYGO       │    │ LILYGO       │   │ RS485    │ │ MCP2515      │ │ MCP2518      │   │ I2C 4ch      │
  │ Channel A    │    │ Channel B,C  │   │ devices  │ │ module       │ │ module       │   │ relay board  │
  │ (CAN FD)     │    │ (MCP2515/18) │   │ (3 buses)│ │              │ │              │   │              │
  │              │    │              │   │          │ │ SPI + CAN_H/L│ │ SPI + CAN_H/L│   │ SDA SCL      │
  │ H-A L-A SGND │    │ H-B L-B etc. │   │ A,B,GND  │ │ → LILYGO B   │ │ → LILYGO C   │   │ 3.3V GND     │
  └──────────────┘    └──────────────┘   └──────────┘ └──────────────┘ └──────────────┘   └──────────────┘
```

---

### 8.3 Wire-by-wire wiring tables

Suggested wire colors follow common practice. Use the same scheme on both ends so you can trace wires easily.

| Color | Use |
|-------|-----|
| **Red** | +3.3 V / +5 V (VCC) |
| **Black** | GND |
| **Yellow** | CAN_H (and SDA for I2C) |
| **Green** | CAN_L (and SCL for I2C) |
| **Purple** | SPI SCK |
| **Blue** | SPI MOSI / SDI |
| **Gray** | SPI MISO / SDO |
| **Orange** | SPI CS |
| **Brown** | SPI INT (or RS485 ch2 A) |
| **White** | SPI RST (or RS485 B with stripe) |
| **Blue / White-Blue / Black** | RS485 ch1: A, B, GND |
| **Orange / White-Orange / Black** | RS485 ch2: A, B, GND |
| **Brown / White-Brown / Black** | RS485 ch3: A, B, GND |

**A. T-Connect CAN FD → LILYGO channel A**

| From (T-Connect CAN FD block) | To (LILYGO ch A) | Suggested wire color |
|-------------------------------|------------------|------------------------|
| CAN_H (or H) | H-A | **Yellow** (twisted pair with CAN_L) |
| CAN_L (or L) | L-A | **Green** (twisted pair with CAN_H) |
| GND | SGNDA and/or DGNDA | **Black** |

**B. T-Connect → MCP2515 module (SPI + power)**

| From (T-Connect header) | To (MCP2515 module) | Suggested wire color |
|------------------------|---------------------|----------------------|
| GPIO 12 | SCK | **Purple** |
| GPIO 11 | SI (MOSI) | **Blue** |
| GPIO 13 | SO (MISO) | **Gray** |
| GPIO 10 | CS | **Orange** |
| GPIO 8 | INT | **Brown** |
| GPIO 9 | RST | **White** |
| 3.3 V | VCC | **Red** |
| GND | GND | **Black** |

**C. MCP2515 → LILYGO channel B (CAN bus)**

| From (MCP2515 transceiver) | To (LILYGO ch B) | Suggested wire color |
|----------------------------|------------------|------------------------|
| CAN_H | H-B | **Yellow** (twisted pair) |
| CAN_L | L-B | **Green** (twisted pair) |
| GND | SGNDB (optional: DGNDB) | **Black** |

**D. T-Connect → MCP2518 module (SPI + power)**

| From (T-Connect header) | To (MCP2518 module) | Suggested wire color |
|------------------------|---------------------|----------------------|
| GPIO 38 | SCK | **Purple** |
| GPIO 42 | SI (SDI) | **Blue** |
| GPIO 37 | SO (SDO) | **Gray** |
| GPIO 41 | CS | **Orange** |
| GPIO 39 | INT | **Brown** |
| 3.3 V | VCC | **Red** |
| GND | GND | **Black** |

**E. MCP2518 → LILYGO channel C (CAN bus)**

| From (MCP2518 transceiver) | To (LILYGO ch C) | Suggested wire color |
|----------------------------|------------------|------------------------|
| CAN_H | H-C | **Yellow** (twisted pair) |
| CAN_L | L-C | **Green** (twisted pair) |
| GND | SGNDC (optional: DGNDC) | **Black** |

**F. T-Connect → I2C 4-channel relay board**

| From (T-Connect header) | To (I2C relay board) | Suggested wire color |
|------------------------|----------------------|----------------------|
| **GPIO 1** | SDA | **Yellow** |
| **GPIO 2** | SCL | **Green** |
| 3.3 V (or 5 V if relay is 5 V) | VCC | **Red** |
| GND | GND | **Black** |

Use 4 wires (SDA, SCL, VCC, GND). If GPIO 1/2 are not free on your board, use **GPIO 19 → SDA** and **GPIO 20 → SCL** instead.

**G. RS485 (3 channels)**

Use the T-Connect’s three RS485 screw terminal blocks. Each block is one channel (e.g. A, B, C). Wire each to your RS485 device (inverter, meter, BMS). Use twisted pair for A–B; keep colors consistent per channel so you can tell channels apart.

| T-Connect RS485 block | Device | Suggested wire color |
|------------------------|--------|----------------------|
| Ch 1: A, B, GND | Device 1 (e.g. inverter) | **Blue** (A), **White/Blue** (B), **Black** (GND) |
| Ch 2: A, B, GND | Device 2 (e.g. meter) | **Orange** (A), **White/Orange** (B), **Black** (GND) |
| Ch 3: A, B, GND | Device 3 (e.g. BMS) | **Brown** (A), **White/Brown** (B), **Black** (GND) |

Match the terminal labels (SGA/DGA/L or A/B/GND) to your T-Connect silkscreen.

---

### 8.4 Summary: what connects where

| Item | Connection |
|------|------------|
| **3 CAN** | T-Connect CAN FD → LILYGO A. MCP2515 (SPI on 10,11,12,13,8,9) → LILYGO B. MCP2518 (SPI on 37,38,39,41,42) → LILYGO C. |
| **3 RS485** | Use T-Connect’s three RS485 terminal blocks; wire each to your Modbus/RS485 device. |
| **I2C relay** | T-Connect GPIO **1** = SDA, **2** = SCL, 3.3 V, GND → relay board (or GPIO **19**/ **20** if 1/2 are in use). |
| **Power** | All add-on modules (MCP2515, MCP2518, relay) get 3.3 V and GND from the T-Connect header; use 5 V only if the relay board requires it. |

---

## 9. Can the ESP32-S3 handle all this load?

### 9.1 Short answer: **Yes**, for typical use

The ESP32-S3 (240 MHz, dual-core) and the Battery-Emulator firmware are built to run **multiple CAN buses** and **RS485** at once. For normal battery-emulator + inverter + 3 CAN + 1 RS485 + I2C relay traffic, the board has enough headroom. Very heavy CAN logging or many devices on all buses at once can add load; the firmware exposes timing so you can monitor it.

### 9.2 How the firmware shares the work

| Load | How it runs | CPU impact |
|------|-------------|------------|
| **3 CAN (native + MCP2515 + MCP2518)** | One **core_loop** task runs every **1 ms** and calls `receive_can()`, which **polls** the native CAN and the two add-on chips. The MCP2515/MCP2518 have **hardware buffers and interrupts**; the MCU just drains those buffers. | Low–medium. SPI for the two add-ons is fast; typical battery/inverter CAN rates (tens to low hundreds of messages per second total) are fine. |
| **RS485 / Modbus** | One **Serial** port (e.g. Serial2) is used for RS485. `receive_rs485()` is called from the same core_loop. Modbus RTU often runs in a **separate FreeRTOS task** (different core), so it doesn’t block the main loop. | Low. Modbus is low bandwidth (e.g. 9600–115200 baud, request/response). |
| **I2C relay** | Only when you change relay state (rare). | Negligible. |
| **Wi‑Fi, web server, MQTT** | Run on the **other core** (e.g. WIFICORE) in separate tasks. | Isolated from the real-time CAN/contactors loop. |
| **Contactors, safety, 10 ms / 1 s logic** | In the same core_loop with CAN/RS485; timed with `millis()`. | Bounded; firmware tracks overrun. |

So the “real-time” side (CAN, RS485, contactors) runs on one core at 1 ms; the other core does Wi‑Fi and web. That keeps CAN and safety responsive.

### 9.3 What could push the limits

- **Very high CAN traffic** on all three buses at once (e.g. hundreds of frames per second on each). The MCP2515/2518 buffers can fill; the loop must run often enough to drain them. If you log every frame to SD or USB, that adds work.
- **SD card CAN logging** – Writes and buffer handling add latency. Use only when needed.
- **Only one RS485 in firmware today** – The T-Connect has **3 RS485 channels** in hardware, but the current firmware uses **one** UART (e.g. Serial2) for RS485/Modbus. So you get **one** RS485 bus in software unless the code is extended to use a second/third UART for the other two blocks. Load-wise, even three Modbus ports would be fine; the limit is software, not CPU.
- **Task watchdog** – The core_loop must finish within the watchdog timeout (seconds). If the loop is blocked too long (e.g. slow SPI, too much work per iteration), the ESP can reset. The web UI shows **“Core task max load”** (in µs) so you can see if you’re getting close.

### 9.4 Practical recommendations

1. **Use the web UI** – Enable “performance measurement” if available and watch **core task max load** and any overrun/event flags. If max load stays well below the watchdog timeout (e.g. &lt; 100–500 ms), you’re fine.
2. **Avoid unnecessary CAN logging** – Turn off USB/SD CAN logging when you’re not debugging.
3. **3 CAN + 1 RS485 + I2C relay** – This is within what the firmware and ESP32-S3 are designed for. Add a **T-Connect HAL** so the right pins are used and you don’t conflict with on-board peripherals.
4. **If you need 3 RS485 buses** – Plan on firmware changes to drive a second (and third) UART with the correct TX/RX/DE pins for the other two T-Connect RS485 blocks; the MCU can handle the load.

**Summary:** The ESP32-S3 can handle 3 CAN channels, RS485, and an I2C relay for this application. The main constraint is firmware support (e.g. one RS485 port today), not raw CPU power. Keep an eye on core loop timing if you turn on heavy logging or add more protocols later.

---

## 10. MQTT logging: Modbus and cell data

The firmware already supports **MQTT publishing** of battery/emulator state and **optional full cell data**. Modbus (RS485) is used to talk to inverters or BMS; the *results* of that communication end up in the datalayer and are published via MQTT. Raw Modbus register dumps are not published today.

### 10.1 What’s already published over MQTT

| Data | Topic (example) | When |
|------|------------------|------|
| **Common info** (battery/inverter-derived) | `BE/info` | Every publish interval (default 5 s). Includes: SOC, SOC_real, temperatures, battery voltage/current, power, min/max cell voltage, capacity, max charge/discharge power, balancing status, BMS status, event level, emulator status. For a second battery: same fields with `_2` suffix. |
| **All cell voltages** | `BE/spec_data` (and `BE/spec_data_2` for battery 2) | Only if **“Send all cellvoltages via MQTT”** (MQTTCELLV) is enabled in Settings. JSON array of every cell voltage in V. |
| **Cell balancing** (per-cell on/off) | `BE/balancing_data` (and `_2`) | Only when MQTTCELLV is enabled. JSON array of balancing status per cell. |
| **Events** (alarms, warnings) | Events topic | When events occur; supports Home Assistant autodiscovery. |
| **Status** | `BE/status` | “online” when connected. |

The **common info** is exactly the data the emulator has from CAN and/or Modbus: battery voltage, current, SOC, limits, temperatures, etc. So “Modbus data” in the sense of *values we got from the inverter or BMS over RS485* is already included there. There is no separate “raw Modbus registers” topic; that would require new code.

### 10.2 Enabling cell and “Modbus” logging via MQTT

1. **Settings → MQTT:** Enable MQTT, set broker, port, user/password if needed.
2. **MQTT publish interval (seconds):** Default 5; 5–10 s is a good balance. Shorter = more traffic and CPU.
3. **Send all cellvoltages via MQTT:** Check this to publish full cell voltage and cell balancing arrays.

That gives you:
- All **derived** data (from CAN and Modbus) in **common info** every 5–10 s.
- **All cell voltages and balancing** every 5–10 s when MQTTCELLV is on.

### 10.3 Buffer and load considerations

- **Message buffer:** MQTT uses a single buffer of **1024 bytes** (`MQTT_MSG_BUFFER_SIZE`). The common-info JSON usually fits. A **cell_voltages** array for 96 cells (~6 chars per value + brackets) can be ~600–700 bytes; for **192 cells** it can exceed 1024 and cause truncation or failure unless the code splits or the buffer is increased.
- **Publish interval:** MQTT runs in its own task; publishing every 5 s is light. If you set 1 s and enable full cell data for a large pack, you may hit buffer limits or slightly higher CPU use.
- **Recommendation:** Use **5–10 s** interval and enable **Send all cellvoltages via MQTT**. If you have a very large pack (e.g. 150+ cells), watch for failed publishes or consider a firmware change to use a larger buffer or split cell data into multiple messages.

### 10.4 What “logging all Modbus” would mean (if you want more)

- **Already covered:** All *interpreted* values from Modbus (and CAN) are in the datalayer and published in **common info** and (for cells) in **spec_data** / **balancing_data**. So “logging Modbus and cell data via MQTT” is already largely there.
- **Not implemented:** Publishing **raw Modbus register values** (e.g. every register read from the inverter or BMS). That would require new code to: read the desired registers, format them (e.g. JSON or CSV), and publish on a separate topic. The ESP32-S3 can handle the extra MQTT traffic if the publish rate stays reasonable (e.g. every 5–10 s).

**Summary:** You can log “Modbus and cell data” via MQTT by enabling MQTT and **Send all cellvoltages via MQTT**. Common info already contains all derived battery/inverter data (including from Modbus). For very large packs, keep an eye on the 1024-byte buffer or plan a firmware tweak to split/increase it.

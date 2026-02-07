# LilyGo T-CAN485: MCP2515 CAN add-on troubleshooting

If the **CAN add-on (MCP2515)** is not working on the LilyGo board, use this guide to check wiring, settings, and firmware messages.

**Full wiring guide:** For a connection table with **wire colors**, LilyGo header layout, and MCP2515 J4 pin order, see **[LILYGO_MCP2515_WIRING_TABLE.md](LILYGO_MCP2515_WIRING_TABLE.md)**.

### Inverter on native CAN, battery on MCP2515 – inverter doesn’t show battery values

If the **inverter is connected to native CAN** (LilyGo’s built-in CAN pins) and you set **Battery** to **Add-on MCP2515**, the battery emulation is sent only on the MCP2515 bus (TX5). The inverter is on the other bus (native), so it never sees that traffic and never “registers” SOC/voltage/current. **Fix:** Set **Battery communication (BATTCOMM)** to **CAN Native** so both inverter and battery use the same bus. Then the inverter will see the battery frames and values. Use MCP2515 for battery only when the inverter is physically connected to the MCP2515 bus.

---

## MCP2515 not working – how to fix (step by step)

If the inverter works on **CAN (Native)** but you get **no RX4/TX5** when you set **Inverter interface** to **CAN (MCP2515 add-on)**, the add-on is not initializing. Do the following in order.

1. **Serial boot log**  
   Connect at **115200** and power-cycle the board. Look for:
   - **"Dual CAN Bus (ESP32+MCP2515) selected"** then **"Can ok"** → init OK; you should then see **TX5** (and **RX4** if the inverter is on the MCP2515 bus). If you still see only RX0/TX1, confirm **Inverter interface** is **CAN (MCP2515 add-on)** and save/reboot.
   - **"Error Can: 0x...."** and **"MCP2515 pins: ..."** → init failed; continue below.

2. **Wiring (most common cause of "chip not detected")**  
   Use **[LILYGO_MCP2515_WIRING_TABLE.md](LILYGO_MCP2515_WIRING_TABLE.md)**. Check:
   - **3.3 V only** on MCP2515 VCC (not 5 V).
   - **GND** common between LilyGo and module.
   - **SCK → GPIO 12**, **MOSI (SI) → GPIO 5**, **MISO (SO) → GPIO 34**, **CS → GPIO 18**, **INT → GPIO 35**. Do not swap MOSI and MISO.
   - Connector orientation on the module (e.g. J4 pin 1 = INT). Short or wrong pin = no response.

3. **Settings**  
   - **CAN addon crystal (Mhz):** **8** for 8 MHz crystal, **16** for 16 MHz (try the other if one fails).
   - **BMS Power pin:** **Pin 25** (not 18; 18 is MCP2515 CS).
   - **SMA enable pin:** **Pin 33** (not 5; 5 is MCP2515 MOSI).

4. **Retry**  
   Newer firmware waits 150 ms after SPI start and **retries once** if the chip is not detected. Power-cycle again; if the second try succeeds, you’ll see **"Can ok"** after the retry message.

5. **Hardware**  
   If it still fails: check 3.3 V at the module, continuity for each signal, and try another MCP2515 module if you have one. A logic analyzer on SCK/CS during boot can confirm whether the ESP32 is toggling SPI; if CS never goes low or SCK doesn’t move, the wiring or pinout is wrong.

---

## Prove the MCP2515 is working

Use these steps to confirm the add-on is detected, transmitting, and (optionally) receiving.

### Step 1: Init must succeed (serial)

1. Set **at least one** of **Battery** or **Inverter** to **Add-on CAN via GPIO MCP2515** in **Settings**, then save and reboot.
2. Connect **serial at 115200** and reset the board (or power cycle).
3. Look for one of:
   - **"Dual CAN Bus (ESP32+MCP2515) selected"** then **"Can ok"** → MCP2515 init **passed** (chip seen on SPI).
   - **"Error Can: 0x...."** and **"MCP2515 pins: ..."** → init **failed**; use the decoded message and the checklist in section 4 to fix wiring/CANFREQ/BMS_POWER.

If you never see "Dual CAN Bus" or "Can ok", the firmware is not trying to init the MCP2515 (no battery/inverter is set to MCP2515).

### Step 2: See TX on MCP2515 (proves transmit path)

1. Keep **Inverter** (or Battery) set to **Add-on CAN via GPIO MCP2515**.
2. In **Settings**, enable **CAN message logging via USB serial** (or use the CAN logger page).
3. Open serial at 115200 (or watch the web CAN log). You should see lines like:
   - **TX5** *ID* *[DLC]* *bytes...*

**TX5** = transmit on the MCP2515 bus. If you see **TX5** traffic, the firmware is sending frames through the MCP2515; that proves SPI, CS, and the chip's transmit path are working.

### Step 3: See RX on MCP2515 (proves receive path)

- **Option A – Inverter on MCP2515:** Connect the inverter's CAN to the MCP2515 module's screw terminals (CAN_H, CAN_L, 120 Ω at one end if needed). Set **Inverter** to **Add-on CAN via GPIO MCP2515**, **Battery** to the same or Native as needed. With CAN logging on, look for **RX4** in the log (RX4 = receive on MCP2515). If the inverter sends anything (e.g. discovery, requests), you'll see **RX4** and the inverter may start showing battery values.
- **Option B – Loopback (no inverter):** Some modules let you do a simple bus loopback: connect **CAN_H** and **CAN_L** on the MCP2515 through a **120 Ω** resistor (and nothing else). Then the transceiver may echo your own frames and you might see **RX4** when **TX5** is active. This is hardware-dependent; if you never see RX4 in loopback, that's not conclusive.

**RX4** = receive on MCP2515. Any **RX4** line in the log proves the MCP2515 receive path and bus wiring are working.

### Summary

| What you see | Meaning |
|--------------|--------|
| "Can ok" in serial | MCP2515 init OK (chip detected). |
| **TX5** in CAN log | MCP2515 transmit path working. |
| **RX4** in CAN log | MCP2515 receive path working (inverter or loopback). |
| Inverter shows SOC/voltage | Full path working: BE → MCP2515 → inverter → BE. |

---

## 1. Pinout (LilyGo → MCP2515 module)

The firmware expects these GPIOs for the add-on (see `hw_lilygo.h`):

| Function | LilyGo GPIO | MCP2515 module pin |
|----------|-------------|--------------------|
| SCK      | **12**      | SCK                |
| MOSI     | **5**       | SI (data in)       |
| MISO     | **34**      | SO (data out)      |
| CS       | **18**      | CS                 |
| INT      | **35**      | INT (interrupt out)|
| 3.3V     | 3.3V        | VCC                |
| GND      | GND         | GND                |

- **Do not** swap MOSI and MISO.
- Use **3.3V** only (not 5V) for the MCP2515 module.
- If your module has an **RST** pin, tie it to 3.3V or leave unconnected (LilyGo HAL has no RST GPIO for the add-on).

## 2. Crystal frequency (CANFREQ)

The MCP2515 uses an external crystal; the firmware must match it.

- **8 MHz** is the default in firmware and in the web settings.
- Many modules use **16 MHz**; if so, set **CAN addon crystal (Mhz)** to **16** in the web UI: **Settings → CAN addon crystal (Mhz)**.

Wrong frequency can cause "chip not detected" (0x0001) or "timeout entering normal mode".

## 3. Serial / event log messages

After boot, open **serial monitor** (e.g. 115200 baud) or check the **Events** list in the web UI.

- **"Can ok"** – MCP2515 init succeeded.
- **"Error Can: 0x...."** – Init failed. The firmware now decodes the code and prints:
  - **0x0001 (kNoMCP2515)** – Chip not detected → check wiring (especially SCK/MOSI/MISO/CS), 3.3V/GND, and **CANFREQ** (8 or 16).
  - **0x0008 (INT pin)** – INT pin not usable as interrupt on this board.
  - **0x0020 (RequestedModeTimeOut)** – Timeout entering normal mode → power/crystal/wiring.
  - **0x0002 (TooFarFromDesiredBitRate)** – Try changing CANFREQ (8 or 16 MHz).

It also prints the pins and crystal used, e.g.  
`MCP2515 pins: CS=18 INT=35 SCK=12 MISO=34 MOSI=5 crystal=8 MHz`  
Use this to confirm wiring and CANFREQ.

## 4. Checklist

1. **Wiring** – SCK→12, MOSI→5, MISO→34, CS→18, INT→35, 3.3V, GND. No shorts.
2. **Pin conflicts with MCP2515** – When using the MCP2515 add-on, two settings must **not** use the same GPIOs as the add-on:
   - **BMS Power pin:** must be **Pin 25** (not Pin 18). MCP2515 CS uses GPIO 18.
   - **SMA enable pin:** must be **Pin 33** (not Pin 5). MCP2515 MOSI uses GPIO 5.
   In **Settings**, set **BMS Power pin** to **Pin 25** and **SMA enable pin** to **Pin 33** so they do not conflict with MCP2515 CS and MOSI.
3. **CANFREQ** – Web **Settings → CAN addon crystal (Mhz)** = **8** or **16** to match your module.
4. **Battery/Inverter interface** – In Settings, set the interface that should use the add-on to **"Add-on CAN via GPIO MCP2515"** (not Native CAN).
5. **Serial** – Reproduce the issue and capture the exact "Error Can: 0x...." and the line starting with "MCP2515 pins:".
6. **Events** – Web UI **Events** may show **MCP2515 init failure** with the same error code.

## 5. Board resets before CAN init (GPIO 78 / 568 errors)

If the board **resets in a loop** and you see in serial:

- `gpio_pullup_en(78): GPIO number error`
- `gpio_isr_handler_remove(568): GPIO isr service is not installed`

then something is crashing **before** or **soon after** CAN init (invalid GPIO 78 and 568 on ESP32). To see if MCP2515 init would succeed:

1. In **Settings**, turn **off** **SD logging** and **CAN SD logging** (so the SD/MMC path is not used).
2. **Remove the SD card** from the slot (if present).
3. Reboot and watch serial for **"Dual CAN Bus"** and **"Can ok"**.

If you then get "Can ok", MCP2515 is working and the crash is elsewhere (e.g. SD or another task). If the board still resets, the crash may be in CAN or another early init; run the serial monitor **locally** from power-on and capture the full log (and any stack trace) to report.

**Firmware safeguards:** The HAL now rejects invalid GPIO numbers (e.g. &gt; 39 on ESP32), and SD card init skips if any SD pin is out of range. MCP2515 init is skipped if the INT pin is not defined or out of range. That prevents our code from ever calling the GPIO layer with an invalid pin; if you still see GPIO 78 or 568 after updating, the source is likely inside a library (e.g. Arduino ESP32 core or SD_MMC) and the workaround above (disable SD logging, remove SD card) is the way to get to "Can ok".

## 6. Hardware checks (optional)

- **Voltage** – 3.3V on MCP2515 VCC.
- **Continuity** – Each signal from LilyGo header to the correct pin on the MCP2515 module.
- **Other use of GPIO 18** – On LilyGo, BMS_POWER can be configured as GPIO 18; if so, it must not conflict with MCP2515 CS. Default BMS_POWER is GPIO 18; if you use the add-on, ensure BMS_POWER is set to the option that does **not** use GPIO 18 (e.g. GPIO 25), or use a different board/HAL that separates these.

If you have a logic analyzer or oscilloscope, verify SCK toggles when the firmware talks to the MCP2515 (during init); if CS never goes low or SCK never moves, the SPI or CS wiring is wrong.

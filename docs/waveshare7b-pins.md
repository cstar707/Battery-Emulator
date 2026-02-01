# Waveshare ESP32-S3-Touch-LCD-7B – Pin Reference

Pin usage for the Battery Emulator on this board: what is already used and what you can use for contactors and other integration.

---

## Already used by the board (do not use)

| Function | Pins | Notes |
|----------|------|--------|
| **LCD (RGB)** | 0, 1, 2, 3, 5, 7, 10, 14, 17, 18, 21, 38, 39, 40, 41, 42, 45, 46, 47, 48 | Display data, sync, clock |
| **Touch (GT911)** | 4 (IRQ), 8 (SDA), 9 (SCL) | I2C + interrupt |
| **CAN / USB** | 19 (RX), 20 (TX) | TJA1051; EXIO5 selects CAN vs USB |
| **RS485** | 15 (TX), 16 (RX) | |
| **SPI (SD + MCP2515)** | 11 (MOSI), 12 (SCK), 13 (MISO), 6 (MCP2515 CS) | SD_CS via IO expander EXIO4 |
| **Flash / PSRAM** | 26–32, 33–37 | Do not use (internal) |

**IO expander (CH422G, I2C 0x24):**  
EXIO1=Touch RST, EXIO2=Backlight, EXIO3=LCD RST, EXIO4=SD_CS, EXIO5=CAN_SEL, EXIO6=LCD_VDD_EN.  
**EXIO0 and EXIO7** are not used by the display. You can use them as **two extra outputs** (e.g. relays for precharge, BMS power, or status). The firmware can mirror contactor/precharge state to these pins – see below.

---

## Free GPIOs for your wiring

Only **two** ESP32 GPIOs are left for contactors / integration:

| GPIO | Suggested use in firmware | Typical wiring |
|------|----------------------------|----------------|
| **43** | Positive contactor | Relay/contactor driver (optocoupler or MOSFET) |
| **44** | Negative contactor | Relay/contactor driver (optocoupler or MOSFET) |

These are assigned in `hw_waveshare7b.h` as:

- `POSITIVE_CONTACTOR_PIN()` → **GPIO 43**
- `NEGATIVE_CONTACTOR_PIN()` → **GPIO 44**
- `PRECHARGE_PIN()` → not connected (NC)
- `BMS_POWER()` → not connected (NC)

So you can drive **two contactors** (e.g. positive and negative) from GPIO 43 and 44. Precharge and BMS power are not assigned to pins on this board.

---

## Where to connect 43 and 44

- Check the board’s **sensor / PH2.0** connector or schematic to see if **43** and **44** are brought out.
- If they are not on a connector, you may need to solder to the ESP32-S3 module pads or to test points that the schematic shows for GPIO 43 and 44.

Use a relay board or contactor driver (optocoupler + MOSFET or similar) between the GPIOs and the contactor coils; do not drive coils directly from the pins.

**Power:** The same connector typically has **3.3V** and **GND**, so you can power I2C peripherals, relay boards, or optocouplers from one cable (SDA, SCL, 3.3V, GND).

---

## I2C port expander (CH422G) – extra outputs

The board’s **CH422G** is on I2C address **0x24** (same bus as touch, GPIO 8/9). It has 8 pins; the firmware uses 1, 2, 4, 5, 6 for touch, backlight, SD_CS, CAN_SEL, LCD. **EXIO0 and EXIO7 are free.**

You can use them as two extra digital outputs, e.g.:

| Expander pin | Suggested use        | Typical wiring                    |
|--------------|----------------------|-----------------------------------|
| **EXIO0**    | Precharge relay      | Relay/optocoupler for precharge   |
| **EXIO7**    | BMS power / status   | Relay for BMS power or “run” LED  |

The firmware **mirrors** internal state to these pins (see “Expander mirror” below), so EXIO0/EXIO7 follow contactor/precharge without assigning them in the HAL. Drive relays/optocouplers from EXIO0/EXIO7; do not connect contactor coils directly.

### Expander mirror (automatic)

The display code updates EXIO0 and EXIO7 every 2 seconds from the datalayer:

| Pin   | Logic (HIGH when) |
|-------|--------------------|
| EXIO0 | Main contactors engaged |
| EXIO7 | Precharge in progress, precharge completed, or contactors engaged |

So you can wire EXIO0 to a “main contactors closed” relay/LED and EXIO7 to a precharge relay or “safe to close” indicator.

**Note:** EXIO3 is reserved for LCD reset; EXIO4 is SD_CS; EXIO6 is LCD_VDD. Do not use those for your circuits.

---

## If you need more than two outputs

- **More than EXIO0 + EXIO7:** Add an external I2C GPIO expander (e.g. PCA9555) on the same I2C bus (GPIO 8/9).

---

## Summary

| Purpose | Pin | Notes |
|--------|-----|--------|
| Positive contactor | **GPIO 43** | Use for main positive contactor |
| Negative contactor | **GPIO 44** | Use for main negative contactor |
| Precharge | *(none)* | NC in firmware; use external logic or expander |
| BMS power | *(none)* | NC in firmware; use external logic or expander |

Confirm GPIO 43 and 44 on your specific board revision (schematic or “Sensor terminal” / PH2.0 pinout) before wiring.

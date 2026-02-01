# LilyGO T-CAN485 – Pin Reference

Board: [T-CAN485](https://github.com/Xinyuan-LilyGO/T-CAN485) – ESP32, 4MB Flash, 1× CAN (SN65HVD231), 1× RS485 (MAX13487EESA+). 5V–12V input, WS2812 RGB LED, TF card.

**Our HAL (`hw_tcan485.h`) is aligned with the [official Battery-Emulator](https://github.com/dalathegreat/Battery-Emulator) pinout**, which uses `HW_LILYGO` for this board. See the [Battery-Emulator wiki](https://github.com/dalathegreat/Battery-Emulator/wiki) for hardware setup, inverters, and CAN troubleshooting.

---

## Official Battery-Emulator vs LilyGO hardware doc

| Function        | Official BE (hw_lilygo) | LilyGO T-CAN485 README |
|-----------------|-------------------------|-------------------------|
| RS485 TX/RX     | 22, 21                  | 22, 21 ✓                |
| RS485 EN (DE/RE)| **17**                  | **9** (CALLBACK=17)     |
| RS485 SE        | **19**                  | not listed              |
| CAN TX/RX       | **27, 26**              | not listed              |
| CAN SE          | **23**                  | not listed              |
| PIN_5V_EN       | **16** (ME2107)         | ME2107 EN=16 ✓          |
| SD (TF)         | 2, 15, 14, 13           | 2, 15, 14, 13 ✓         |
| LED             | 4 (WS2812)              | 4 ✓                     |
| Contactors      | 32, 33, precharge=25    | —                       |
| BMS power       | 18 or 25 (web option)   | —                       |
| SMA enable      | 5 or 33 (web option)    | —                       |

We follow **official BE** so firmware and wiki match. If your board uses RS485 EN=9 (per LilyGO doc), you may need to change `RS485_EN_PIN()` to GPIO 9 in the HAL or verify your board revision.

---

## HAL assignments (this project, HW_TCAN485)

Matches [dalathegreat/Battery-Emulator](https://github.com/dalathegreat/Battery-Emulator) `hw_lilygo.h` for T-CAN485:

| Function              | GPIO | Notes                          |
|-----------------------|------|---------------------------------|
| PIN_5V_EN             | 16   | ME2107 booster                  |
| RS485 TX / RX         | 22, 21 |                             |
| RS485 EN / SE         | 17, 19 | Half-duplex direction        |
| CAN TX / RX / SE      | 27, 26, 23 | Native CAN                 |
| SD MISO/MOSI/SCLK/CS  | 2, 15, 14, 13 | TF card                 |
| LED (WS2812)          | 4    | Status LED                      |
| Positive / Negative   | 32, 33 | Contactors                   |
| Precharge             | 25   |                                |
| BMS power             | 18 or 25 | Web setting GPIOOPT2        |
| Second battery        | 15   | SECOND_BATTERY_CONTACTORS       |
| SMA inverter enable   | 5 or 33 | Web setting GPIOOPT3        |
| Equipment stop        | 35   |                                |
| WUP1 / WUP2           | 25, 32 | Wake-up                      |
| MCP2515 (add-on)      | SCK=12, MOSI=5, MISO=34, CS=18, INT=35 | Optional second CAN |
| MCP2517 FD (add-on)   | Same SPI-style pins | Optional CAN FD          |

---

## Web settings (GPIOOPT2 / GPIOOPT3)

- **BMS Power pin (GPIOOPT2):** PIN18 (default) or PIN25.
- **SMA enable pin (GPIOOPT3):** PIN5 (default) or PIN33.

These are shown in the Settings page when building for **HW_TCAN485** (or HW_LILYGO).

---

## Interfaces

- **Native CAN (27/26):** Tesla battery / inverter. Wiki: remove built-in terminator when using CAN inverter.
- **RS485 (21/22, EN=17):** Modbus inverter (e.g. BYD Modbus, Kostal).
- **SD (2, 13, 14, 15):** TF card.
- **Contactors:** 32 (positive), 33 (negative), 25 (precharge). Use relay/optocoupler; do not drive coils directly.

---

## Build

Use the **tcan485** PlatformIO env:

```bash
pio run -e tcan485
```

---

## References

- [Official Battery-Emulator](https://github.com/dalathegreat/Battery-Emulator) – firmware and [wiki](https://github.com/dalathegreat/Battery-Emulator/wiki) (hardware, inverters, CAN).
- [LilyGO T-CAN485](https://github.com/Xinyuan-LilyGO/T-CAN485) – hardware repo and pin table.
- [Integration guide](integration-guide-mcp2515-rs485-contactors.md) – MCP2515, RS485/Modbus, contactors (if present in this repo).

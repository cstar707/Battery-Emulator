# LilyGO T-CAN485 – Pin Reference

Board: [T-CAN485](https://github.com/Xinyuan-LilyGO/T-CAN485) – ESP32, 4MB Flash, 1× CAN (SN65HVD231), 1× RS485 (MAX13487EESA+). 5V–12V input, WS2812 RGB LED, TF card.

This document summarizes pin usage for the Battery Emulator on this board. Pin table from the [T-CAN485 README](https://github.com/Xinyuan-LilyGO/T-CAN485).

---

## Pin overview (from T-CAN485)

| Function   | ESP32 GPIO | Notes                    |
|-----------|------------|--------------------------|
| RS485 TX  | 22         |                          |
| RS485 RX  | 21         |                          |
| RS485 CALLBACK | 17   | (not used in HAL)        |
| RS485 EN  | 9          | Direction (DE/RE)        |
| WS2812    | 4          | DATA                     |
| ME2107 EN | 16         | Booster enable           |
| SD MISO   | 2          | TF card                  |
| SD MOSI   | 15         |                          |
| SD SCLK   | 14         |                          |
| SD CS     | 13         |                          |

CAN (SN65HVD231) GPIOs are not listed in the official pin table. The HAL uses **TX=26, RX=25** as a common ESP32 TWAI pair; **verify from the board schematic** if you have issues.

---

## HAL assignments (Battery Emulator)

| Function           | GPIO | Notes                          |
|--------------------|------|---------------------------------|
| RS485 TX           | 22   |                                |
| RS485 RX           | 21   |                                |
| RS485 EN           | 9    | Half-duplex direction          |
| CAN TX             | 26   | Verify from schematic          |
| CAN RX             | 25   | Verify from schematic          |
| SD MISO/MOSI/SCLK/CS | 2, 15, 14, 13 | TF card        |
| Positive contactor | 32   |                                |
| Negative contactor | 33   |                                |
| Precharge          | 27   |                                |
| BMS power          | NC   |                                |

No MCP2515 on board; no display (SMALL_FLASH_DEVICE, 4MB).

---

## Interfaces

- **Native CAN (25/26):** Tesla battery / inverter. Confirm TX/RX on your board.
- **RS485 (21/22, EN=9):** Modbus inverter (e.g. BYD Modbus, Kostal). EN is used for direction.
- **SD (2, 13, 14, 15):** TF card.
- **Contactors:** 32 (positive), 33 (negative), 27 (precharge). Use relay/optocoupler; do not drive coils directly.

---

## Build

Use the **tcan485** PlatformIO env:

```bash
pio run -e tcan485
```

---

## Integration guide

For MCP2515 add-on (not on T-CAN485), RS485/Modbus inverter setup, and contactor/precharge wiring, see [integration-guide-mcp2515-rs485-contactors.md](integration-guide-mcp2515-rs485-contactors.md).

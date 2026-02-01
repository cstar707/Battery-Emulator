# Waveshare ESP32-P4-WIFI6-Touch-LCD-7B – Pin Reference

Board: [ESP32-P4-WIFI6-Touch-LCD-7B](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-7b.htm) (7" 1024×600 MIPI DSI touch display, WiFi 6 via ESP32-C6).

This document summarizes pin usage for the Battery Emulator on this board. For schematics and demos see the [Waveshare wiki](https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-7B).

**Integration guide:** For MCP2515 add-on CAN, RS485/Modbus inverter wiring, and contactor/precharge GPIO details, see [integration-guide-mcp2515-rs485-contactors.md](integration-guide-mcp2515-rs485-contactors.md).

## SoC and connectivity

- **MCU:** ESP32-P4 (RISC-V dual-core + LP core), 32 MB Nor Flash, 32 MB PSRAM in package.
- **WiFi/BT:** ESP32-C6-MINI over SDIO (WiFi 6, BLE 5).
- **Display:** MIPI DSI 7" 1024×600 IPS, capacitive touch (GT911).
- **No CH422G IO expander** (unlike the ESP32-S3 7B).

## Pin assignments (from Waveshare wiki)

| Function   | GPIO | Notes                          |
|-----------|------|---------------------------------|
| I2C SDA   | 7    | Touch (GT911), peripherals      |
| I2C SCL   | 8    |                                |
| I2S DIN   | 9    | ES7210 mic input               |
| I2S LRCK  | 10   |                                |
| I2S DOUT  | 11   | ES8311 line out               |
| I2S SCLK  | 12   |                                |
| I2S MCLK  | 13   |                                |
| **Contactor +** | 14 | HAL: positive contactor (verify schematic) |
| **Contactor −** | 15 | HAL: negative contactor (verify schematic) |
| CAN RX    | 21   | Native CAN                     |
| CAN TX    | 22   |                                |
| RS485 TX  | 26   |                                |
| RS485 RX  | 27   |                                |
| SDMMC D0  | 39   | TF card 4-wire                 |
| SDMMC D1  | 40   |                                |
| SDMMC D2  | 41   |                                |
| SDMMC D3  | 42   |                                |
| SDMMC CLK | 43   |                                |
| SDMMC CMD | 44   |                                |
| PA_Ctrl   | 53   | Power amplifier enable         |

## Interfaces

- **I2C (GPIO7/8):** Touch, optional external I2C devices.
- **CAN (GPIO21/22):** Native CAN for Tesla/inverter (no IO expander; always available).
- **RS485 (GPIO26/27):** PH2.0 4-pin RS485 header.
- **SDMMC (39–44):** 4-wire TF card; no separate CS in HAL.
- **PH2.0 12-pin header:** 17 programmable GPIOs + power (exact GPIO list in schematic).

## Contactor outputs

The HAL uses **GPIO14** and **GPIO15** for positive and negative contactor outputs. Confirm against the board schematic; if the 12-pin header uses different GPIOs, change `hw_waveshare_p4_7b.h` accordingly. For precharge, BMS power, wiring (relay/optocoupler), and sequence, see [integration-guide-mcp2515-rs485-contactors.md](integration-guide-mcp2515-rs485-contactors.md).

## Display (MIPI DSI)

The 7B UI (LVGL, 1024×600) is not yet ported to this board. Current build uses a **display stub** (no LVGL, no MIPI DSI). Porting will require an ESP32-P4 MIPI DSI driver and LVGL integration (e.g. from [Waveshare demos](https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-7B) or ESP-IDF examples).

## Build

Use the `waveshare_p4_7b` PlatformIO env. ESP32-P4 support depends on the platform (e.g. `espressif32` 6.x or a fork with P4). If the default platform has no P4 board, switch to a platform/board that supports ESP32-P4.

# LILYGO T-Connect

Board documentation and pinout for the **LILYGO T-Connect** (ESP32-S3, RS485 + CAN).  
Source: [LILYGO T-Connect Wiki](https://wiki.lilygo.cc/get_started/en/High_speed/T-Connect/T-Connect.html#Pin-Overview).

## Overview

T-Connect is a multi-functional development board based on the **ESP32-S3** chip, with 8MB PSRAM and 16MB Flash, Wi-Fi/Bluetooth, and **RS485/CAN** industrial protocols. It has a built-in APA102 LED driver for RGB strips, relay, and QWIIC expansion. Up to **three RS485** and **one CAN** bus can be used (shared via 4 UART pairs; configuration via jumpers/software).

| Component | Description |
|-----------|-------------|
| MCU | ESP32-S3-R8 |
| FLASH | 16MB |
| PSRAM | 8MB (Octal SPI) |
| Communication | RS485 (UART) / CAN (TWAI) |
| LED Driver | APA102 |
| Relay | 10A output |
| Wireless | 2.4 GHz Wi-Fi & Bluetooth 5 (LE) |
| USB | 1× USB Type-C (OTG) |
| Output config | Max 3× RS485 + 1× CAN |
| Expansion | 1× QWIIC |
| Buttons | 1× RESET, 1× BOOT |
| Power | 7–12 V DC + 5 V/500 mA USB |
| Mounting | 4× 2 mm positioning holes |
| Dimensions | 94×83×13 mm |

## Pin overview (ESP32-S3)

### LED (APA102)

| Function | ESP32-S3 GPIO |
|----------|----------------|
| APA102_DATA | **IO8** |
| APA102_CLOCK | **IO3** |

### CAN and RS485 shared UART pins

Four pairs; each can be configured as CAN or RS485 (jumpers/software).

| Function | ESP32-S3 GPIO |
|----------|----------------|
| TX_1 | **IO4** |
| RX_1 | **IO5** |
| TX_2 | **IO6** |
| RX_2 | **IO7** |
| TX_3 | **IO17** |
| RX_3 | **IO18** |
| TX_4 | **IO9** |
| RX_4 | **IO10** |

## Battery-Emulator HAL mapping (T-Connect)

- **Native CAN (TWAI):** TX_1 / RX_1 → **GPIO 4 / GPIO 5**
- **RS485 (primary):** TX_2 / RX_2 → **GPIO 6 / GPIO 7** (DE/RE pins not in wiki; use NC or board jumpers)
- **LED:** **GPIO 8** (APA102 data; simple LED use on this pin)
- **SD card / contactors:** Not defined on board; HAL uses `GPIO_NUM_NC` where not applicable.

## Quick start (PlatformIO)

- Environment: **`tconnect`**
- Build: `pio run -e tconnect`
- Upload: `pio run -e tconnect -t upload`
- Monitor: `pio device monitor -e tconnect`

**Upload tip:** If upload fails, hold the **BOOT** button while starting upload.

## Arduino IDE (reference)

| Setting | Value |
|---------|--------|
| Board | ESP32S3 Dev Module |
| Upload Speed | 921600 |
| USB Mode | Hardware CDC and JTAG |
| USB CDC On Boot | Enabled |
| Flash Size | 16MB (128Mb) |
| Partition Scheme | 16M Flash (3MB APP/9.9MB FATFS) |
| PSRAM | OPI PSRAM |
| CPU Frequency | 240 MHz (WiFi) |
| Flash Mode | QIO 80 MHz |

## Resources

- [LILYGO T-Connect Wiki](https://wiki.lilygo.cc/get_started/en/High_speed/T-Connect/T-Connect.html#Pin-Overview)
- [LILYGO Mall – T-Connect](https://lilygo.cc/products/t-connect)
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
- Dependent libraries: FastLED, ESP32TWAI (IDF TWAI)

# LilyGo T-CAN485 Wiring Documentation

## Board Pin Header Layout (2×6)

Physical arrangement of the IO/power header on the board. Square pad = Pin 1 (orientation marker).

| Row | Left Pin | Right Pin |
|-----|----------|-----------|
| 1   | GND      | IO25      |
| 2   | IO32     | IO33      |
| 3   | IO5      | IO12      |
| 4   | IO34     | IO35      |
| 5   | IO18     | VDD       |
| 6   | GND      | VDD       |

*Two LEDs above the header indicate polarity (- / +).*

## Wire Color Reference

| GPIO | Wire Color | Function (Dala's Config) |
|------|------------|--------------------------|
| GND  | Black      | Ground                   |
| IO5  | Yellow     | MCP2515 MOSI (SDI)       |
| IO12 | Green      | MCP2515 SCK (Clock)      |
| IO32 | Yellow     | Positive Contactor       |
| IO33 | Green      | Negative Contactor       |
| IO18 | Red        | MCP2515 CS (Chip Select) |
| IO25 | Purple     | Precharge Relay          |
| IO34 | Grey       | MCP2515 MISO (SDO)       |
| IO35 | White      | MCP2515 INT (Interrupt)  |

## MCP2515 CAN Module Wiring

| MCP2515 Pin | LilyGo GPIO | Wire Color |
|-------------|-------------|------------|
| VCC         | 3.3V        | -          |
| GND         | GND         | Black      |
| CS          | IO18        | Red        |
| SO (MISO)   | IO34        | Grey       |
| SI (MOSI)   | IO5         | Yellow     |
| SCK         | IO12        | Green      |
| INT         | IO35        | White      |

## Contactor/Relay Wiring

| Function            | LilyGo GPIO | Wire Color |
|---------------------|-------------|------------|
| Precharge Relay     | IO25        | Purple     |
| Positive Contactor  | IO32        | Yellow     |
| Negative Contactor  | IO33        | Green      |

## Notes

- This wiring follows Dala's original configuration in `hw_lilygo.h`
- MCP2515 module provides secondary CAN bus for battery communication
- Contactor control requires `contactor_control_enabled = true` in settings
- Precharge sequence: NEG ON → wait → PRECHARGE ON → wait → POS ON → PRECHARGE OFF

## Pinout Reference (hw_lilygo.h defaults)

```
Native CAN:     TX=27, RX=26
RS485:          TX=22, RX=21, EN=17, SE=19
5V Enable:      IO16
LED:            IO4
SD Card:        MISO=2, MOSI=15, SCLK=14, CS=13
```

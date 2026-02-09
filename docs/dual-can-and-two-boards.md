# Dual CAN and Using Two Boards

## How Battery-Emulator uses CAN

The firmware expects **one MCU** with **one or two CAN interfaces**:

- **Battery CAN** – talks to the EV battery pack (e.g. Tesla over native CAN).
- **Inverter CAN** – talks to the inverter (e.g. Solis over MCP2515/Pylon).

You choose which physical port is “battery” and which is “inverter” in the web UI (Settings → BATTCOMM / INVCOMM). The same MCU runs both the battery driver and the inverter protocol and bridges them in the datalayer.

---

## One board, two CAN ports (recommended)

On hardware that has two CAN ports, you use **one board** and assign:

- **BATTCOMM** = the port connected to the battery (e.g. “CAN Native”).
- **INVCOMM** = the port connected to the inverter (e.g. “CAN Addon MCP2515”).

Examples:

- **Waveshare 7B:** Battery = Native CAN (GPIO19/20), Inverter = MCP2515 add-on (SPI + CS on GPIO6). If the MCP2515 is not working, you only have one usable CAN on this board.
- **LilyGo T-CAN485:** One native CAN + one MCP2515. Assign one to battery, one to inverter.
- **LilyGo T_2CAN:** CAN A (MCP2515) + CAN B (Native). Same idea: battery on one, inverter on the other.

So “two CANs” in the project means **two interfaces on the same board**, not two separate boards.

---

## Can we use two LilyGo boards (one for battery, one for inverter)?

**Short answer:** Not with the current firmware. There is no support for splitting “battery CAN on one board” and “inverter CAN on another board.”

**Why:** The design is “one MCU, two CAN ports.” The battery driver and inverter driver run on the same chip and share the same datalayer. There is no code path that says “get battery data from another device over UART/WiFi” or “run only the inverter side.”

**What would be needed for two boards (out of scope for current project):**

1. **Board A (battery side):**  
   Firmware that only reads the battery CAN, decodes to a datalayer (or compact protocol), and sends that over **UART or WiFi** to Board B.

2. **Board B (inverter side):**  
   Firmware that receives battery data from Board A (UART/WiFi), fills the datalayer, and runs **only** the inverter protocol and contactor logic, sending to the inverter CAN.

That would require new “gateway”/“split” firmware and possibly wiring (UART between boards or both on the same WiFi). It is a substantial change and is not implemented.

---

## If the second MCP2515 is not working

**Practical options:**

1. **Fix the MCP2515 on the current board**  
   - Check wiring (CS, SCK, MOSI, MISO, INT, 3.3V, GND).  
   - Try another MCP2515 module or 8 MHz crystal.  
   - On Waveshare 7B, MCP2515 shares SPI with the SD card; confirm CS and that only one device is active at a time.

2. **Use a board that already has two CAN ports and is known to work**  
   - **LilyGo T_2CAN** (ESP32-S3, native CAN + MCP2515): assign battery to one port, inverter to the other, single board.  
   - Or another supported board with native + add-on CAN.

3. **Two separate LilyGo boards**  
   - Not supported as-is. Would require custom “battery gateway” + “inverter-only” firmware and a link (UART or WiFi) between the two boards, as described above.

---

## Summary

| Setup | Supported? |
|-------|------------|
| One board, battery on CAN A and inverter on CAN B | Yes – set BATTCOMM and INVCOMM in Settings. |
| Two boards: one only battery CAN, one only inverter CAN | No – would need new gateway/split firmware. |

Best path if the add-on MCP2515 is unreliable: fix wiring/crystal/module on the current board, or switch to a single board that has two working CAN ports (e.g. LilyGo T_2CAN) and keep using one MCU for both battery and inverter.

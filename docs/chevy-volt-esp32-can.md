# Chevy Volt Gen 2 – ESP32 CAN Bus (TRS485CAN)

We use **Battery-Emulator (BE)** as the platform to **read the Volt's CAN bus** and **talk to the battery**: one firmware on the LilyGo RS485/CAN board handles CAN I/O, battery state (Bolt/Ampera driver), logging, and the web UI. This repo's **Bolt/Ampera** battery and **Chevy Volt charger** support run inside BE.

## Board: LilyGo RS485/CAN (T-CAN485)

We use a **LilyGo T-CAN485** (or equivalent LilyGo RS485 + CAN board): one native CAN port for the car's OBD-II/CAN bus, plus RS485 for inverter or other devices. This repo supports it as the **`lilygo_330`** environment in PlatformIO.

- **CAN**: Connect to the Volt's OBD-II **pins 6 (CAN-H) and 14 (CAN-L)** via a male OBD-II plug; 500 kbit/s for Gen 2 Volt HS-CAN.
- **Power**: USB for development, or 12 V from OBD-II pin 16 through a step-down to 5 V/3.3 V if powering from the car.
- **RS485**: Available for solar inverter, meter, or other RS485/Modbus gear in a stationary setup.

**Build and flash:**
```bash
pio run -e lilygo_330
pio run -e lilygo_330 -t upload
```
Select **Bolt/Ampera** (or Volt charger) in the web UI for Volt-related use.

## Hardware (reference)

- **OBD-II**: Male connector; **pins 6 and 14** for CAN; **pin 16** = 12 V (use a step-down if powering the board from the car).
- **Optional**: SD card for standalone logging. The LilyGo board includes CAN transceiver and RS485; no separate transceiver needed.

## Protocol (Gen 2 Volt)

- **CAN**: ISO 11898, **500 kbit/s**, typically **CAN 2.0B** (11- and 29-bit IDs).
- **OBD-II**: HS-CAN on **pins 6 and 14**.
- **BECM on the CAN bus**: The **BECM** (Battery Energy Control Module) is on the same HS-CAN; when you connect BE and log or monitor the bus, you should see BECM traffic (e.g. broadcast frames and responses). The Bolt/Ampera driver in BE uses some of this (e.g. 7E7) for pack state; other traffic can be captured for analysis.
- **BECM / diagnostics**: Reading BECM fault codes (e.g. U26xx) usually needs **UDS** (Unified Diagnostic Services) requests, not only passive listening.

## Software: BE as the platform

- **Battery-Emulator (BE)** is the single software platform:
  - **Read CAN bus**: BE's CAN stack (native on LilyGo) receives frames from the Volt's OBD-II/CAN; the **Bolt/Ampera** battery driver parses them and updates SOC, voltage, current, etc. in the datalayer.
  - **Talk to the battery**: BE exposes battery state via the web UI, MQTT, and to inverters/chargers; the Bolt/Ampera driver handles the Volt/Bolt CAN protocol (e.g. 7E7 polls).
  - **CAN logging / replay**: Built-in CAN logging and replay in the web UI for capture and analysis.
- **External tools**: **SavvyCAN** for offline analysis of exported logs; **UDS** (e.g. via BE or a separate tool) for BECM DTCs and advanced diagnostics.

## Summary

Use **BE software** on the **LilyGo RS485/CAN board** (`lilygo_330`) to read the Volt CAN bus and talk to the battery: flash BE, select **Bolt/Ampera** (or Volt charger) in the web UI, connect CAN to OBD-II pins 6 and 14 at 500 kbit/s. BE handles CAN I/O, battery state, and optional RS485 to an inverter.

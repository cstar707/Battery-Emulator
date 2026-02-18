# Ruixu battery over CAN bus

The Battery-Emulator can talk to **Ruixu batteries** (e.g. Lithi2-16) over **CAN bus**. Ruixu supports closed-loop communication with inverters via CAN and RS485; this driver uses CAN only.

## Setup

1. **Settings → Battery:** Select **Ruixu (CAN)**.
2. **CAN wiring:** Connect the emulator’s CAN interface to the Ruixu master battery’s communication port (e.g. RJ45 OUT). Ruixu typically uses CAN on pins 4–5 of the RJ45; use the correct cable (e.g. CAT5e, T568B, max 10 m to inverter).
3. **Baud rate:** The driver uses **500 kbps** CAN. If your Ruixu BMS uses a different speed, the code can be changed in `RUIXU-BATTERY.h` (constructor).

## Protocol (TODO)

The Ruixu BMS CAN **message IDs and byte layout** are not yet filled in. The code in `Software/src/battery/RUIXU-BATTERY.cpp` is a skeleton:

- `handle_incoming_can_frame()` has a placeholder `switch (rx_frame.ID)` with no cases yet.
- Once you have the **Ruixu BMS CAN protocol** (e.g. from the Ruixu Operation and Maintenance Manual, Lithi2-16 docs, or Ruixu support), add the correct CAN IDs and byte mapping in `handle_incoming_can_frame()` and set `datalayer_battery->status.CAN_battery_still_alive` when a valid frame is received.

Some stationary BMS use Victron-style IDs (e.g. 0x351, 0x355, 0x356); others use a proprietary layout. Confirm with Ruixu documentation before adding IDs.

## Files

- `Software/src/battery/RUIXU-BATTERY.h` – Ruixu battery class (CanBattery).
- `Software/src/battery/RUIXU-BATTERY.cpp` – CAN RX handling and `update_values()`; add protocol details here.
- `Software/src/battery/Battery.h` – `BatteryType::Ruixu`.
- `Software/src/battery/BATTERIES.h` / `BATTERIES.cpp` – Ruixu registered in the battery list and factory.

## Inverter compatibility

Ruixu batteries are often used with Sol-Ark, Deye, Sunsynk, and others. When using this emulator with Ruixu as the battery and Sol-Ark as the inverter, select **Ruixu (CAN)** for the battery and **Sol-Ark LV** (or your inverter) for the inverter; the emulator then bridges battery CAN data to the inverter protocol.

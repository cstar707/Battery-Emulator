# Off-Grid Microgrid System Context

This document states the system understanding for the **entire power plant** and should be taken into consideration for all design and integration work.

---

## Microgrid model

- **Off-grid microgrid:** There is no backup grid connection. Energy demand must match production.
- **Microgrid Controller** coordinates:
  - **Battery energy storage system (BESS)** — e.g. Tesla pack(s) and the battery emulator.
  - **Solar and other generation** — PV inverters and other sources.
- The controller operates **all storage and generation assets in parallel as needed** so that supply and demand stay balanced.

---

## Assets in this system

| Asset | Capacity | Role | Interface |
|-------|----------|------|-----------|
| **Solis inverter (1) + Tesla battery 1** | **70 kWh** | BESS — storage; **controlled via RS485** (Modbus RTU). Connected to Tesla battery 1. | **RS485** |
| **Solark inverter system** | **64 kWh** | Inverter/generation — can be charged from Solis or charge Solis; coordinated with BESS and solar. | — |
| **Solis inverter (2)** *(planned)* | **50 kWh** | BESS — same as Solis 1; leave room on RS485 (and possibly CAN) or use a separate battery emulator. | **RS485** (or separate emulator) |
| **Enphase system 1** | — | Solar / generation. **Export or turn-on only when Solis or Solark needs charging.** | — |
| **Enphase system 2** | — | Solar / generation. **Export or turn-on only when Solis or Solark needs charging.** | — |

*Battery emulator(s)* talk to Tesla pack(s) over CAN and to Solis over RS485; they report SOC and limits and can accept setpoints (e.g. via MQTT) for coordination.

The T-Connect and Battery-Emulator firmware form part of the control and monitoring layer: they talk to the battery (CAN), to inverters and meters (CAN/RS485), and can report or accept setpoints via MQTT or the web UI for coordination with higher-level microgrid logic. Wiring, CAN channels, **RS485** (one channel for Solis 1; **reserve capacity for Solis 2**), and MQTT choices should support coordination among **BESS, Solark, both Solis inverters (RS485), and both Enphase systems**.

---

## Design: room for Solis inverter #2

- **RS485:** Leave at least one spare RS485 channel (or one address on a shared Modbus bus) for **Solis inverter #2** when it is added. The T-Connect has 3–4 RS485 channels: use one for Solis 1 and reserve one for Solis 2.
- **CAN:** If Solis #2 will be driven by the same emulator (inverter protocol over CAN), reserve a CAN channel or ensure the existing CAN allocation can carry a second inverter. If instead you use a **separate battery emulator** for Solis #2 (second Tesla pack + second T-Connect or second board), then that emulator has its own CAN and RS485; no need to reserve on the first.
- **Summary:** Plan wiring and firmware so that either (a) this emulator can talk to both Solis inverters over RS485 (and CAN if needed), or (b) Solis #2 is assigned to a second battery emulator and the first system only needs capacity for Solis 1.

---

## SOC management and export control

Storage capacities and control objectives:

- **Solis 1 (70 kWh)** — Tesla battery 1. SOC and export controlled via RS485.
- **Solark (64 kWh)** — Can be **tier-charged** from Solis (e.g. Solis exports to charge Solark when needed).
- **Solis 2 (50 kWh)** *(planned)* — Same logic as Solis 1 once added.

**Control objectives:**

1. **Manage all SOC levels** — Monitor and coordinate state of charge across Solis 1, Solark, and (when added) Solis 2 so the microgrid stays balanced and no storage is over- or under-used.
2. **Bidirectional charging between Solis and Solark:**
   - **Solis → Solark (tier charge Solark)** — When desired, use Solis (e.g. Solis 1) to export power to **charge the Solark** battery.
   - **Solark → Solis (charge Solis from Solark)** — When desired, use the Solark to export power to **charge the Solis** battery(ies). So Solis can be charged from Solark as well as the other way around.
   - **Zero export and run loads** — When not transferring between storage units, run in **zero-export** mode: use generation to supply loads only, with no export; storage (SOC) is managed to support that.
3. **Enphase (export / turn-on only when storage needs charging)** — The Enphase systems may **only export or be turned on when Solis or Solark needs charging**. When neither storage needs a charge, Enphase does not export (or stays off). This avoids excess generation when there is nowhere for it to go.
4. **Coordination** — The Microgrid Controller (and/or battery emulator logic, MQTT setpoints) must decide: when to allow Enphase to export or turn on (based on Solis/Solark charging need); when to tier-charge in either direction (Solis↔Solark); and when to zero-export and run loads, using SOC and load/generation data from all assets.

Firmware, MQTT, and any higher-level controller should support: (a) Solis export to charge Solark, (b) Solark export to charge Solis, (c) zero export / run loads only, (d) **Enphase export or turn-on only when Solis or Solark needs charging**, and (e) coordinated SOC management across 70 kWh (Solis 1), 64 kWh (Solark), and 50 kWh (Solis 2).

---

## References

- Solis inverter Modbus/RS485: [solis-modbus-registers.md](solis-modbus-registers.md)

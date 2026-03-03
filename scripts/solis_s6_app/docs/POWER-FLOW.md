# Solis S6 Power Flow (no-battery setup)

## Important: Solark *is* the grid

In this system, **the Solark inverter is the grid** from the Solis's perspective. The Solis does not connect to the utility grid directly — it connects to the Solark. So:

- **Solis exports power → to the Solark**
- The external meter on the Solis measures the connection between Solis and Solark
- **Negative grid power** = Solis exporting to Solark ✓
- **Positive grid power** = Solis drawing from Solark (e.g. when no sun, or Solark feeding backup loads)

If the sensors do **not** show this (e.g. signs seem wrong, or values don't match expected flow), then either:
1. The external Solis meter (CT) is miswired or misconfigured (direction, placement)
2. The meter register we're reading (33130) may not match your meter setup
3. There may be another register that reflects Solis↔Solark flow — worth checking Solis Modbus docs for meter placement options

All data below is read from **Solis Modbus (unit 16)** — the app connects to the Solis inverter; Solark data is fetched separately via HTTP.

---

## Data sources (all from Solis Modbus, unit 16)

| UI label       | Register | Description |
|----------------|----------|-------------|
| **Grid (meter)** | 33130  | External Solis meter CT at the Solis↔Solark connection. **Negative = export to Solark**, positive = import from Solark. |
| **PV**           | 33057-58| Total DC power from solar. |
| **Battery**      | 33149-50| Charge/discharge (positive = charge, negative = discharge). Zero when no battery. |
| **Backup port**  | 33148  | Power to critical/backup loads outlet. |
| **House load**   | 33147  | Household load (CT on house side, if configured). May equal backup when only backup port is used. |

## Power flow (feed-in, no battery)

```
PV → Solis inverter AC out → Backup port (critical loads)
                         ↘→ Excess exports to Solark (Solark = grid)

         [Solis]                    [Solark]
    PV ──► Inverter ──► Backup port
              │
              └────────► meter CT ──► Solark (grid)
                         (33130: negative = export to Solark)
```

- **During sun**: PV feeds backup port; excess exports to Solark → grid power negative.
- **At night / no sun**: Solark feeds backup port via Solis → grid power positive.
- **Grid import/export today (kWh)**: Cumulative for the day. Import = total drawn from Solark so far; export = total sent to Solark.

## Example numbers (-1611 W grid, ~2941 W load)

- **Grid -1611 W** = Solis exporting 1611 W to Solark ✓
- **Load ~2941 W** = Backup port consumption
- **Approx balance**: PV ≈ 2941 + 1611 ≈ 4552 W (solar feeding backup + export to Solark)
- **11.5 kWh import today** = Cumulative from earlier (e.g. overnight when Solark fed loads); does not decrease when exporting.

## Separate sensor for backup port

The Solis Modbus register **33148** (Backup Load power) is the inverter’s measurement of power to the backup port. We read and display this as **Backup port** on the dashboard. Meter data (33130) comes from the same Solis Modbus connection — ensure Settings has Modbus unit **16** if that is your inverter.

If you want a **separate physical sensor** (e.g. CT on backup port wiring) for redundancy or calibration, you’d need to:
1. Add a device that reads that CT and exposes data (e.g. ESPHome, Shelly, etc.).
2. Integrate that data source into this app (new API/HTTP endpoint or MQTT topic).

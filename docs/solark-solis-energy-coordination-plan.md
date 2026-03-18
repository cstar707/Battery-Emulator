# Solark–Solis Energy Coordination Plan

## Generation Sources (Keep On as Much as Possible)

| Source        | Capacity                | Control                      |
| ------------- | ----------------------- | ---------------------------- |
| **Solark PV** | up to ~15 kW            | Built-in (not app-curtailed) |
| **Envoy 1**   | 12× M215 ≈ 2.5 kW       | HA switch (mseries)          |
| **Envoy 2**   | 14× IQ8 @ ~355 W ≈ 5 kW | HA switch (iq8)              |
| **Tabuchi**   | ≈ 3 kW                  | HA switch (tabuchi)          |
| **Solis PV**  | varies                  | Power limit 0–100%           |

**Total potential generation** ≈ 15 + 10.5 + Solis PV. Keep all curtailable sources ON whenever Solark SOC allows; curtail at SOC ≥ 98%.

## CRITICAL SAFETY: 98% Solark SOC Hard Cap

**Solark will over-voltage at 100% SOC** with nowhere to store power. Maintain a **2% safety window** — **never exceed 98% Solark SOC**.

At 98%: Disable **ALL** generation (Solis PV, Envoys, Tabuchi) and stop charge-from-PV. Restore only when SOC drops below 95%.

**Primary goal**: Maximize PV consumption when the sun is shining, especially when storage is full. Use surplus power for:

1. **Solis grid charge** — charge Solis batteries from AC bus
2. **Tesla EV charge** — Model 3 and Model S; maximize charging when surplus available
3. **Miner dump load** — turn on miners to consume power when storage full and we still have surplus

**Consumption priority** (when surplus PV): Storage (Solis) first → EV charging → Miners as dump load when storage full.

---

## All Automations (Separate Toggles in Admin)

| Automation                  | Purpose                                                                    | Settings Toggle           | Stage |
| --------------------------- | -------------------------------------------------------------------------- | ------------------------- | ----- |
| **Solark SOC curtailment**  | At SOC ≥ 98%: disable ALL (Solis, Envoys, Tabuchi). Restore at < 95%. 2% safety window. | "Solark SOC automation"   | 1     |
| **Solis charge from PV**    | When PV available + SOC safe: enable Solis grid charge to consume surplus. | "Solis charge from PV"    | 2     |
| **Solar prediction**        | Forecast.Solar API. Two toggles: Forecast API (fetches on/off), Solar prediction mode (use forecast in automations on/off). | "Forecast API", "Solar prediction" | 4     |
| **Tesla EV charge from PV** | When surplus PV: maximize Model 3 and Model S charging (rate-adjustable).  | "Tesla EV charge from PV" | 5     |
| **Miner dump load**         | When storage full + surplus: turn on miners to consume power.              | "Miner dump load from PV" | 6     |

**Key**: Each automation is a **separate option** in the admin (Settings) page. Users enable/disable independently.

---

## Capacities

- **Each Solis inverter**: up to 11.4 kW grid charge (setup-dependent); two inverters ≈ 22.8 kW total. **Solis-2** controlled same as Solis-1 when available and online.
- **Tesla EVs**: charge rate (amperage) controllable to match surplus (Stage 5 — integration TBD).
- **Manual override**: Manual control always wins; automation does not override when user has set power_control manually.

---

## Solar Prediction API (Proactive Planning)

**Forecast.Solar** (free public tier): 12 calls/60 min per IP, no API key. **Today + tomorrow only** (full week = paid). Returns watt and watt_hours; 1h resolution. One call = both days. Use 4–6 calls/day. Optional InfluxDB for forecast vs actual time series. **Panel config**: Combined kWp (Solark + Solis + Envoy + Tabuchi); all south, 45° tilt (default).

**Two separate settings**: (1) **Forecast API** — on/off for Forecast.Solar API calls; when OFF, no fetches. (2) **Solar prediction mode** — on/off for using forecast in automations; when OFF, automations ignore forecast (data can still show in Solar Forecast tab if API is on). Both default OFF.

**UI**: New tab "Solar Forecast" in nav — route `/forecast`, shows daily kWh (today + tomorrow), hourly W timeline, last fetch, rate-limit. Optional InfluxDB write for forecast vs actual time series (Grafana dashboards).

## Stage 2: Solis Charge-from-PV — Dynamic Wattage & Monitoring

**Critical**: Solis grid charge pulls whatever `watts` value we set. We must **reliably control that value** based on available surplus — not a fixed max. Otherwise we import from grid when surplus is low.

**Available power estimation** (Solark MQTT/HTTP data): Solark acts as grid source (off-grid); Solis, Tabuchi, Envoys AC-coupled to same bus. **Do not use grid_ct_power_W** (not reliable off-grid). Use **battery_total_power_W** when positive (charging) = surplus we can divert to Solis. available_power = max(0, battery_total_power_W).
- `pv_power_W`, `battery_total_power_W` — use for decision logic and sanity checks

**Formula**: `charge_watts = min(available_surplus, max_charge_per_inverter)`.

**Modbus rate limiting**: Each Solis Modbus write takes time. Do not overload: (1) **Cooldown** — min 90–120 s between writes; (2) **Deadband** — only write when new watts differs by >500 W from current. Recalculate every poll; apply only when deadband crossed and cooldown elapsed. Rely on power_control refresher (120 s) where possible.

**Per-inverter**: Solis-1 and Solis-2 each pull their configured charge rate. When both online, split surplus or apply same (verify power_control scope).

### 2026-03-18 Sunny-day test notes (what we learned)
- Solis grid charging requires the inverter’s **“Allow grid charge” gate** in **register `43110` (BIT5)** to be enabled (in practice on this system: **BIT5=1 enables**).
- Earlier behavior where changing the grid charge limit caused Solis to stop and fall back was traced to our Modbus helper writing sequence: it would clear/overwrite the `43110` gate during watt adjustments.
- The robust fix is a **two-phase write sequence**:
  - **Arm (rare)**: `arm_grid_charge()` sets the `43110` gate + required charge-limit registers, without touching the remote import power.
  - **Refresh (every cycle)**: the 120s dead-man refresher updates only remote import (`43132=2` + `43128`), and **re-arms only if** `allow_grid_charge` drops false.

## Staged Adoption

- **Stage 1**: Threshold 98% (safety cap), separate toggles in Settings
- **Stage 2**: Solis charge-from-PV — dynamic wattage from surplus; monitor Solark PV, battery, grid
- **Stage 3**: UI visibility, tuning of thresholds
- **Stage 4**: Solar prediction API (Forecast.Solar) — today + tomorrow forecast; Solar Forecast tab; optional InfluxDB for forecast vs actual
- **Stage 5**: Tesla EV charge-from-PV (Model 3, Model S)
- **Stage 6**: Miner dump load

## Stage 4 Implementation (Solar Forecast)

- **Config**: `SOLAR_FORECAST_API_ENABLED` (on/off — controls API fetches); `SOLAR_PREDICTION_ENABLED` (on/off — controls whether automations use forecast); `SOLAR_PREDICTION_LAT`, `SOLAR_PREDICTION_LON`, panel tilt/azimuth, kWp; optional `INFLUX_*`
- **Forecast.Solar**: 12 calls/60 min per IP; today + tomorrow only; 1h resolution; one call returns both days
- **Background job**: Fetch 4–6×/day; cache response
- **Routes**: `/forecast`, `/api/forecast`
- **Template**: `forecast.html` — daily kWh (today + tomorrow), hourly W timeline, last fetch, rate-limit
- **Nav**: Add "Solar Forecast" link in base.html
- **Optional InfluxDB**: `influx.py` — write forecast + actual for Grafana dashboards

---

## Resolved Questions

1. **Physical setup**: Solark acts as the **grid source** (off-grid). Solis, Tabuchi, Envoys are AC-coupled to the Solark AC bus. We control **only** Solis grid charging.
2. **Manual override**: Yes — manual control in every way possible; manual `power_control` blocks automation.
3. **Solis-2**: Yes — same as Solis-1, when Solis-2 is available and online.
4. **Solar prediction**: One combined total (Solark + Solis + Envoy + Tabuchi); all south, 45° tilt until otherwise specified.
5. **Tesla integration**: Later stage — approach not decided.
6. **Miner integration**: HA switch; entity IDs TBD.

# Solis S6 app — Solark data from MQTT (contract)

The app uses **only MQTT** for Solark data. Whatever publishes Solark data to the broker must match this contract so the dashboard and automations work.

## Required

- **`battery_soc_pptt`** — Must be present. Used by automations (Solis self-use at 98% SOC, HA curtailment) and as the gate for accepting a payload. Units: percent × 100 (e.g. 9850 = 98.5%).

## Two supported formats

### 1. JSON blob on `solar/solark`

Publish a **single JSON message** to topic `solar/solark` (or whatever `SOLARK_MQTT_TOPIC` is set to). The app only accepts it if the JSON contains `battery_soc_pptt`.

**Recommended keys** (same as dashboard/API expect; missing keys show as "—"):

| Key | Description | Example |
|-----|-------------|--------|
| `battery_soc_pptt` | SOC (percent × 100) | 9850 |
| `battery_voltage_dV` | Battery voltage (V × 10) | 520 |
| `battery_current_dA` | Battery current (A × 10); + charge, − discharge after app normalize | -30 |
| `battery_power_W` | Battery power (W) | -320 |
| `battery_total_power_W` | Total battery power (W) | -320 |
| `battery_temperature` | Battery temp (°C) | 25.5 |
| `pv_power_W` | PV power (W) | 3500 |
| `total_pv_power_W` | Total PV (W) | 3500 |
| `pv1_power_W`, `pv2_power_W` | String 1/2 (W) | 1800, 1700 |
| `grid_ct_power_W` | Grid CT power (W) | -200 |
| `inverter_power_W` | Inverter power (W) | 3200 |
| `inverter_current_dA` | Inverter current (A × 10) | 140 |
| `day_pv_energy_kWh` | Today PV (kWh) | 15.2 |
| `day_batt_charge_kWh` | Today batt charge (kWh) | 2.1 |
| `day_batt_discharge_kWh` | Today batt discharge (kWh) | 1.8 |
| `day_load_energy_kWh` | Today load (kWh) | 12.0 |
| `total_pv_energy_kWh` | Lifetime PV (kWh) | 5200 |
| `total_batt_charge_kWh` | Lifetime batt charge (kWh) | 800 |
| `total_batt_dis_kWh` | Lifetime batt discharge (kWh) | 750 |
| `total_load_energy_kWh` | Lifetime load (kWh) | 4500 |
| `total_grid_export_kWh` | Lifetime grid export (kWh) | 1200 |
| `total_grid_import_kWh` | Lifetime grid import (kWh) | 300 |

**Sign convention:** The app normalizes battery power and current so **+ = charging, − = discharging** (Sunsynk Modbus uses the opposite). So the publisher can send raw values; the app will flip sign for `battery_power_W`, `battery_total_power_W`, and `battery_current_dA`.

### 2. Individual sensors on `solar/solark/sensors/<suffix>`

Publish **one numeric value per topic**. The app maps topic suffix → internal key and scale. Cache is updated only when **`battery_soc`** has been received (mapped to `battery_soc_pptt`).

| Topic suffix | Internal key | Scale (payload × scale) |
|--------------|--------------|-------------------------|
| `battery_soc` | battery_soc_pptt | 100 (% → pptt) |
| `battery_voltage` | battery_voltage_dV | 10 |
| `battery_current` | battery_current_dA | 10 |
| `battery_power` | battery_power_W | 1 |
| `total_battery_power` | battery_total_power_W | 1 |
| `battery_temperature` | battery_temperature | 1 |
| `solar_power` | pv_power_W | 1 |
| `total_solar_power` | total_pv_power_W | 1 |
| `pv1_power`, `pv2_power` | pv1_power_W, pv2_power_W | 1 |
| `grid_ct_power` | grid_ct_power_W | 1 |
| `inverter_power` | inverter_power_W | 1 |
| `inverter_current` | inverter_current_dA | 10 |
| `day_pv_energy` | day_pv_energy_kWh | 1 |
| `day_battery_charge` | day_batt_charge_kWh | 1 |
| `day_battery_discharge` | day_batt_discharge_kWh | 1 |
| `day_load_energy` | day_load_energy_kWh | 1 |
| `total_pv_energy` | total_pv_energy_kWh | 1 |
| `total_battery_charge` | total_batt_charge_kWh | 1 |
| `total_battery_discharge` | total_batt_dis_kWh | 1 |
| `total_load_energy` | total_load_energy_kWh | 1 |
| `total_grid_export` | total_grid_export_kWh | 1 |
| `total_grid_import` | total_grid_import_kWh | 1 |

**Minimum for automations:** Publish at least `solar/solark/sensors/battery_soc` (and optionally the rest). The app will not update the cache until it has received `battery_soc`.

## Summary

- **Automations** need only **`battery_soc_pptt`** (from blob or from sensor `battery_soc`).
- **Dashboard** uses all the keys above; missing keys display as "—".
- **JSON blob** is simplest: one message to `solar/solark` with the keys above (at least `battery_soc_pptt`).

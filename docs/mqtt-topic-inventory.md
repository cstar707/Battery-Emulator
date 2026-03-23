# MQTT Topic Inventory

## Overview

Broker: 10.10.53.92:1883 (MQTT_HOST). Credentials: api/test12345.

---

## Topics by Source

### Solis Inverters (solis-s6-app)

| Topic Pattern | Publisher | Format | Key Fields |
|---------------|-----------|--------|------------|
| `solar/solis/s6-inv-1/status` | solis-s6-app | JSON | Full sensor payload |
| `solar/solis/s6-inv-1/sensors/<key>` | solis-s6-app | Scalar string | Per-sensor values |
| `solar/solis/s6-inv-2/status` | solis-s6-app | JSON | Same (when inv 2 online) |
| `solar/solis/s6-inv-2/sensors/<key>` | solis-s6-app | Scalar string | Same |

**Solis sensor keys** (from mqtt_publish.py): `grid_power_W`, `load_power_W`, `pv_power_W`, `battery_power_W`, `battery_soc_pct`, `battery_remaining_kWh`, `battery_runtime_hours`, `battery_runtime_direction`, `battery_voltage_V`, `battery_current_A`, `meter_power_W`, `grid_freq_Hz`, `inverter_temp_C`, `inverter_state`, `storage_control_raw`, `energy_today_pv_kWh`, `total_pv_energy_kWh`, `energy_today_load_kWh`, `product_model`, `active_power_W`, `pv_voltage_1_V`, `pv_current_1_A`, `pv_voltage_2_V`, `pv_current_2_A`, `ac_voltage_V`, `ac_current_A`, `battery_soh_pct`, `house_load_W`, `energy_today_bat_charge_kWh`, `energy_today_bat_discharge_kWh`, `energy_today_grid_import_kWh`, `energy_today_grid_export_kWh`.

---

### Solark (External Publisher)

| Topic Pattern | Publisher | Format | Key Fields |
|---------------|-----------|--------|------------|
| `solar/solark` | External (ESPHome, HA, Modbus) | JSON | Must include `battery_soc_pptt` |
| `solar/solark/sensors/<suffix>` | External | Scalar string | Individual sensors |

**Solark sensor suffixes** (from _SENSOR_TOPIC_MAP): `battery_soc`, `battery_voltage`, `battery_current`, `battery_power`, `slave_battery_power`, `total_battery_power`, `battery_temperature`, `solar_power`, `total_solar_power`, `pv1_power`, `pv2_power`, `load_power`, `grid_power`, `grid_ct_power`, `inverter_power`, `inverter_current`, `day_pv_energy`, `day_battery_charge`, `day_battery_discharge`, `day_load_energy`, `total_pv_energy`, `total_battery_charge`, `total_battery_discharge`, `total_load_energy`, `total_grid_export`, `total_grid_import`, `slave_battery_soc`.

---

### Envoy (solis-s6-app)

**Single source**: All Envoy data is computed on the server (from 3004) and published to MQTT. The display and other consumers should **subscribe only** — no HTTP polling to 3004/3008.

| Topic | Publisher | Format |
|-------|-----------|--------|
| `solar/envoy/status` | solis-s6-app | Full JSON: 3004 debug data + `total_production_W` + summary (`total_live`, `total_today`, `house_*`, `shed_*`, `trailer_*`). Use for single-message clients. |
| `solar/envoy/summary/total_live` | solis-s6-app | Scalar (W) |
| `solar/envoy/summary/total_today` | solis-s6-app | Scalar (kWh) |
| `solar/envoy/summary/house_today` | solis-s6-app | Scalar (kWh) |
| `solar/envoy/summary/house_live` | solis-s6-app | Scalar (W) |
| `solar/envoy/summary/shed_today` | solis-s6-app | Scalar (kWh) |
| `solar/envoy/summary/shed_live` | solis-s6-app | Scalar (W) |
| `solar/envoy/summary/trailer_today` | solis-s6-app | Scalar (kWh) |
| `solar/envoy/summary/trailer_live` | solis-s6-app | Scalar (W) |

**Mapping**: House = envoy1 (3 serials). Shed = envoy1 remaining (m-series). Trailer = all envoy2.

**Display**: Either subscribe to `solar/envoy/summary/#` (individual topics) or `solar/envoy/status` (full JSON). Remove HTTP envoy polling.

**API**: Same values available at `GET /api/envoy/summary` for debug or alternative consumers.

### Battery Emulator (ESP32)

| Topic Pattern | Publisher | Format |
|---------------|-----------|--------|
| `BE/status` | ESP32 | "online" |
| `BE/info` | ESP32 | JSON. Contactor status requires `equipment_stop_active` (true=open). Pause does not open contactors. |
| `BE/spec_data` | ESP32 | JSON |
| `BE/events` | ESP32 | JSON |

Topic prefix configurable via MQTTTOPIC (default "BE").

---

## Verification Commands

```bash
# Subscribe to all solar topics
mosquitto_sub -h 10.10.53.92 -p 1883 -u api -P test12345 -t 'solar/#' -v

# Solis only
mosquitto_sub -h 10.10.53.92 -p 1883 -u api -P test12345 -t 'solar/solis/#' -v

# Solark only
mosquitto_sub -h 10.10.53.92 -p 1883 -u api -P test12345 -t 'solar/solark' -t 'solar/solark/sensors/#' -v

# Envoy summary (single-source)
mosquitto_sub -h 10.10.53.92 -p 1883 -u api -P test12345 -t 'solar/envoy/summary/#' -v
```

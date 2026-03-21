# Envoy API Schema

## Source APIs

Two Envoy data sources exist. Both return data for 26 micro-inverters (12 Envoy 1 / M-series, 14 Envoy 2 / IQ8).

---

## 3004 `/api/envoy/debug` (Envoy Diagnostics)

**URL:** `http://localhost:3004/api/envoy/debug`  
**Response time:** ~15 seconds (polls real Envoys)

### Root Structure

```json
{
  "timestamp": "2026-03-20T21:40:40.505926",
  "system_status": "debug_mode",
  "envoy1": { ... },
  "envoy2": { ... }
}
```

### Per-Envoy Object

| Field | Type | Description |
|-------|------|-------------|
| `production` | number | Aggregate production (W) |
| `name` | string | e.g. "Envoy #1 - SR245-1" |
| `active_inverters` | number | Count of producing inverters |
| `offline_inverters` | number | Count of offline inverters |
| `health` | number | 0.0–1.0 |
| `last_update` | string | ISO timestamp |
| `connection_status` | string | e.g. "connected" |
| `inverters` | array | Per-inverter objects |

### Per-Inverter Object (3004)

| Field | Type | Description |
|-------|------|-------------|
| `serialNumber` | string | Enphase serial |
| `lastReportWatts` | number | Last reported power (W) |
| `maxReportWatts` | number | Max reported power (W) |
| `lastReportDate` | number | Unix timestamp |
| `devType` | number | Device type |

---

## 3008 `/api/envoy/data` (battery-dashboard)

**URL:** `http://127.0.0.1:3008/api/envoy/data`  
**Response time:** ~300ms (cached)

### Root Structure

```json
{
  "ok": true,
  "inverters": [ ... ]
}
```

### Per-Inverter Object (3008)

| Field | Type | Description |
|-------|------|-------------|
| `serial` | string | Enphase serial |
| `envoy_id` | string | "envoy1" or "envoy2" |
| `watts` | number | Current power (W) |
| `max_watts` | number | Max power (W) |
| `daily_kwh` | number | Today's production (kWh) |
| `yesterday_kwh` | number | Yesterday's production (kWh) |
| `last_time` | string | ISO timestamp |

---

## Mapping

| Envoy | System | Host | Units |
|-------|--------|------|-------|
| envoy1 | mseries | 10.10.53.194 | 12 |
| envoy2 | iq8 | 10.10.53.186 | 14 |

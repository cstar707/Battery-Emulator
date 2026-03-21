# InfluxDB Server Setup

## Overview

This document captures the solar monitoring infrastructure on the dashboard server (10.10.53.92) as discovered during the InfluxDB pipeline implementation.

---

## Services (Live Discovery)

| Service | Host:Port | Status | Notes |
|---------|-----------|--------|-------|
| **InfluxDB** | 10.10.53.92:8086 | Running | v2.8.0, health=pass. Token required for API. |
| **MQTT (Mosquitto)** | 10.10.53.92:1883 | Open | Credentials api/test12345 per config |
| **Telegraf** | 10.10.53.92:9273 | Not detected | Port closed; no MQTT→Influx bridge running |
| **solis-s6-app** | 10.10.53.92:3007 | 200 OK | Controls, settings, curtailment |
| **battery-dashboard** | 10.10.53.92:3008 | 200 OK | `/api/sensors/latest`, `/api/envoy/data`, `/api/battery-sources` |
| **Envoy Diagnostics** | 10.10.53.92:3004 | 200 OK | `/api/envoy/debug` (~15s response) |
| **Grafana** | 10.10.53.92:3000 | Running | Visualization layer |
| **Legacy Dashboard** | 10.10.53.92:3001 | 200 OK | |
| **SolarK Support** | 10.10.53.92:3002 | 200 OK | |
| **Port 80** | 10.10.53.92:80 | 200 OK | Current entry |

---

## MQTT Broker

- **Host:** 10.10.53.92 (or MQTT_HOST from settings)
- **Port:** 1883
- **Credentials:** MQTT_USER=api, MQTT_PASSWORD=test12345 (from config)
- **Client ID:** solis-s6-ui (configurable via MQTT_CLIENT_ID)

---

## InfluxDB

- **URL:** http://10.10.53.92:8086 (or localhost:8086 when on server)
- **Health:** `GET /health` returns `{"status":"pass","version":"v2.8.0"}`
- **Auth:** API token required for reads/writes. Create token in InfluxDB UI or CLI.
- **Bucket:** Configure INFLUX_BUCKET for the pipeline (e.g. `solar` or `monitoring`).

---

## Telegraf

- **Status:** Not running (port 9273 closed)
- **Alternative:** Use custom Python MQTT→Influx bridge (see `scripts/influx_mqtt_bridge/`).

---

## Key Endpoints

| Endpoint | Purpose |
|----------|---------|
| `http://10.10.53.92:3008/api/sensors/latest` | Aggregated battery/Solark sensor values |
| `http://10.10.53.92:3008/api/envoy/data` | Normalized Envoy inverter data (26 inverters) |
| `http://10.10.53.92:3004/api/envoy/debug` | Raw Enphase Envoy API response |

---

## Runbook

### Start MQTT→Influx bridge

```bash
cd scripts/influx_mqtt_bridge
pip install -r requirements.txt
export INFLUX_URL=http://localhost:8086
export INFLUX_TOKEN=<your-token>
export INFLUX_BUCKET=solar
export INFLUX_ORG=<org-if-required>
python influx_mqtt_bridge.py
```

Run as systemd service or in screen/tmux for production.

### Verify pipeline

```bash
python scripts/influx_mqtt_bridge/verify_influx.py
# Or: curl http://localhost:3007/api/influx-health
```

### Debug missing data

1. Check MQTT: `mosquitto_sub -h 10.10.53.92 -t 'solar/#' -v`
2. Check bridge logs for Influx write errors
3. Verify Envoy publisher: solis-s6-app logs "Envoy MQTT publish" on failure
4. Grafana: ensure InfluxDB data source URL and token are correct

---

## Grafana Setup

### Add InfluxDB data source

1. Open Grafana: http://10.10.53.92:3000
2. **Connections → Data sources → Add data source**
3. Select **InfluxDB**
4. Configure:
   - **Query Language:** Flux
   - **URL:** `http://localhost:8086` (or `http://10.10.53.92:8086` if Grafana is remote)
   - **Organization:** (from InfluxDB; e.g. your org name or ID)
   - **Token:** InfluxDB API token (same as INFLUX_TOKEN for the bridge)
5. **Save & test**

### Import Solar Overview dashboard

1. **Dashboards → Import**
2. **Upload JSON file** and select `grafana/dashboards/solar-overview.json`
3. Select the InfluxDB data source and **Import**

The dashboard shows Solis PV power, Solark PV power, Envoy production (IQ8 + M-series), and battery SOC.

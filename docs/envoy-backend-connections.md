# Envoy Backend Connections Reference

The **Envoy Debug Dashboard** at `http://10.10.53.92:3004` (or 3003) displays data from two Enphase Envoy gateways. This document describes the data flow and connection points for troubleshooting.

## Data Flow

```
┌─────────────────┐     HTTPS + JWT      ┌──────────────────┐
│  Envoy #1       │◄─────────────────────┤                  │
│  10.10.53.194   │                      │  Envoy API       │
└─────────────────┘                      │  (envoy_api.py)  │
                                         │  Port 3003/3004  │
┌─────────────────┐     HTTPS + JWT      │  10.10.53.92     │
│  Envoy #2       │◄─────────────────────┤                  │
│  10.10.53.186   │                      └────────┬─────────┘
└─────────────────┘                               │
                                                  │ MQTT publish
                                                  ▼
                                         ┌──────────────────┐
                                         │  Mosquitto       │
                                         │  10.10.53.92:1883│
                                         │  Topics:         │
                                         │  solar/envoy     │
                                         │  solar/envoy1    │
                                         │  solar/envoy2    │
                                         └──────────────────┘
```

## Connection Points

### 1. Envoy Physical Devices

| Envoy | Host | Serial | Endpoints |
|-------|------|--------|-----------|
| **Envoy #1** (SR245-1) | `10.10.53.194` | 122124017034 | `https://10.10.53.194/production.json`, `https://10.10.53.194/api/v1/production/inverters` |
| **Envoy #2** (SR245-2) | `10.10.53.186` | 202245123231 | Same paths |

- **Protocol:** HTTPS (port 443)
- **Auth:** JWT token in `Authorization: Bearer <token>` header
- **Token source:** Enphase Enlighten OAuth or manual JWT from installer portal

### 2. Envoy API Server (Backend)

- **Code:** `solar-monitoring/servers/envoy-api/envoy_api.py`
- **Port:** 3003 (code default) or 3004 (deployment may override)
- **Key endpoints:**
  - `GET /` – Dashboard UI
  - `GET /api/envoy/debug` – Combined Envoy data (used by dashboard and InfluxDB collector)
  - `GET /api/envoy/test/{envoy_id}/{endpoint}` – Test single Envoy endpoint
  - `POST /api/envoy/update-token/{envoy_id}` – Update JWT token

### 3. MQTT Broker

- **Host:** 10.10.53.92
- **Port:** 1883
- **Topics:** `solar/envoy`, `solar/envoy1`, `solar/envoy2`
- **Auth:** api / test12345 (configurable via env)

## Battery Dashboard Envoys Page (port 3008)

The **battery-dashboard** at `http://10.10.53.92:3008/envoys.html` shows Envoy inverter data. It fetches from:

1. **MQTT** – topics `solar/envoy1`, `solar/envoy2` (from Envoy API publisher)
2. **HTTP fallback** – `ENVOY_API_URL` → `http://localhost:3003/api/envoy/debug`

Ensure `ENVOY_API_URL=http://127.0.0.1:3003` (or `3004` if deployed differently) is set in `solar-monitoring/battery-dashboard/.env`. Without it, the HTTP fallback uses the wrong default (3002) and returns no data.

## Common Failure Modes

1. **JWT token expired** – Enphase tokens expire (often ~12 hours). Use dashboard **OAuth Refresh** or **Direct Refresh**.
2. **Envoy unreachable** – Network/firewall; verify `10.10.53.194` and `10.10.53.186` are reachable from 10.10.53.92.
3. **Wrong port** – Dashboard may be on 3003 or 3004; check systemd/docker.
4. **MQTT disconnected** – Envoy API publishes to MQTT; broker must be running.
5. **Envoys page shows no data** – Set `ENVOY_API_URL` in battery-dashboard `.env` (see above).

## Diagnostic Script

From `solar-monitoring`:

```bash
python scripts/check-envoy-connections.py
# Or if dashboard is on 3004:
ENVOY_API_BASE=http://10.10.53.92:3004 python scripts/check-envoy-connections.py
```

Run from a machine that can reach the 10.10.53.x network (e.g. the solar server or same LAN).

## Related Docs

- `solar-monitoring/SOLAR_MONITORING_STATUS_SAVE.md` – Service ports and status
- `docs/mqtt-solar-server-reference.md` – MQTT topic layout

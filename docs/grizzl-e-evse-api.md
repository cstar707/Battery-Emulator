# Grizzl-E EVSE Charger API Reference

**Model:** GRC 48A CLUB 2025  
**Serial:** GRC-A0000397214  
**Firmware Main:** GCS077A-01.07.1  
**Firmware WiFi:** USGCW070A-03.15.2  
**IP (local):** 10.10.53.149  
**MAC:** 80:F3:DA:BD:03:D4  
**AP SSID:** GRC-A0000397214  

## Open Ports

| Port | Protocol | Service |
|------|----------|---------|
| 80   | TCP/HTTP | Web UI + REST API |

## API Overview

All endpoints use **HTTP POST**. No authentication is configured by default (`httpUsername` / `httpPassword` are empty). Responses are JSON unless otherwise noted.

---

## Endpoints

### `POST /init`

Returns device configuration. Called once on page load.

```bash
curl -X POST http://10.10.53.149/init
```

**Response:**

```json
{
  "curDesign": 48,
  "minCurrent": 7,
  "ssidNameAP": "GRC-A0000397214",
  "ssidPasswordAP": "",
  "ssidName": "SR245",
  "ssidPassword": "",
  "httpUsername": "",
  "httpPassword": "",
  "WifiMode": 3,
  "broadcastMode": 1,
  "ESP_MAC": "80:F3:DA:BD:03:D4",
  "mac_bind": 0,
  "STA_MAC": "",
  "lang": 1,
  "thirdPartyBackends": 0
}
```

---

### `POST /main`

Returns live telemetry. The web UI polls this every 1 second.

```bash
curl -X POST http://10.10.53.149/main
```

**Response:**

```json
{
  "verFWMain": "GCS077A-01.07.1 ",
  "verFWWifi": "USGCW070A-03.15.2",
  "serialNum": "GRC-A0000397214",
  "stationId": "GRC-A0000397214",
  "model": "GRC 48A CLUB 2025",
  "broadcastMode": 1,
  "switchState": 14,
  "pilot": 1,
  "state": 3,
  "subState": 0,
  "currentSet": 48,
  "curDesign": 48,
  "minCurrent": 7,
  "minVoltage": 175,
  "gridRange": 0,
  "typeEvse": 1,
  "typeRelay": 0,
  "ground": 1,
  "groundCtrl": 1,
  "voltMeas1": 243,
  "voltMeas2": 0,
  "voltMeas3": 0,
  "curMeas1": 0,
  "curMeas2": 0,
  "curMeas3": 0,
  "powerMeas": 0,
  "temperature1": 3,
  "temperature2": 3,
  "leakValue": 17,
  "leakValueH": 13,
  "vBat": 3.03,
  "totalEnergy": 48.82,
  "sessionTime": 20421,
  "sessionEnergy": 0,
  "sessionMoney": 0,
  "sessionStarted": 1,
  "IEM1": 48.82,
  "IEM2": 48.82,
  "IEM1_money": 48.82,
  "IEM2_money": 48.82,
  "tarif": 100,
  "activeTarif": 0,
  "evseEnabled": 0,
  "timeLimit": 0,
  "energyLimit": 0,
  "moneyLimit": 0,
  "aiModecurrent": 7,
  "aiStatus": 0,
  "aiVoltage": 200,
  "ocppconnected": 1,
  "ocppEnabled": 1,
  "ocppVendor": 1,
  "ocppOfflineAva": 10,
  "STA_IP_Addres": "10.10.53.149",
  "RSSI": -48,
  "adapter": 255,
  "lang": 1,
  "systemTime": 1773192425
}
```

**Key fields:**

| Field | Description |
|-------|-------------|
| `state` | EVSE state (3 = idle/standby) |
| `pilot` | CP pilot signal state (1 = no vehicle) |
| `currentSet` | Active charge current limit (A) |
| `curDesign` | Hardware max current (A) |
| `voltMeas1/2/3` | Line voltage per phase (V) |
| `curMeas1/2/3` | Line current per phase (A) |
| `powerMeas` | Active power (W) |
| `temperature1/2` | Internal temps (C) |
| `totalEnergy` | Lifetime energy metered (kWh) |
| `sessionEnergy` | Current session energy (kWh) |
| `sessionTime` | Current session duration (seconds) |
| `evseEnabled` | Charging enabled flag (0/1) |
| `leakValue` | Ground fault / leakage current (mA) |
| `vBat` | Backup battery voltage (V) |
| `RSSI` | WiFi signal strength (dBm) |
| `ocppconnected` | OCPP backend connected (0/1) |
| `ground` | Ground fault detected (0/1) |

---

### `POST /pageEvent`

Send control commands. Uses `application/x-www-form-urlencoded` body with `name=value` format.

```bash
curl -X POST http://10.10.53.149/pageEvent \
  -H "Content-type: application/x-www-form-urlencoded" \
  -H "pageEvent: currentSet" \
  -d "currentSet=32"
```

**Controllable parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `currentSet` | int (7-48) | Charge current limit in amps |
| `evseEnabled` | 0/1 | Enable / disable charging |
| `aiMode` | int | Adaptive mode (0=off, 1=voltage, 2=auto, 3=power) |
| `aiVoltage` | int | Adaptive voltage threshold (V) |
| `broadcastMode` | 0/1 | WiFi mode (0=AP, 1=STA) |
| `timeLimit` | int | Session time limit (seconds, 0=off) |
| `energyLimit` | int | Session energy limit (kWh, 0=off) |
| `moneyLimit` | int | Session cost limit (0=off) |
| `oneCharge` | 0/1 | Single-charge mode |
| `sh1Enabled` | 0/1 | Schedule 1 enable |
| `sh1Start` | int | Schedule 1 start (minutes from midnight) |
| `sh1Stop` | int | Schedule 1 stop (minutes from midnight) |
| `sh1CurrentEnable` | 0/1 | Schedule 1 current override enable |
| `sh1CurrentValue` | int | Schedule 1 current limit (A) |
| `sh1EnergyEnable` | 0/1 | Schedule 1 energy limit enable |
| `sh1EnergyValue` | int | Schedule 1 energy limit (kWh) |
| `sh2Enabled` | 0/1 | Schedule 2 enable |
| `sh2Start` | int | Schedule 2 start |
| `sh2Stop` | int | Schedule 2 stop |
| `sh2CurrentEnable` | 0/1 | Schedule 2 current override enable |
| `sh2CurrentValue` | int | Schedule 2 current limit (A) |
| `sh2EnergyEnable` | 0/1 | Schedule 2 energy limit enable |
| `sh2EnergyValue` | int | Schedule 2 energy limit (kWh) |

**Multiple parameters in one request:**

```bash
curl -X POST http://10.10.53.149/pageEvent \
  -H "Content-type: application/x-www-form-urlencoded" \
  -d "currentSet=32&evseEnabled=1"
```

---

### `POST /config`

Save WiFi station (STA) settings. Form-encoded body with `ssidName` and `ssidPassword`.

---

### `POST /configAP`

Save WiFi access point (AP) settings. Form-encoded body with `ssidNameAP`, `ssidPasswordAP`, `ssidPasswordAPConf`.

---

### `POST /configHttp`

Save HTTP authentication credentials. Form-encoded body with `httpUsername`, `httpPassword`, `httpPasswordConf`.

---

### `POST /scan`

Trigger a WiFi network scan. Returns immediately; results fetched via `/scanResult`.

---

### `POST /scanResult`

Returns WiFi scan results (text, not JSON). Returns "Scanning..." if scan is still in progress.

---

### `POST /get_logResult`

Retrieve device logs (text).

---

### `POST /ocppEvent`

Send OCPP configuration changes. Same `name=value` format as `/pageEvent`.

---

### `POST /update`

Firmware update endpoint.

---

## EVSE States

Based on the `state` field from `/main`:

| Value | Meaning |
|-------|---------|
| 3 | Standby / idle |
| 12 | (default/init) |

Based on the `pilot` field:

| Value | Meaning |
|-------|---------|
| 1 | No vehicle connected (State A) |
| 2 | Vehicle connected, not charging (State B) |
| 3 | Charging (State C) |

## Usage Examples

### Poll live data

```bash
while true; do
  curl -s -X POST http://10.10.53.149/main | jq '{v: .voltMeas1, a: .curMeas1, w: .powerMeas, kWh: .sessionEnergy, temp: .temperature1}'
  sleep 5
done
```

### Set charge current to 24A

```bash
curl -X POST http://10.10.53.149/pageEvent \
  -H "Content-type: application/x-www-form-urlencoded" \
  -H "pageEvent: currentSet" \
  -d "currentSet=24"
```

### Enable charging

```bash
curl -X POST http://10.10.53.149/pageEvent \
  -H "Content-type: application/x-www-form-urlencoded" \
  -H "pageEvent: evseEnabled" \
  -d "evseEnabled=1"
```

### Disable charging

```bash
curl -X POST http://10.10.53.149/pageEvent \
  -H "Content-type: application/x-www-form-urlencoded" \
  -H "pageEvent: evseEnabled" \
  -d "evseEnabled=0"
```

## Notes

- The web UI polls `/main` every 1 second via XHR.
- `/init` is called once on page load to populate config fields.
- No authentication is required by default. Set credentials via the web UI or `/configHttp`.
- OCPP is enabled and connected to a backend (vendor ID 1).
- The charger supports adaptive current limiting based on voltage drop, power, or automatic mode.
- Single-phase only based on observed data (voltMeas2/3 = 0, curMeas2/3 = 0), but firmware supports 3-phase metering.

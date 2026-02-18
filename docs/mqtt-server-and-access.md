# MQTT server – configuration and access

The Battery-Emulator board (including when replacing 10.10.53.32) uses **one MQTT connection** to publish both **Battery Emulator** data (battery state, cells, status) and **Solark** data (inverter monitoring, and later switches). There is a **single configuration section** for the MQTT server; that setup is used for everything.

---

## 1. MQTT configuration section (used for both Battery Emulator and Solark)

**One broker, one set of settings.** Configure the MQTT server (broker) in the board’s **web UI → Settings → MQTT**. The same broker and credentials are used for:

- Battery Emulator: `BE/status`, common info, cell voltages, balancing, events, buttons, HA discovery for battery.
- Solark: **solar/solark** (same topic as today; when implemented, slave could be e.g. solar/solark_slave), HA discovery for Solark sensors/switches.
- **ESP board health:** `BE/board` – free heap, min free heap, heap size, max alloc heap, CPU temperature (°C), uptime (ms), and (when performance measurement is enabled) core/MQTT/WiFi task max times (µs) so you can monitor for overload.

You do **not** configure MQTT separately for battery vs Solark. Enable MQTT and set the broker once; all traffic goes there.

### Settings → MQTT (all fields)

| Setting | NVM key | Meaning | Typical value |
|--------|---------|---------|----------------|
| **Enable MQTT** | MQTTENABLED | Turn MQTT on/off for the whole board | Check to enable |
| **MQTT server** | MQTTSERVER | Broker hostname or IP | e.g. `192.168.1.10`, `homeassistant.local`, or your HA IP |
| **MQTT port** | MQTTPORT | Broker port | **1883** (default in firmware if unset) |
| **MQTT user** | MQTTUSER | Optional username | If your broker requires auth |
| **MQTT password** | MQTTPASSWORD | Optional password | If your broker requires auth |
| **MQTT timeout ms** | MQTTTIMEOUT | Connection timeout | 2000 |
| **MQTT publish interval (seconds)** | MQTTPUBLISHMS | How often to publish (battery + Solark) | 5 |
| **Send all cellvoltages via MQTT** | MQTTCELLV | Include full cell data in publish | Optional |
| **Enable Home Assistant auto discovery** | HADISC | Publish HA discovery for battery and Solark | Check if using HA |
| **Customized MQTT topics** | MQTTTOPICS | Use custom topic name, object ID, device name | Check when replacing 10.10.53.32 (sunsynk) |
| **MQTT topic name** | MQTTTOPIC | Prefix for all topics (e.g. `BE`) | `BE` |
| **Prefix for MQTT object ID** | MQTTOBJIDPREFIX | e.g. `sunsynk_` for Solark-style entities | `be_` or `sunsynk_` |
| **HA device name** | MQTTDEVICENAME | Device name in HA (e.g. sunsynk) | `Battery Emulator` or `sunsynk` |
| **HA device ID** | HADEVICEID | Stable device ID in HA | `battery-emulator` or `sunsynk` |

**How to open Settings:**

1. In a browser, open the board’s web UI:
   - By IP: **http://\<board-ip\>** (e.g. `http://192.168.1.42`)
   - By hostname (replacement for 10.10.53.32): **http://esphome-web-7a7e60.local/**
2. Log in if web auth is enabled.
3. Open **Settings**, scroll to **MQTT**, set **MQTT server** and **MQTT port** (1883), optionally user/password, check **Enable MQTT**, then save. For 10.10.53.32 replacement, also enable **Customized MQTT topics** and set topic/device name as in `docs/solark-rs485-integration.md`.

**Where it’s stored:** NVM keys above; loaded in `Software/src/communication/nvm/comm_nvm.cpp`, used in `Software/src/devboard/mqtt/mqtt.cpp` for the single MQTT client connection.

**Using the same broker as a solar monitoring server:** If your MQTT broker is the one used by a solar dashboard/unified API (e.g. Envoy + Solark), configure the board with that broker’s host, port 1883, and the same username/password (stored only on the device and on the server, never in this repo). Topic layout and integration notes: **`docs/mqtt-solar-server-reference.md`**.

---

## 2. How to access the MQTT server (broker)

**“Access” can mean two things:**

### A) You want the board to connect to the broker

- Configure the board as above (Settings → MQTT server, port, enable).  
- The board will connect to **that** broker. So the “MQTT server” you care about is **the broker you entered** (e.g. your Home Assistant MQTT broker, or a standalone Mosquitto instance).  
- That broker can be:
  - **On your Home Assistant server** (same machine as HA; port 1883 if you use the default HA MQTT add-on).
  - **On another machine** (e.g. Raspberry Pi running Mosquitto).  
- The board does **not** run a broker; it only connects to one.

### B) You want to read the data the board publishes

- Use **the same broker** the board is configured to use.  
- **From Home Assistant:** Add the MQTT integration pointing at that broker; the board publishes to topics like `solar/solark`, `BE/status`, etc. With **Home Assistant discovery** enabled on the board, HA will create entities automatically.  
- **From command line (e.g. mosquitto_sub):**
  ```bash
  mosquitto_sub -h <broker-ip> -p 1883 -u "<user>" -P "<pass>" -t "BE/#" -v
  ```
  Replace `<broker-ip>` with the same host you set as “MQTT server” on the board (e.g. your HA IP).  
- **Solark data:** Topic **solar/solark** (same as today). Payload is JSON: `battery_power_W`, `battery_soc_pptt`, `grid_power_W`, `load_power_W`, `pv_power_W`, `raw_registers`, etc.

---

## 3. Summary

| Question | Answer |
|----------|--------|
| **Where is the MQTT server?** | The MQTT **broker** is a host you set in the board’s **Settings → MQTT server** (e.g. your HA host or a dedicated broker). The board does not host a broker. |
| **How do I set it?** | Web UI → **Settings** → **MQTT**. One configuration is used for both Battery Emulator and Solark; set **MQTT server** (hostname/IP), **MQTT port** (1883), optionally user/password, enable MQTT, save. |
| **How do I open the board’s web UI?** | **http://\<board-ip\>** or **http://esphome-web-7a7e60.local/** (replacement device with default hostname). |
| **How do I access the data?** | Use the **same broker** the board uses: subscribe to `BE/#` (or your topic) from HA, `mosquitto_sub`, or any MQTT client. Solark data is on **solar/solark**. |

If you use **Home Assistant**, point the board at your HA MQTT broker (same host as HA, port 1883 unless you changed it). Then HA can subscribe to `solar/solark` and discovery topics automatically.

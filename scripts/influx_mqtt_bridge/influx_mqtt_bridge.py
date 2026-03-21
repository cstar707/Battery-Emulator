#!/usr/bin/env python3
"""
MQTT to InfluxDB bridge for solar monitoring data.

Subscribes to: solar/solis/#, solar/solark, solar/solark/sensors/#, solar/envoy/status, BE/#
Writes to InfluxDB v2 with measurements: solis, solark, envoy_aggregate, envoy_inverter, battery_emulator.

Env: INFLUX_URL, INFLUX_TOKEN, INFLUX_BUCKET, INFLUX_ORG, MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASSWORD
"""
from __future__ import annotations

import json
import logging
import os
import re
import sys
import time
from datetime import datetime, timezone

import paho.mqtt.client as mqtt

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
logger = logging.getLogger(__name__)

INFLUX_URL = os.environ.get("INFLUX_URL", "http://localhost:8086")
INFLUX_TOKEN = os.environ.get("INFLUX_TOKEN", "")
INFLUX_BUCKET = os.environ.get("INFLUX_BUCKET", "solar")
INFLUX_ORG = os.environ.get("INFLUX_ORG", "")
MQTT_HOST = os.environ.get("MQTT_HOST", "10.10.53.92")
MQTT_PORT = int(os.environ.get("MQTT_PORT", "1883"))
MQTT_USER = os.environ.get("MQTT_USER", "api")
MQTT_PASSWORD = os.environ.get("MQTT_PASSWORD", "test12345")

# Solis: solar/solis/s6-inv-1/status, solar/solis/s6-inv-1/sensors/<key>
SOLIS_STATUS_RE = re.compile(r"^solar/solis/([^/]+)/status$")
SOLIS_SENSOR_RE = re.compile(r"^solar/solis/([^/]+)/sensors/([^/]+)$")

# Envoy aggregate mapping
ENVOY_SYSTEM_MAP = {"envoy1": "mseries", "envoy2": "iq8"}


def get_influx_client():
    try:
        from influxdb_client import InfluxDBClient, Point
        from influxdb_client.client.write_api import SYNCHRONOUS
    except ImportError:
        logger.error("influxdb_client not installed. pip install influxdb-client")
        sys.exit(1)
    if not INFLUX_TOKEN:
        logger.error("INFLUX_TOKEN required")
        sys.exit(1)
    client = InfluxDBClient(url=INFLUX_URL, token=INFLUX_TOKEN, org=INFLUX_ORG)
    return client, client.write_api(write_options=SYNCHRONOUS), Point


_influx_client = None
_write_api = None
_Point = None


def _ensure_influx():
    global _influx_client, _write_api, _Point
    if _influx_client is None:
        _influx_client, _write_api, _Point = get_influx_client()
    return _influx_client, _write_api, _Point


def _write_point(measurement: str, tags: dict, fields: dict):
    _, write_api, Point = _ensure_influx()
    p = Point(measurement).time(datetime.now(timezone.utc))
    for k, v in tags.items():
        p = p.tag(k, str(v))
    for k, v in fields.items():
        if v is not None and isinstance(v, (int, float, str, bool)):
            p = p.field(k, v)
    try:
        write_api.write(bucket=INFLUX_BUCKET, record=p)
    except Exception as e:
        logger.warning("Influx write failed: %s", e)


def _on_solis_status(topic: str, payload: str):
    m = SOLIS_STATUS_RE.match(topic)
    if not m:
        return
    inverter = m.group(1)
    try:
        data = json.loads(payload)
    except json.JSONDecodeError:
        return
    fields = {}
    for k in ("pv_power_W", "battery_power_W", "grid_power_W", "load_power_W", "battery_soc_pct",
              "energy_today_pv_kWh", "inverter_temp_C"):
        if k in data and isinstance(data[k], (int, float)):
            fields[k] = data[k]
    if fields:
        _write_point("solis", {"inverter": inverter}, fields)


def _on_solis_sensor(topic: str, payload: str):
    m = SOLIS_SENSOR_RE.match(topic)
    if not m:
        return
    inverter, key = m.group(1), m.group(2)
    try:
        val = float(payload.strip())
    except ValueError:
        return
    field_map = {k: k for k in ("pv_power_W", "battery_power_W", "grid_power_W", "load_power_W",
                                 "battery_soc_pct", "energy_today_pv_kWh")}
    if key in field_map:
        _write_point("solis", {"inverter": inverter}, {key: val})


def _on_solark(topic: str, payload: str):
    if topic != "solar/solark":
        return
    try:
        data = json.loads(payload)
    except json.JSONDecodeError:
        return
    fields = {}
    for k in ("battery_soc_pptt", "pv_power_W", "battery_power_W", "load_power_W", "grid_power_W",
              "day_pv_energy_kWh"):
        v = data.get(k)
        if v is not None and isinstance(v, (int, float)):
            fields[k] = v
    if data.get("battery_soc_pptt") is not None:
        fields["battery_soc_pct"] = data["battery_soc_pptt"] / 100.0
    if fields:
        _write_point("solark", {}, fields)


def _on_solark_sensor(topic: str, payload: str):
    if not topic.startswith("solar/solark/sensors/"):
        return
    suffix = topic.split("/sensors/", 1)[1]
    try:
        val = float(payload.strip())
    except ValueError:
        return
    key_map = {
        "battery_soc": "battery_soc_pct", "solar_power": "pv_power_W", "battery_power": "battery_power_W",
        "load_power": "load_power_W", "grid_power": "grid_power_W", "day_pv_energy": "day_pv_energy_kWh",
    }
    if suffix in key_map:
        _write_point("solark", {}, {key_map[suffix]: val})


def _on_envoy(topic: str, payload: str):
    if topic != "solar/envoy/status":
        return
    try:
        data = json.loads(payload)
    except json.JSONDecodeError:
        return
    for eid, edata in data.items():
        if not (eid.startswith("envoy") and isinstance(edata, dict)):
            continue
        system = ENVOY_SYSTEM_MAP.get(eid, eid)
        invs = edata.get("inverters") or []
        _write_point("envoy_aggregate", {"envoy_id": eid, "system": system}, {
            "production_W": edata.get("production"),
            "active_inverters": edata.get("active_inverters"),
            "offline_inverters": edata.get("offline_inverters"),
            "inverter_count": len(invs),
        })
        for inv in invs:
            sn = inv.get("serialNumber") or inv.get("serial")
            if not sn:
                continue
            watts = inv.get("lastReportWatts") if "lastReportWatts" in inv else inv.get("watts")
            daily_kwh = inv.get("daily_kwh")
            fields = {"production_W": watts, "max_report_watts": inv.get("maxReportWatts") or inv.get("max_watts")}
            if daily_kwh is not None:
                fields["daily_kWh"] = daily_kwh
            _write_point("envoy_inverter", {"envoy_id": eid, "system": system, "serial": str(sn)}, fields)


def _on_be(topic: str, payload: str):
    if topic == "BE/status":
        return
    if topic == "BE/info":
        try:
            data = json.loads(payload)
        except json.JSONDecodeError:
            return
        fields = {}
        for k in ("SOC", "stat_batt_power", "battery_voltage", "battery_current"):
            v = data.get(k)
            if v is not None and isinstance(v, (int, float)):
                fields[k] = v
        if data.get("SOC") is not None:
            fields["soc_pct"] = data["SOC"]
        if fields:
            _write_point("battery_emulator", {}, fields)


def _on_message(_client, _userdata, msg):
    topic = msg.topic
    payload = msg.payload.decode("utf-8", errors="ignore")
    try:
        if SOLIS_STATUS_RE.match(topic):
            _on_solis_status(topic, payload)
        elif SOLIS_SENSOR_RE.match(topic):
            _on_solis_sensor(topic, payload)
        elif topic == "solar/solark":
            _on_solark(topic, payload)
        elif topic.startswith("solar/solark/sensors/"):
            _on_solark_sensor(topic, payload)
        elif topic == "solar/envoy/status":
            _on_envoy(topic, payload)
        elif topic.startswith("BE/"):
            _on_be(topic, payload)
    except Exception as e:
        logger.warning("Message handler error [%s]: %s", topic, e)


def main():
    logger.info("Influx MQTT bridge starting (Influx=%s bucket=%s)", INFLUX_URL, INFLUX_BUCKET)
    client = mqtt.Client(client_id="influx-mqtt-bridge", protocol=mqtt.MQTTv311)
    if MQTT_USER:
        client.username_pw_set(MQTT_USER, MQTT_PASSWORD or "")
    client.on_message = _on_message
    client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
    topics = [
        ("solar/solis/#", 0),
        ("solar/solark", 0),
        ("solar/solark/sensors/#", 0),
        ("solar/envoy/status", 0),
        ("BE/#", 0),
    ]
    client.subscribe(topics)
    logger.info("Subscribed to %s", [t[0] for t in topics])
    client.loop_forever()


if __name__ == "__main__":
    main()

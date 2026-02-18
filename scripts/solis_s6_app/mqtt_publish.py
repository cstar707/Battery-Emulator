"""
Publish Solis sensor data to MQTT broker.
- One JSON payload on <prefix>/status with all sensors (and storage_bits/hybrid_bits).
- Individual topics <prefix>/sensors/<name> for each scalar value (for HA or scripts).
"""
from __future__ import annotations

import json
import logging
from typing import Any

import paho.mqtt.client as mqtt

from config import (
    MQTT_CLIENT_ID,
    MQTT_HOST,
    MQTT_PASSWORD,
    MQTT_PORT,
    MQTT_TOPIC_PREFIX,
    MQTT_USER,
)

logger = logging.getLogger(__name__)

# Scalar sensor keys to publish as individual topics (exclude nested dicts/lists)
_SENSOR_KEYS = [
    "ok",
    "grid_power_W",
    "load_power_W",
    "pv_power_W",
    "battery_power_W",
    "battery_soc_pct",
    "battery_voltage_V",
    "battery_current_A",
    "meter_power_W",
    "grid_freq_Hz",
    "inverter_temp_C",
    "inverter_state",
    "storage_control_raw",
    "energy_today_pv_kWh",
    "energy_today_load_kWh",
    "product_model",
    "active_power_W",
    "pv_voltage_1_V",
    "pv_current_1_A",
    "pv_voltage_2_V",
    "pv_current_2_A",
    "ac_voltage_V",
    "ac_current_A",
    "battery_soh_pct",
    "house_load_W",
    "energy_today_bat_charge_kWh",
    "energy_today_bat_discharge_kWh",
    "energy_today_grid_import_kWh",
    "energy_today_grid_export_kWh",
]


def _serializable_payload(data: dict[str, Any]) -> dict[str, Any]:
    """Build JSON-serializable dict; exclude raw_blocks (large), include storage_bits/hybrid_bits."""
    out: dict[str, Any] = {}
    for k, v in data.items():
        if k == "raw_blocks":
            continue
        if isinstance(v, (dict, list)):
            try:
                json.dumps(v)
            except (TypeError, ValueError):
                out[k] = str(v)
            else:
                out[k] = v
        elif isinstance(v, (int, float, str, bool)) or v is None:
            out[k] = v
        else:
            out[k] = v
    return out


def publish_solis_sensors(data: dict[str, Any], ts: float | None = None) -> None:
    """Publish all sensors to MQTT: one JSON on <prefix>/status, and per-sensor on <prefix>/sensors/<name>."""
    if not MQTT_HOST:
        return
    payload = _serializable_payload(data)
    if ts is not None:
        payload["ts"] = ts
    try:
        client = mqtt.Client(client_id=MQTT_CLIENT_ID, protocol=mqtt.MQTTv311)
        if MQTT_USER:
            client.username_pw_set(MQTT_USER, MQTT_PASSWORD or "")
        client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
        # Full JSON on status topic
        client.publish(
            f"{MQTT_TOPIC_PREFIX}/status",
            json.dumps(payload),
            qos=0,
            retain=False,
        )
        # Individual sensor topics
        for key in _SENSOR_KEYS:
            if key not in data:
                continue
            val = data[key]
            if isinstance(val, (dict, list)):
                continue
            topic = f"{MQTT_TOPIC_PREFIX}/sensors/{key}"
            client.publish(topic, str(val) if val is not None else "", qos=0, retain=False)
        client.disconnect()
    except Exception as e:
        logger.warning("MQTT publish failed: %s", e)

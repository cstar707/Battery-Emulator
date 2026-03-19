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
    MQTT_LEGACY_PREFIX,
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
    "battery_remaining_kWh",
    "battery_runtime_hours",
    "battery_runtime_direction",
    "battery_voltage_V",
    "battery_current_A",
    "meter_power_W",
    "grid_freq_Hz",
    "inverter_temp_C",
    "inverter_state",
    "storage_control_raw",
    "energy_today_pv_kWh",
    "total_pv_energy_kWh",
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
        if k == "raw_blocks" or k.startswith("_"):
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


def _inverter_id_from_prefix(prefix: str) -> str:
    """e.g. solar/solis/s6-inv-1 -> s6-inv-1; solar/solis -> solis."""
    return prefix.split("/")[-1] if prefix else "s6-inv-1"


def publish_solis_sensors(
    data: dict[str, Any],
    ts: float | None = None,
    topic_prefix: str | None = None,
    publish_legacy: bool | None = None,
) -> None:
    """Publish to primary topic and optionally to legacy prefix for BE display.
    topic_prefix: primary prefix (e.g. solar/solis/s6-inv-1). If None, use MQTT_TOPIC_PREFIX.
    publish_legacy: if True, also publish to MQTT_LEGACY_PREFIX. If None, True only when topic_prefix is first inverter (s6-inv-1).
    """
    if not MQTT_HOST:
        return
    primary = (topic_prefix or MQTT_TOPIC_PREFIX).rstrip("/")
    payload = _serializable_payload(data)
    if ts is not None:
        payload["ts"] = ts
    payload["inverter_id"] = _inverter_id_from_prefix(primary)
    prefixes = [primary]
    if publish_legacy is None:
        publish_legacy = "s6-inv-1" in primary
    if publish_legacy and MQTT_LEGACY_PREFIX and MQTT_LEGACY_PREFIX.rstrip("/") != primary:
        prefixes.append(MQTT_LEGACY_PREFIX.rstrip("/"))
    try:
        client = mqtt.Client(client_id=MQTT_CLIENT_ID, protocol=mqtt.MQTTv311)
        if MQTT_USER:
            client.username_pw_set(MQTT_USER, MQTT_PASSWORD or "")
        client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
        json_str = json.dumps(payload)
        for prefix in prefixes:
            client.publish(f"{prefix}/status", json_str, qos=0, retain=False)
            for key in _SENSOR_KEYS:
                if key not in data:
                    continue
                val = data[key]
                if isinstance(val, (dict, list)):
                    continue
                client.publish(
                    f"{prefix}/sensors/{key}",
                    str(val) if val is not None else "",
                    qos=0,
                    retain=False,
                )
        client.disconnect()
    except Exception as e:
        logger.warning("MQTT publish failed: %s", e)


def publish_solark_status(online: bool) -> None:
    """Publish Solark connectivity status to solar/solark/status for display LED."""
    if not MQTT_HOST:
        return
    try:
        client = mqtt.Client(client_id=f"{MQTT_CLIENT_ID}-solark-status", protocol=mqtt.MQTTv311)
        if MQTT_USER:
            client.username_pw_set(MQTT_USER, MQTT_PASSWORD or "")
        client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
        payload = json.dumps({"online": online})
        client.publish("solar/solark/status", payload, qos=0, retain=False)
        client.disconnect()
    except Exception as e:
        logger.warning("MQTT publish solark status failed: %s", e)

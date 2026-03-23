"""
Publish Solis, Envoy sensor data to MQTT broker.
- Solis: one JSON on <prefix>/status + per-sensor topics.
- Envoy: full JSON on solar/envoy/status.
"""
from __future__ import annotations

import json
import logging
from typing import Any

import paho.mqtt.client as mqtt

from config import (
    get_be_mqtt_topic,
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

ENVOY_TOPIC = "solar/envoy/status"


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


def _publish_solis_to_topic(
    client: mqtt.Client,
    data: dict[str, Any],
    topic_prefix: str,
    ts: float | None = None,
) -> None:
    """Publish Solis data to given topic prefix."""
    payload = _serializable_payload(data)
    if ts is not None:
        payload["ts"] = ts
    client.publish(
        f"{topic_prefix}/status",
        json.dumps(payload),
        qos=0,
        retain=False,
    )
    for key in _SENSOR_KEYS:
        if key not in data:
            continue
        val = data[key]
        if isinstance(val, (dict, list)):
            continue
        topic = f"{topic_prefix}/sensors/{key}"
        client.publish(topic, str(val) if val is not None else "", qos=0, retain=False)


def publish_solis_sensors(
    data: dict[str, Any],
    ts: float | None = None,
    *,
    topic_prefix: str | None = None,
    publish_legacy: bool = False,
) -> None:
    """Publish all sensors to MQTT: one JSON on <prefix>/status, and per-sensor on <prefix>/sensors/<name>.
    Supports topic_prefix for per-inverter topics (e.g. solar/solis/s6-inv-1).
    publish_legacy: also publish to legacy solar/solis prefix for BE compatibility.
    """
    if not MQTT_HOST:
        return
    prefix = topic_prefix or MQTT_TOPIC_PREFIX
    try:
        client = mqtt.Client(client_id=MQTT_CLIENT_ID, protocol=mqtt.MQTTv311)
        if MQTT_USER:
            client.username_pw_set(MQTT_USER, MQTT_PASSWORD or "")
        client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
        _publish_solis_to_topic(client, data, prefix, ts=ts)
        if publish_legacy and prefix != "solar/solis":
            _publish_solis_to_topic(client, data, "solar/solis", ts=ts)
        client.disconnect()
    except Exception as e:
        logger.warning("MQTT publish Solis failed: %s", e)


def publish_solark_status(online: bool) -> None:
    """Publish Solark connectivity status to MQTT."""
    if not MQTT_HOST:
        return
    try:
        client = mqtt.Client(client_id=f"{MQTT_CLIENT_ID}-solark-status", protocol=mqtt.MQTTv311)
        if MQTT_USER:
            client.username_pw_set(MQTT_USER, MQTT_PASSWORD or "")
        client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
        client.publish("solar/solark/status", "online" if online else "offline", qos=0, retain=False)
        client.disconnect()
    except Exception as e:
        logger.warning("MQTT publish Solark status failed: %s", e)


def _envoy_total_production_from_data(data: dict[str, Any]) -> int:
    """Compute total Envoy production (W) from raw 3004 response. Same logic as main._envoy_total_production_w."""
    envoys = {k: v for k, v in data.items() if k.startswith("envoy") and isinstance(v, dict)}
    total = sum(int(v.get("production") or 0) for v in envoys.values())
    return max(0, min(11400, total))


def publish_envoy_sensors(
    data: dict[str, Any],
    *,
    summary: dict[str, int | float] | None = None,
) -> None:
    """Publish Envoy data to solar/envoy/status. data = raw response from 3004 /api/envoy/debug.
    Includes total_production_W (sum of envoy1.production + envoy2.production).
    If summary is provided, merges total_live, total_today, house_*, shed_*, trailer_* for clients
    that prefer a single JSON message (e.g. Waveshare)."""
    if not MQTT_HOST:
        return
    try:
        client = mqtt.Client(client_id=f"{MQTT_CLIENT_ID}-envoy", protocol=mqtt.MQTTv311)
        if MQTT_USER:
            client.username_pw_set(MQTT_USER, MQTT_PASSWORD or "")
        client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
        payload = _serializable_payload(data)
        payload["total_production_W"] = _envoy_total_production_from_data(data)
        if summary:
            for k in (
                "total_live", "total_today", "house_today", "shed_today", "trailer_today",
                "house_live", "shed_live", "trailer_live",
            ):
                if k in summary and summary[k] is not None:
                    payload[k] = summary[k]
        client.publish(ENVOY_TOPIC, json.dumps(payload), qos=0, retain=False)
        client.disconnect()
    except Exception as e:
        logger.warning("MQTT publish Envoy failed: %s", e)


ENVOY_SUMMARY_TOPIC_PREFIX = "solar/envoy/summary"


def publish_envoy_summary(summary: dict[str, int | float]) -> None:
    """Publish computed Envoy summary to solar/envoy/summary/<key>.
    Display subscribes to solar/envoy/summary/# and uses values directly — single source of truth.
    summary keys: total_live, total_today, house_today, shed_today, trailer_today,
                  house_live, shed_live, trailer_live."""
    if not MQTT_HOST:
        return
    keys = (
        "total_live", "total_today", "house_today", "shed_today", "trailer_today",
        "house_live", "shed_live", "trailer_live",
    )
    try:
        client = mqtt.Client(client_id=f"{MQTT_CLIENT_ID}-envoy-summary", protocol=mqtt.MQTTv311)
        if MQTT_USER:
            client.username_pw_set(MQTT_USER, MQTT_PASSWORD or "")
        client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
        for k in keys:
            if k in summary and summary[k] is not None:
                client.publish(
                    f"{ENVOY_SUMMARY_TOPIC_PREFIX}/{k}",
                    str(summary[k]),
                    qos=0,
                    retain=False,
                )
        client.disconnect()
    except Exception as e:
        logger.warning("MQTT publish Envoy summary failed: %s", e)


def publish_be_command(command: str, *, topic_prefix: str | None = None) -> bool:
    """Publish a command to the Battery Emulator via MQTT.
    command: STOP (open contactors), RESUME (close contactors + resume from pause), PAUSE (limit power to zero), BMSRESET (reset BMS), RESTART (reboot board).
    Returns True if published, False on error or no MQTT broker.
    """
    if not MQTT_HOST:
        return False
    cmd = (command or "").strip().upper()
    if cmd not in ("STOP", "RESUME", "PAUSE", "BMSRESET", "RESTART"):
        return False
    topic_base = (topic_prefix or get_be_mqtt_topic()).strip() or "BE"
    topic = f"{topic_base}/command/{cmd}"
    try:
        client = mqtt.Client(client_id=f"{MQTT_CLIENT_ID}-be-cmd", protocol=mqtt.MQTTv311)
        if MQTT_USER:
            client.username_pw_set(MQTT_USER, MQTT_PASSWORD or "")
        client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
        client.publish(topic, "", qos=0, retain=False)
        client.disconnect()
        logger.info("Published BE command %s to %s", cmd, topic)
        return True
    except Exception as e:
        logger.warning("MQTT publish BE command %s failed: %s", cmd, e)
        return False

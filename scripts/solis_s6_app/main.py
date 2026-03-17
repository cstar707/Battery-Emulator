"""
Solis S6 + Solark UI: FastAPI backend on port 3007.
Dashboard, sensors, storage toggles (43110), settings.
Run: uvicorn main:app --host 0.0.0.0 --port 3007
All optional deps (config, mqtt, modbus, debug) have fallbacks so the app always starts.
"""
from __future__ import annotations

import asyncio
import ipaddress
import json
import logging
import re
import threading
import time
from contextlib import asynccontextmanager
from datetime import datetime, timezone
from pathlib import Path
from urllib.request import Request as UrllibRequest, urlopen
from urllib.error import URLError
from base64 import b64encode

from fastapi import FastAPI, Request
from fastapi.responses import HTMLResponse, JSONResponse, RedirectResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates

# Config: must not crash
try:
    from config import (
        APP_VERSION,
        get_solark1_host,
        get_solark1_http_port,
        get_solark1_modbus_unit,
        get_solark1_port,
        get_solark2_modbus_unit,
        get_solis_host,
        get_solis_inverters,
        get_solis_modbus_unit,
        get_solis_port,
        get_solark_soc_automation_enabled,
        INVERTER_LABEL_SOLARK1,
        INVERTER_LABEL_SOLARK2,
        INVERTER_LABEL_SOLIS1,
        load_settings,
        MQTT_CLIENT_ID,
        MQTT_HOST,
        MQTT_PASSWORD,
        MQTT_PORT,
        MQTT_USER,
        POLL_INTERVAL_SEC,
        save_settings,
        ESPHOME_SOLARK_PASSWORD,
        ESPHOME_SOLARK_USER,
        get_esphome_solark_host,
        get_esphome_solark_port,
        SOLARK_HTTP_PASSWORD,
        SOLARK_HTTP_USER,
        SOLIS_DAILY_PV_SCALE,
        SOLARK_MQTT_TOPIC,
        SOLARK_SOC_FEEDIN_BELOW_PCT,
        SOLARK_SOC_SELF_USE_THRESHOLD_PCT,
        HA_URL,
        HA_TOKEN,
        HA_SWITCH_IQ8,
        HA_SWITCH_MSERIES,
        HA_SWITCH_TABUCHI,
        get_tabuchi_today_pv_kwh,
        SOLIS_TOTAL_BATTERY_KWH,
    )
except Exception as e:
    logging.warning("Config import failed, using defaults: %s", e)
    APP_VERSION = "0.0.0"
    INVERTER_LABEL_SOLIS1 = "Solis1"
    INVERTER_LABEL_SOLARK1, INVERTER_LABEL_SOLARK2 = "Solark1", "Solark2"
    MQTT_HOST, POLL_INTERVAL_SEC = "", 5.0
    SOLARK_SOC_SELF_USE_THRESHOLD_PCT, SOLARK_SOC_FEEDIN_BELOW_PCT = 98, 95
    SOLIS_DAILY_PV_SCALE = 1.0
    SOLARK_HTTP_USER, SOLARK_HTTP_PASSWORD = None, None
    SOLARK_MQTT_TOPIC = "solar/solark"
    MQTT_CLIENT_ID = "solis-s6-ui"
    MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASSWORD = "", 1883, None, None
    def get_solark1_host():
        return ""
    def get_solark1_http_port():
        return 80
    def get_solark1_port():
        return 502
    def get_solark1_modbus_unit():
        return 1
    def get_solark2_modbus_unit():
        return 2
    def get_solark_soc_automation_enabled():
        return True
    def load_settings():
        return {}
    def get_solis_host():
        return "10.10.53.16"
    def get_solis_port():
        return 502
    def get_solis_modbus_unit():
        return 1
    def save_settings(_):
        raise NotImplementedError("config failed")
    HA_URL, HA_TOKEN = "http://10.10.53.179:8123", ""
    HA_SWITCH_IQ8 = "switch.enphase_iq8_micro_socket_1"
    HA_SWITCH_MSERIES = "switch.shed_micro_inverters_socket_1"
    HA_SWITCH_TABUCHI = "switch.sonoff_1001204d65_1"
    def get_tabuchi_today_pv_kwh():
        return 3.0
    SOLIS_TOTAL_BATTERY_KWH = 48.0

# Debug ring: optional (capture reason so /debug page can show it)
_debug_unavailable_reason: str | None = None
try:
    from debug_ring import ModbusDebugHandler, clear as debug_clear, get_lines as debug_get_lines
    _debug_available = True
except Exception as e:
    _debug_unavailable_reason = str(e)
    logging.warning("Debug ring unavailable: %s", e)
    def debug_clear():
        pass
    def debug_get_lines():
        return []
    ModbusDebugHandler = None
    _debug_available = False

# MQTT: optional
try:
    from mqtt_publish import publish_solis_sensors
except Exception as e:
    logging.warning("MQTT publish unavailable: %s", e)
    def publish_solis_sensors(*a, **k):
        pass

# Modbus: optional – if this fails, app still runs with stub
_STORAGE_BIT_NAMES = [
    "self_use", "time_of_use", "off_grid", "battery_wakeup", "reserve_battery",
    "allow_grid_charge", "feed_in_priority", "batt_ovc", "forcecharge_peakshaving",
    "battery_current_correction", "battery_healing", "peak_shaving",
]
_HYBRID_BIT_NAMES = [
    "dual_backup", "ac_coupling", "smart_load_forced", "allow_export",
    "backup2load_auto", "backup2load_manual", "smart_load_offgrid_stop", "grid_peakshaving",
]

def _modbus_stub_return_false(*a, **k):
    return False
def _modbus_stub_return_dict_bits():
    return {n: False for n in _STORAGE_BIT_NAMES}
def _modbus_stub_return_hybrid_bits():
    return {n: False for n in _HYBRID_BIT_NAMES}
def _modbus_stub_poll():
    return {"ok": False, "storage_bits": {n: False for n in _STORAGE_BIT_NAMES}, "hybrid_bits": {n: False for n in _HYBRID_BIT_NAMES}}

try:
    from solis_modbus import (
        _HYBRID_BIT_NAMES as _HM,
        _STORAGE_BIT_NAMES as _SM,
        apply_use_all_solar_preset,
        get_active_power_limit,
        get_hybrid_control_bits,
        get_storage_control_bits,
        poll_solis,
        poll_solis_for_inverter,
        set_active_power_limit,
        set_export_target,
        set_grid_charge_limits,
        set_hybrid_control_bit,
        set_power_control_off,
        set_storage_control_bit,
        set_storage_control_bits,
    )
    _STORAGE_BIT_NAMES, _HYBRID_BIT_NAMES = _SM, _HM
    _modbus_available = True
except Exception as e:
    logging.warning("Modbus unavailable, using stubs: %s", e)
    _modbus_available = False
    poll_solis = _modbus_stub_poll
    set_storage_control_bits = _modbus_stub_return_false
    set_storage_control_bit = set_hybrid_control_bit = _modbus_stub_return_false
    set_active_power_limit = _modbus_stub_return_false
    apply_use_all_solar_preset = _modbus_stub_return_false
    def set_grid_charge_limits(*a, **kw): return {"ok": False, "message": "Modbus unavailable", "writes": []}
    def set_export_target(*a, **kw): return {"ok": False, "message": "Modbus unavailable", "writes": []}
    def set_power_control_off(*a, **kw): return {"ok": False, "message": "Modbus unavailable", "writes": []}
    get_storage_control_bits = _modbus_stub_return_dict_bits
    get_hybrid_control_bits = _modbus_stub_return_hybrid_bits
    def get_active_power_limit(): return {"ok": False, "enabled": False, "limit_pct": 100.0}

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)
# Keep Modbus logs clean: pymodbus can log transaction_id mismatches (expected with Solis)
logging.getLogger("pymodbus").setLevel(logging.WARNING)

# Attach debug ring to Modbus logger only if debug_ring loaded
if _debug_available and ModbusDebugHandler is not None:
    try:
        _modbus_logger = logging.getLogger("solis_modbus")
        _debug_handler = ModbusDebugHandler()
        _debug_handler.setFormatter(logging.Formatter("%(message)s"))
        _modbus_logger.addHandler(_debug_handler)
        _modbus_logger.setLevel(logging.DEBUG)
    except Exception as e:
        logger.warning("Could not attach Modbus debug handler: %s", e)
        _debug_available = False

# In-memory caches for Solis data keyed by topic_id (e.g. s6-inv-1, s6-inv-2)
_solis_caches: dict[str, dict] = {}


def _solis_cache_first() -> dict:
    """First inverter cache for single-inverter pages (control, grid_status)."""
    return _solis_caches.get("s6-inv-1") or {"data": {}, "ts": 0, "ok": False}
# Solark data: from MQTT only (solar/solark, solar/solark/sensors/#). source = "mqtt" | "mqtt-sensors"; background subscriber updates this.
_solark_cache: dict = {"data": {}, "ts": 0, "ok": False, "source": None, "last_error": None}

# Automation: when Solark SOC >= threshold, switch Solis to self-use; hysteresis below SOLARK_SOC_FEEDIN_BELOW_PCT
#
# *** CRITICAL SAFETY DEFAULT ***
# Both flags are initialised to True (curtailed) so the system boots in the SAFE state.
# Switches are OFF and Solis output is limited to 0% at startup (enforced in lifespan).
# Generation is ONLY restored once Solark SOC drops BELOW SOLARK_SOC_FEEDIN_BELOW_PCT (95%).
# This prevents over-charge faults if the app restarts while SOC is high.
_solark_auto_self_use_active: bool = True  # assume curtailed until SOC confirmed below 95%
_last_solis_auto_switch_ts: float = 0
_AUTO_SWITCH_COOLDOWN_SEC = 300  # don't flip Solis mode more than once per 5 min

# Automation: HA switch curtailment (Steps 2-4) — fire alongside Solis mode switch
_ha_curtail_active: bool = True            # assume curtailed at startup — only released when SOC < 95%
_last_ha_curtail_ts: float = 0             # last time any HA curtail action was fired
_HA_CURTAIL_COOLDOWN_SEC = 120             # minimum seconds between HA switch state changes

# Modbus write timeout (seconds) so a stuck inverter doesn't hang the request
_MODBUS_WRITE_TIMEOUT = 15.0

# One-hot work-mode bits for storage register 43110.
# Enabling any one of these modes clears the others in the same write.
_STORAGE_WORK_MODE_BITS = (0, 2, 6, 11)  # self_use, off_grid, feed_in_priority, peak_shaving
_STORAGE_OFF_GRID_BIT = 2
_STORAGE_ALLOW_GRID_CHARGE_BIT = 5
_HYBRID_ALLOW_EXPORT_BIT = 3


def _storage_changes_with_exclusions(bit_index: int, on: bool) -> dict:
    """Return full set of bit changes to apply, including mutual exclusions."""
    changes = {bit_index: on}
    if on and bit_index in _STORAGE_WORK_MODE_BITS:
        for mode_bit in _STORAGE_WORK_MODE_BITS:
            if mode_bit != bit_index:
                changes[mode_bit] = False
    return changes


def _off_grid_active() -> bool:
    """Read current storage bits and report whether Off-Grid mode is active."""
    return bool((get_storage_control_bits() or {}).get("off_grid"))


def _apply_control_change(register: str, bit_index: int, on: bool) -> tuple[bool, str | None]:
    """Apply one control change, enforcing cross-register Off-Grid policy when needed."""
    if register == "storage" and 0 <= bit_index <= 11:
        if on and bit_index == _STORAGE_ALLOW_GRID_CHARGE_BIT and _off_grid_active():
            return False, "off_grid_policy"

        if on and bit_index == _STORAGE_OFF_GRID_BIT:
            # Off-Grid dominates both policy bits: clear export first, then storage bits.
            if not set_hybrid_control_bit(_HYBRID_ALLOW_EXPORT_BIT, False):
                return False, "write_failed"
            changes = _storage_changes_with_exclusions(bit_index, on)
            changes[_STORAGE_ALLOW_GRID_CHARGE_BIT] = False
            ok = set_storage_control_bits(changes)
            return bool(ok), None if ok else "write_failed"

        changes = _storage_changes_with_exclusions(bit_index, on)
        ok = set_storage_control_bits(changes)
        return bool(ok), None if ok else "write_failed"

    if register == "hybrid" and 0 <= bit_index <= 7:
        if on and bit_index == _HYBRID_ALLOW_EXPORT_BIT and _off_grid_active():
            return False, "off_grid_policy"
        ok = set_hybrid_control_bit(bit_index, on)
        return bool(ok), None if ok else "write_failed"

    return False, "invalid_bit"


def _control_write_timeout(register: str, bit_index: int, on: bool) -> float:
    """Allow a longer timeout for multi-step policy writes such as Off-Grid activation."""
    if register == "storage" and bit_index == _STORAGE_OFF_GRID_BIT and on:
        return _MODBUS_WRITE_TIMEOUT * 2
    return _MODBUS_WRITE_TIMEOUT


# Map individual sensor topic suffixes → data dict keys used by the rest of the app.
# Topics: solar/solark/sensors/<suffix>  (each a plain numeric string)
_SENSOR_TOPIC_MAP: dict[str, tuple[str, float]] = {
    # suffix                  : (data_key,          scale)
    "battery_soc"             : ("battery_soc_pptt",   100.0),   # % → pptt (×100)
    "battery_voltage"         : ("battery_voltage_dV",  10.0),   # V → dV
    "battery_current"         : ("battery_current_dA",  10.0),   # A → dA
    "battery_power"           : ("battery_power_W",      1.0),
    "slave_battery_power"     : ("battery_slave_power_W", 1.0),
    "total_battery_power"     : ("battery_total_power_W", 1.0),
    "battery_temperature"     : ("battery_temperature",   1.0),
    "solar_power"             : ("pv_power_W",            1.0),
    "total_solar_power"       : ("total_pv_power_W",      1.0),
    "pv1_power"               : ("pv1_power_W",           1.0),
    "pv2_power"               : ("pv2_power_W",           1.0),
    "load_power"              : ("load_power_W",          1.0),
    "grid_power"              : ("grid_power_W",          1.0),
    "grid_ct_power"           : ("grid_ct_power_W",       1.0),
    "inverter_power"          : ("inverter_power_W",      1.0),
    "inverter_current"        : ("inverter_current_dA",  10.0),   # A → dA
    "day_pv_energy"           : ("day_pv_energy_kWh",     1.0),
    "day_battery_charge"      : ("day_batt_charge_kWh",   1.0),
    "day_battery_discharge"   : ("day_batt_discharge_kWh",1.0),
    "day_load_energy"         : ("day_load_energy_kWh",   1.0),
    "total_pv_energy"         : ("total_pv_energy_kWh",   1.0),
    "total_battery_charge"    : ("total_batt_charge_kWh", 1.0),
    "total_battery_discharge" : ("total_batt_dis_kWh",    1.0),
    "total_load_energy"       : ("total_load_energy_kWh", 1.0),
    "total_grid_export"       : ("total_grid_export_kWh", 1.0),
    "total_grid_import"       : ("total_grid_import_kWh", 1.0),
    "slave_battery_soc"       : ("battery_slave_soc_pptt", 100.0),
}

_solark_sensor_scratch: dict = {}   # accumulates individual sensor values between ticks

# Sunsynk/Solark Modbus: positive = discharge, negative = charge. We normalize to match Solis UI: + = charge, − = discharge.
_SOLARK_BATTERY_POWER_KEYS = ("battery_power_W", "battery_total_power_W")
_SOLARK_BATTERY_CURRENT_KEY = "battery_current_dA"


def _normalize_solark_data(data: dict) -> dict:
    """Normalize Solark data so battery power and current use + = charging, − = discharging (like Solis). Mutates and returns data."""
    for k in _SOLARK_BATTERY_POWER_KEYS:
        v = data.get(k)
        if v is not None and isinstance(v, (int, float)):
            data[k] = -v
    v = data.get(_SOLARK_BATTERY_CURRENT_KEY)
    if v is not None and isinstance(v, (int, float)):
        data[_SOLARK_BATTERY_CURRENT_KEY] = -v
    return data


def _on_solark_mqtt_message(_client, _userdata, msg):
    """Update _solark_cache from MQTT.

    Handles two formats:
      1. JSON blob on  solar/solark          — must contain battery_soc_pptt; see docs/solis-s6-app-solark-mqtt-contract.md
      2. Individual    solar/solark/sensors/<suffix>  — suffix must be in _SENSOR_TOPIC_MAP; at least battery_soc required.
    """
    global _solark_cache, _solark_sensor_scratch
    try:
        topic: str = msg.topic
        payload = msg.payload.decode().strip()

        # ── Format 1: JSON blob ──────────────────────────────────────────────
        if topic == SOLARK_MQTT_TOPIC or not topic.startswith(SOLARK_MQTT_TOPIC + "/sensors/"):
            data = json.loads(payload) if payload else {}
            if data.get("battery_soc_pptt") is not None:
                _solark_cache["data"] = _normalize_solark_data(data)
                _solark_cache["ts"] = time.time()
                _solark_cache["ok"] = True
                _solark_cache["source"] = "mqtt"
                _solark_cache["last_error"] = None
            return

        # ── Format 2: individual sensor topic ───────────────────────────────
        suffix = topic.split("/sensors/", 1)[1]
        mapping = _SENSOR_TOPIC_MAP.get(suffix)
        if mapping is None:
            return
        data_key, scale = mapping
        try:
            raw = float(payload)
        except ValueError:
            return
        _solark_sensor_scratch[data_key] = round(raw * scale) if scale != 1.0 else raw

        # Flush scratch → cache whenever we have at least SOC
        if "battery_soc_pptt" in _solark_sensor_scratch:
            _solark_cache["data"] = _normalize_solark_data(dict(_solark_sensor_scratch))
            _solark_cache["ts"] = time.time()
            _solark_cache["ok"] = True
            _solark_cache["source"] = "mqtt-sensors"
            _solark_cache["last_error"] = None

    except (ValueError, TypeError, json.JSONDecodeError) as e:
        logger.debug("Solark MQTT payload parse: %s", e)


def _run_solark_mqtt_subscriber() -> None:
    """Subscribe to both JSON blob topic and individual sensor topics."""
    if not MQTT_HOST:
        return
    try:
        import paho.mqtt.client as mqtt
        client = mqtt.Client(client_id=f"{MQTT_CLIENT_ID}-solark", protocol=mqtt.MQTTv311)
        if MQTT_USER:
            client.username_pw_set(MQTT_USER, MQTT_PASSWORD or "")
        client.on_message = _on_solark_mqtt_message
        client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
        # Subscribe to JSON blob and individual sensor topics (publisher is configurable; not the .90 Tesla BE)
        topics = []
        if SOLARK_MQTT_TOPIC:
            topics.append((SOLARK_MQTT_TOPIC, 0))
        topics.append(("solar/solark/sensors/#", 0))
        client.subscribe(topics)
        logger.info("Solark MQTT subscriber: %s + solar/solark/sensors/# on %s", SOLARK_MQTT_TOPIC, MQTT_HOST)
        client.loop_forever()
    except Exception as e:
        logger.warning("Solark MQTT subscriber failed: %s", e)
        _solark_cache["last_error"] = f"MQTT: {e}"
        _solark_cache["source"] = "mqtt"


def _fetch_solark_data_sync() -> tuple[dict, str | None]:
    """GET /solark_data from the Solark data source (SOLARK1_HOST). Return (data, None) on success or ({}, error_str) on failure.
    Note: 10.10.53.90 is the Tesla Battery Emulator (BE) board; Solark is a separate source—set SOLARK1_HOST to the device that serves /solark_data."""
    host = get_solark1_host()
    if not host:
        return {}, None
    port = get_solark1_http_port()
    url = f"http://{host}:{port}/solark_data"
    req = UrllibRequest(url)
    if SOLARK_HTTP_USER and SOLARK_HTTP_PASSWORD:
        creds = b64encode(f"{SOLARK_HTTP_USER}:{SOLARK_HTTP_PASSWORD}".encode()).decode()
        req.add_header("Authorization", f"Basic {creds}")
    try:
        with urlopen(req, timeout=10) as r:
            raw = r.read().decode()
            data = json.loads(raw) if raw else {}
            return (data, None) if data.get("battery_soc_pptt") is not None else (data, "No battery_soc_pptt in response")
    except (URLError, OSError, ValueError, json.JSONDecodeError) as e:
        err = str(e)
        logger.warning("Solark fetch %s: %s", url, err)
        return {}, err


def _fetch_esphome_solark_data_sync() -> tuple[dict, str | None]:
    """Fetch Solark data from ESPHome device via per-sensor REST API.

    All fields use the sunsynk Total* sensors which combine primary+slave inverters.
    """
    host = get_esphome_solark_host()
    if not host:
        return {}, None
    port = get_esphome_solark_port()

    creds_header: str | None = None
    if ESPHOME_SOLARK_USER and ESPHOME_SOLARK_PASSWORD:
        creds = b64encode(f"{ESPHOME_SOLARK_USER}:{ESPHOME_SOLARK_PASSWORD}".encode()).decode()
        creds_header = f"Basic {creds}"

    def _get(sensor_id: str) -> float | None:
        url = f"http://{host}:{port}/sensor/{sensor_id}"
        req = UrllibRequest(url)
        if creds_header:
            req.add_header("Authorization", creds_header)
        try:
            with urlopen(req, timeout=4) as r:
                d = json.loads(r.read().decode())
                v = d.get("value")
                return float(v) if v is not None else None
        except Exception as exc:
            raise OSError(f"{sensor_id}: {exc}") from exc

    try:
        soc = _get("sunsynk_battery_soc")
        if soc is None:
            return {}, f"ESPHome {host}: battery_soc is NA"

        def _w(sid: str) -> int:
            v = _get(sid)
            return int(v) if v is not None else 0

        def _kwh(sid: str) -> float | None:
            v = _get(sid)
            return round(float(v), 2) if v is not None else None

        master_batt = _w("sunsynk_battery_power")  # primary inverter only
        total_batt = _w("sunsynk_total_battery_power")  # master + slave
        slave_batt = total_batt - master_batt if (total_batt != 0 or master_batt != 0) else 0
        result: dict = {
            # --- live power ---
            "battery_soc_pptt":       int(soc * 100),
            "battery_power_W":        total_batt,  # backward compat (total)
            "battery_master_power_W": master_batt,
            "battery_slave_power_W":  slave_batt,
            "battery_total_power_W":  total_batt,
            "pv_power_W":             _w("sunsynk_total_solar_power"),
            "pv1_power_W":            int(pv1) if (pv1 := _get("sunsynk_pv1_power")) is not None else None,
            "pv2_power_W":            int(pv2) if (pv2 := _get("sunsynk_pv2_power")) is not None else None,
            "grid_ct_power_W":        _w("sunsynk_total_grid_ct_power"),
            "inverter_power_W":       _w("sunsynk_total_inverter_power"),
            # --- live electrical ---
            "battery_current_dA":     int((_get("sunsynk_total_battery_current") or 0) * 10),
            "battery_voltage_dV":     int((_get("sunsynk_battery_voltage") or 0) * 10),
            "battery_temperature":    _get("sunsynk_battery_temperature"),
            "inverter_current_dA":    int((_get("sunsynk_total_inverter_current") or 0) * 10),
            # --- today's energy (kWh) ---
            "day_pv_energy_kWh":      _kwh("sunsynk_total_day_pv_energy"),
            "day_batt_charge_kWh":    _kwh("sunsynk_total_day_battery_charge"),
            "day_batt_discharge_kWh": _kwh("sunsynk_total_day_battery_discharge"),
            "day_load_energy_kWh":    _kwh("sunsynk_total_day_load_energy"),
            # --- lifetime totals (kWh) ---
            "total_pv_energy_kWh":    _kwh("sunsynk_total_pv_energy"),
            "total_batt_charge_kWh":  _kwh("sunsynk_total_battery_charge"),
            "total_batt_dis_kWh":     _kwh("sunsynk_total_battery_discharge"),
            "total_load_energy_kWh":  _kwh("sunsynk_total_load_energy"),
            "total_grid_export_kWh":  _kwh("sunsynk_total_grid_export"),
            "total_grid_import_kWh":  _kwh("sunsynk_total_grid_import"),
        }
        return result, None
    except (URLError, OSError, ValueError, json.JSONDecodeError) as exc:
        err = str(exc)
        logger.warning("ESPHome Solark fetch %s: %s", host, err)
        return {}, err


def _run_solark_solis_automation(solis_data: dict, solark_data: dict) -> None:
    """
    Curtail / restore Solis PV output using the active power limit registers.

    Curtail  (SOC ≥ threshold): set power limit to 0% — inverter ramps MPPT to 0W.
    Restore  (SOC < hysteresis): disable power limit — inverter returns to full output.

    No operating-mode (43110) changes are made; inverter stays in Feed-In Priority.
    This is correct for Phase 1 (no Solis battery). Revisit in Phase 2 when a Solis
    battery is connected (see docs/SOLIS-STORAGE-MODE-TOGGLES.md).
    """
    global _solark_auto_self_use_active, _last_solis_auto_switch_ts
    if not get_solark_soc_automation_enabled() or not _modbus_available or SOLARK_SOC_SELF_USE_THRESHOLD_PCT <= 0:
        return
    now = time.time()
    if now - _last_solis_auto_switch_ts < _AUTO_SWITCH_COOLDOWN_SEC:
        return
    raw_soc = solark_data.get("battery_soc_pptt")
    if raw_soc is None:
        return
    soc_pptt = int(raw_soc)
    soc_pct = soc_pptt / 100.0
    threshold_pptt = SOLARK_SOC_SELF_USE_THRESHOLD_PCT * 100
    below_pptt = SOLARK_SOC_FEEDIN_BELOW_PCT * 100
    logger.debug(
        "solis automation check: soc_pptt=%d threshold=%d below=%d curtail_active=%s",
        soc_pptt, threshold_pptt, below_pptt, _solark_auto_self_use_active,
    )
    if soc_pptt >= threshold_pptt and not _solark_auto_self_use_active:
        # Curtail: limit Solis output to 0%
        try:
            if set_active_power_limit(0.0):
                _solark_auto_self_use_active = True
                _last_solis_auto_switch_ts = now
                logger.info("Solark SOC %.1f%% >= %d%%: Solis power limited to 0%%", soc_pct, SOLARK_SOC_SELF_USE_THRESHOLD_PCT)
        except Exception as e:
            logger.warning("Automation curtail Solis: %s", e)
    elif _solark_auto_self_use_active and soc_pptt < below_pptt:
        # Restore: disable power limit
        try:
            if set_active_power_limit(100.0):
                _solark_auto_self_use_active = False
                _last_solis_auto_switch_ts = now
                logger.info("Solark SOC %.1f%% < %d%%: Solis power limit disabled (full output)", soc_pct, SOLARK_SOC_FEEDIN_BELOW_PCT)
        except Exception as e:
            logger.warning("Automation restore Solis: %s", e)


def _run_ha_curtailment_automation(solark_data: dict) -> None:
    """Turn all three HA generation switches off when SOC >= threshold; restore when below hysteresis."""
    global _ha_curtail_active, _last_ha_curtail_ts
    if not get_solark_soc_automation_enabled() or SOLARK_SOC_SELF_USE_THRESHOLD_PCT <= 0:
        return
    if not HA_TOKEN or not HA_URL:
        return
    now = time.time()
    if now - _last_ha_curtail_ts < _HA_CURTAIL_COOLDOWN_SEC:
        return
    raw_soc = solark_data.get("battery_soc_pptt")
    if raw_soc is None:
        return
    soc_pptt = int(raw_soc)
    soc_pct = soc_pptt / 100.0
    threshold_pptt = SOLARK_SOC_SELF_USE_THRESHOLD_PCT * 100
    below_pptt = SOLARK_SOC_FEEDIN_BELOW_PCT * 100

    if soc_pptt >= threshold_pptt and not _ha_curtail_active:
        logger.info("HA curtailment: SOC %.1f%% >= %d%% — turning off all generation switches", soc_pct, SOLARK_SOC_SELF_USE_THRESHOLD_PCT)
        errors = []
        for sw in _CURTAIL_SWITCHES:
            result = _ha_switch_sync(sw["entity"], turn_on=False)
            if not result.get("ok"):
                errors.append(f"{sw['key']}: {result.get('error', 'unknown')}")
        if errors:
            logger.warning("HA curtailment: some switches failed: %s", errors)
        else:
            _ha_curtail_active = True
            _last_ha_curtail_ts = now
            logger.info("HA curtailment: all switches off (SOC=%.1f%%)", soc_pct)

    elif _ha_curtail_active and soc_pptt < below_pptt:
        logger.info("HA curtailment restore: SOC %.1f%% < %d%% — turning on all generation switches", soc_pct, SOLARK_SOC_FEEDIN_BELOW_PCT)
        errors = []
        for sw in _CURTAIL_SWITCHES:
            result = _ha_switch_sync(sw["entity"], turn_on=True)
            if not result.get("ok"):
                errors.append(f"{sw['key']}: {result.get('error', 'unknown')}")
        if errors:
            logger.warning("HA curtailment restore: some switches failed: %s", errors)
        else:
            _ha_curtail_active = False
            _last_ha_curtail_ts = now
            logger.info("HA curtailment: all switches restored (SOC=%.1f%%)", soc_pct)


def _poll_sync() -> None:
    global _solis_caches, _solark_cache
    ts = time.time()
    try:
        inverters = get_solis_inverters()
        for inv in inverters:
            topic_id = inv["topic_id"]
            try:
                data = poll_solis_for_inverter(inv["host"], inv["port"], inv["unit"])
            except Exception as e:
                logger.warning("poll_solis_for_inverter %s: %s", topic_id, e)
                _solis_caches[topic_id] = {"data": {}, "ts": ts, "ok": False}
                continue
            # Battery runtime enrichment
            soc = data.get("battery_soc_pct")
            power_w = data.get("battery_power_W")
            if (power_w is None or power_w == 0) and data.get("battery_voltage_V") and data.get("battery_current_A"):
                try:
                    v = float(data["battery_voltage_V"])
                    i = float(data["battery_current_A"])
                    if i != 0:
                        power_w = round(v * i, 0)
                except (TypeError, ValueError):
                    pass
            cap = SOLIS_TOTAL_BATTERY_KWH
            if soc is not None and isinstance(soc, (int, float)):
                data["battery_remaining_kWh"] = round(cap * (float(soc) / 100.0), 2)
                remaining = data["battery_remaining_kWh"]
                if power_w is not None and power_w != 0:
                    power_kw = float(power_w) / 1000.0
                    if power_w > 0:
                        to_full_kwh = max(0, cap - remaining)
                        data["battery_runtime_hours"] = round(to_full_kwh / power_kw, 2) if power_kw > 0 else None
                        data["battery_runtime_direction"] = "charge"
                    else:
                        data["battery_runtime_hours"] = round(remaining / abs(power_kw), 2)
                        data["battery_runtime_direction"] = "discharge"
                else:
                    data["battery_runtime_hours"] = None
                    data["battery_runtime_direction"] = "idle"
            else:
                data["battery_remaining_kWh"] = None
                data["battery_runtime_hours"] = None
                data["battery_runtime_direction"] = None
            _solis_caches[topic_id] = {"data": data, "ts": ts, "ok": data.get("ok", False)}
            try:
                publish_solis_sensors(
                    data,
                    ts=ts,
                    topic_prefix=f"solar/solis/{topic_id}",
                    publish_legacy=(topic_id == "s6-inv-1"),
                )
            except Exception as mqtt_e:
                logger.warning("MQTT publish %s: %s", topic_id, mqtt_e)
        # Use first inverter data for automations
        data = _solis_cache_first().get("data") or {}
        # Solark data: MQTT only. Background subscriber (solar/solark, solar/solark/sensors/#) updates _solark_cache; we don't fetch here.
        if not MQTT_HOST:
            _solark_cache["ok"] = False
            _solark_cache["last_error"] = "Solark: set MQTT_HOST and a publisher to solar/solark"
            _solark_cache["source"] = None
            _solark_cache["ts"] = ts

        # Run automations using whatever solark data we have (from MQTT)
        cached_solark = _solark_cache.get("data") or {}
        if cached_solark and SOLARK_SOC_SELF_USE_THRESHOLD_PCT > 0:
            _run_solark_solis_automation(data, cached_solark)
            _run_ha_curtailment_automation(cached_solark)
    except Exception as e:
        logger.exception("poll: %s", e)
        fail_ts = time.time()
        for inv in get_solis_inverters():
            _solis_caches[inv["topic_id"]] = {"data": {}, "ts": fail_ts, "ok": False}


async def _background_poller() -> None:
    while True:
        try:
            await asyncio.wait_for(
                asyncio.get_event_loop().run_in_executor(None, _poll_sync),
                timeout=30,
            )
        except asyncio.TimeoutError:
            logger.warning("background poll: timed out after 30s — Modbus read stalled, skipping cycle")
        except Exception as e:
            logger.exception("background poll: %s", e)
        await asyncio.sleep(max(1, POLL_INTERVAL_SEC))


_POWER_CONTROL_REFRESH_SEC = 120  # Refresh Solis dead-man every 2 min


async def _background_power_control_refresher() -> None:
    """Refresh Solis power control (import/export) every 2 min to keep dead-man alive."""
    await asyncio.sleep(60)  # Delay first run so poller is established
    loop = asyncio.get_event_loop()
    while True:
        try:
            state = _load_power_control()
            mode, watts = state.get("mode", "off"), state.get("watts", 0)
            if mode == "off":
                result = {"ok": True, "message": "no-op"}  # Skip Modbus when off
            elif mode == "import" and watts > 0:
                result = await asyncio.wait_for(
                    loop.run_in_executor(
                        None,
                        lambda: set_grid_charge_limits(import_watts=watts, charge_limit_watts=watts, max_amps=70),
                    ),
                    timeout=_MODBUS_WRITE_TIMEOUT,
                )
            elif mode == "export" and watts > 0:
                result = await asyncio.wait_for(
                    loop.run_in_executor(None, lambda: set_export_target(watts)),
                    timeout=_MODBUS_WRITE_TIMEOUT,
                )
            else:
                result = {"ok": True, "message": "no-op"}
            if not result.get("ok"):
                logger.warning("power_control refresh %s: %s", mode, result.get("message", "?"))
        except asyncio.TimeoutError:
            logger.warning("power_control refresh: Modbus timeout")
        except Exception as e:
            logger.exception("power_control refresh: %s", e)
        await asyncio.sleep(_POWER_CONTROL_REFRESH_SEC)


def _enforce_startup_safe_state() -> None:
    """
    *** CRITICAL — runs once at application startup ***

    Enforces the mandatory safe default:
      - All HA generation switches → OFF (IQ8, Shed, Tabuchi)
      - Solis power limit          → 0% (inverter produces nothing)

    This runs unconditionally regardless of current SOC.  Generation is only
    restored by _run_ha_curtailment_automation / _run_solark_solis_automation
    once Solark SOC is confirmed BELOW 95%.

    DO NOT REMOVE or bypass this function.  Failing to enforce the safe state
    at startup can cause an over-charge fault on the Solark battery if the app
    restarts while SOC is high (97%+) and PV is still generating.
    """
    logger.warning("STARTUP SAFE STATE: enforcing all switches OFF and Solis 0%% limit")

    # 1. Turn all HA generation switches OFF
    if HA_TOKEN and HA_URL:
        for sw in _CURTAIL_SWITCHES:
            try:
                result = _ha_switch_sync(sw["entity"], turn_on=False)
                if result.get("ok"):
                    logger.warning("STARTUP SAFE STATE: turned OFF %s", sw["key"])
                else:
                    logger.error("STARTUP SAFE STATE: failed to turn OFF %s — %s", sw["key"], result.get("error"))
            except Exception as exc:
                logger.error("STARTUP SAFE STATE: exception turning OFF %s — %s", sw["key"], exc)
    else:
        logger.error("STARTUP SAFE STATE: HA_TOKEN or HA_URL not configured — cannot enforce switch state!")

    # 2. Set Solis power limit to 0% (inverter produces nothing)
    if _modbus_available:
        try:
            ok = set_active_power_limit(0.0)
            if ok:
                logger.warning("STARTUP SAFE STATE: Solis power limit set to 0%%")
            else:
                logger.error("STARTUP SAFE STATE: Solis power limit write returned False")
        except Exception as exc:
            logger.error("STARTUP SAFE STATE: exception setting Solis power limit — %s", exc)
    else:
        logger.error("STARTUP SAFE STATE: Modbus not available — cannot enforce Solis 0%% limit!")

    logger.warning(
        "STARTUP SAFE STATE: complete. Generation locked OFF until Solark SOC < %d%% (SOLARK_SOC_FEEDIN_BELOW_PCT).",
        SOLARK_SOC_FEEDIN_BELOW_PCT,
    )


@asynccontextmanager
async def lifespan(app: FastAPI):
    # *** CRITICAL: always enforce safe defaults before accepting any traffic ***
    loop = asyncio.get_event_loop()
    await loop.run_in_executor(None, _enforce_startup_safe_state)

    asyncio.create_task(_background_poller())
    asyncio.create_task(_background_envoy_refresher())
    asyncio.create_task(_background_power_control_refresher())
    if MQTT_HOST and SOLARK_MQTT_TOPIC:
        _mqtt_thread = threading.Thread(target=_run_solark_mqtt_subscriber, daemon=True)
        _mqtt_thread.start()
    yield


app = FastAPI(title="Solis S6 / Solark UI", lifespan=lifespan)


@app.exception_handler(Exception)
async def global_exception_handler(request: Request, exc: Exception):
    """Prevent uncaught exceptions from crashing the worker; return error page or JSON."""
    try:
        logger.exception("Unhandled error: %s", exc)
    except Exception:
        pass
    try:
        path = getattr(request, "url", None) and getattr(request.url, "path", "") or ""
        if path.startswith("/api/"):
            return JSONResponse(
                {"ok": False, "error": "Server error. Try again later."},
                status_code=500,
            )
        if path.startswith("/control"):
            return RedirectResponse(url="/control?error=server_error", status_code=303)
        return RedirectResponse(url="/?error=server_error", status_code=303)
    except Exception:
        return JSONResponse(
            {"ok": False, "error": "Server error."},
            status_code=500,
        )


BASE = Path(__file__).resolve().parent
_POWER_CONTROL_FILE = BASE / "power_control.json"
templates = Jinja2Templates(directory=str(BASE / "templates"))


def _load_power_control() -> dict:
    """Load power control state. mode: off|import|export, watts: 0-11400."""
    try:
        if _POWER_CONTROL_FILE.exists():
            data = json.loads(_POWER_CONTROL_FILE.read_text())
            mode = str(data.get("mode", "off")).lower()
            if mode not in ("off", "import", "export"):
                mode = "off"
            watts = int(data.get("watts", 0))
            watts = max(0, min(11400, watts))
            return {"mode": mode, "watts": watts}
    except Exception as e:
        logger.warning("load power_control: %s", e)
    return {"mode": "off", "watts": 0}


def _save_power_control(data: dict) -> None:
    """Persist power control state."""
    try:
        _POWER_CONTROL_FILE.write_text(json.dumps(data, indent=2))
    except Exception as e:
        logger.warning("save power_control: %s", e)
if (BASE / "static").exists():
    app.mount("/static", StaticFiles(directory=str(BASE / "static")), name="static")


def _template_ctx() -> dict:
    return {
        "app_version": APP_VERSION,
        "inverter_label": "Solis + Solark",
        "label_solis1": INVERTER_LABEL_SOLIS1,
        "label_solark1": INVERTER_LABEL_SOLARK1,
        "label_solark2": INVERTER_LABEL_SOLARK2,
        "solark1_host": get_solark1_host() or "(not set)",
    }


def _page_ctx(request: Request, **kwargs) -> dict:
    ctx = _template_ctx()
    ctx["request"] = request
    ctx.update(kwargs)
    return ctx


def _merge_bits(defaults: dict, from_data: dict | None) -> dict:
    """Merge cached bits with defaults so template always has all keys."""
    out = dict(defaults)
    if from_data:
        for k in out:
            if k in from_data:
                out[k] = from_data[k]
    return out


def _storage_mode_label(storage_bits: dict) -> str:
    """Derive a human-readable inverter mode from storage control bits (register 43110)."""
    if not storage_bits:
        return "—"
    # Main mode (one primary; Solis typically has one of these active)
    if storage_bits.get("off_grid"):
        mode = "Off-grid"
    elif storage_bits.get("peak_shaving"):
        mode = "Peak-shaving"
    elif storage_bits.get("feed_in_priority"):
        mode = "Feed-in"
    elif storage_bits.get("self_use"):
        mode = "Self-use"
    elif storage_bits.get("time_of_use"):
        mode = "Time of use (TOU)"
    elif storage_bits.get("reserve_battery"):
        mode = "Reserve battery"
    else:
        mode = "Unknown"
    # Optional qualifiers
    extras = []
    if storage_bits.get("allow_grid_charge") and mode not in ("—", "Unknown"):
        extras.append("grid charge")
    if storage_bits.get("peak_shaving") and mode != "Peak-shaving":
        extras.append("peak-shaving")
    if extras:
        mode = f"{mode} + {', '.join(extras)}"
    return mode


def _build_solis_inverters() -> list[dict]:
    """Build list of {topic_id, label, data, ok, ts} for dashboard/API from _solis_caches and config."""
    out: list[dict] = []
    for inv in get_solis_inverters():
        tid = inv["topic_id"]
        cache = _solis_caches.get(tid) or {"data": {}, "ts": 0, "ok": False}
        out.append({
            "topic_id": tid,
            "label": inv.get("label", tid),
            "data": cache.get("data") or {},
            "ok": cache.get("ok", False),
            "ts": cache.get("ts", 0),
        })
    return out


@app.get("/", response_class=HTMLResponse)
async def index(request: Request):
    data = _solis_cache_first().get("data", {}) or {}
    storage_bits = _merge_bits({n: False for n in _STORAGE_BIT_NAMES}, data.get("storage_bits"))
    ts = _solis_cache_first().get("ts", 0) or 0
    last_updated = datetime.fromtimestamp(ts, tz=timezone.utc).strftime("%H:%M:%S UTC") if ts else "—"
    solark_data = _solark_cache.get("data", {}) or {}
    solark_ok = _solark_cache.get("ok", False)
    solark_source = _solark_cache.get("source")
    solark_last_error = _solark_cache.get("last_error")
    q = request.query_params
    tabuchi_kwh = get_tabuchi_today_pv_kwh()
    solis_today = (data.get("energy_today_pv_kWh") or 0) if isinstance(data.get("energy_today_pv_kWh"), (int, float)) else 0
    ctx = _page_ctx(
        request,
        data=data,
        ok=_solis_cache_first().get("ok", False),
        storage_bits=storage_bits,
        storage_mode_label=_storage_mode_label(storage_bits),
        last_updated=last_updated,
        solis_inverters=_build_solis_inverters(),
        solark_data=solark_data,
        solark_ok=solark_ok,
        solark_source=solark_source,
        solark_last_error=solark_last_error,
        solark_auto_self_use=_solark_auto_self_use_active,
        automation_enabled=get_solark_soc_automation_enabled(),
        power_limit=get_active_power_limit(),
        flash_saved=q.get("saved") == "1",
        flash_error=q.get("error"),
        flash_automation=q.get("automation"),  # "on" or "off" after toggle
        grid_status={
            "tabuchi_today_pv_kWh": tabuchi_kwh,
            "solis_today_pv_kWh": solis_today,
            "total_today_pv_kWh": solis_today + tabuchi_kwh,
        },
    )
    return templates.TemplateResponse("dashboard.html", ctx)


@app.post("/")
async def index_toggle_redirect(request: Request):
    """Toggle a storage/hybrid bit from dashboard; redirect back with status."""
    try:
        form = await request.form()
        bit_index_raw = form.get("bit_index")
        on = (form.get("on") or "true").strip().lower() in ("1", "true", "on", "yes")
        register = (form.get("register") or "storage").strip().lower()
        if bit_index_raw is None:
            return RedirectResponse(url="/?error=missing_bit", status_code=303)
        try:
            bit_index = int(bit_index_raw)
        except (TypeError, ValueError):
            return RedirectResponse(url="/?error=invalid_bit", status_code=303)
        ok = False
        error = None
        try:
            loop = asyncio.get_event_loop()
            timeout = _control_write_timeout(register, bit_index, on)
            ok, error = await asyncio.wait_for(
                loop.run_in_executor(None, lambda: _apply_control_change(register, bit_index, on)),
                timeout=timeout,
            )
        except asyncio.TimeoutError:
            logger.warning("Modbus write timed out (dashboard)")
            return RedirectResponse(url="/?error=timeout", status_code=303)
        return RedirectResponse(url="/?saved=1" if ok else f"/?error={error or 'write_failed'}", status_code=303)
    except Exception as e:
        logger.exception("POST /: %s", e)
        return RedirectResponse(url="/?error=server_error", status_code=303)


@app.get("/grid-status", response_class=HTMLResponse)
async def grid_status_page(request: Request):
    """Dedicated grid-status page: generation today (Solis + Tabuchi) and battery remaining/runtime."""
    data = _solis_cache_first().get("data", {}) or {}
    tabuchi_kwh = get_tabuchi_today_pv_kwh()
    solis_today = (data.get("energy_today_pv_kWh") or 0) if isinstance(data.get("energy_today_pv_kWh"), (int, float)) else 0
    grid_status = {
        "solis_today_pv_kWh": solis_today,
        "tabuchi_today_pv_kWh": tabuchi_kwh,
        "total_today_pv_kWh": solis_today + tabuchi_kwh,
    }
    ctx = _page_ctx(request, data=data, ok=_solis_cache_first().get("ok", False), grid_status=grid_status)
    return templates.TemplateResponse("grid-status.html", ctx)


@app.get("/control", response_class=HTMLResponse)
async def control_page(request: Request):
    data = _solis_cache_first().get("data", {})
    storage_bits = _merge_bits({n: False for n in _STORAGE_BIT_NAMES}, data.get("storage_bits"))
    hybrid_bits = _merge_bits({n: False for n in _HYBRID_BIT_NAMES}, data.get("hybrid_bits"))
    q = request.query_params
    ctx = _page_ctx(
        request,
        data=data,
        storage_bits=storage_bits,
        hybrid_bits=hybrid_bits,
        flash_saved=q.get("saved") == "1",
        flash_error=q.get("error"),
    )
    return templates.TemplateResponse("control.html", ctx)


def _as_bool(value) -> bool:
    if isinstance(value, bool):
        return value
    return str(value or "").strip().lower() in ("1", "true", "yes", "on")


def _bool_from_form(form, key: str) -> bool:
    return _as_bool(form.get(key))


_BATTERY_SOURCE_TYPE_OPTIONS = ("mqtt", "mock", "direct_http")
_HOSTNAME_RE = re.compile(r"^[A-Za-z0-9.-]+$")


def _validate_host(value: str, label: str, *, allow_empty: bool = False) -> str:
    candidate = (value or "").strip()
    if not candidate:
        if allow_empty:
            return ""
        raise ValueError(f"{label} is required.")
    if "://" in candidate or "/" in candidate:
        raise ValueError(f"{label} must be a hostname or IP only (no URL scheme/path).")
    try:
        ipaddress.ip_address(candidate)
        return candidate
    except ValueError:
        pass
    if not _HOSTNAME_RE.fullmatch(candidate):
        raise ValueError(f"{label} must be a valid IPv4/IPv6 address or hostname.")
    labels = candidate.split(".")
    if any(not part or len(part) > 63 or part.startswith("-") or part.endswith("-") for part in labels):
        raise ValueError(f"{label} must be a valid IPv4/IPv6 address or hostname.")
    return candidate.lower()


def _validate_port(value, label: str, *, default: int | None = None) -> int:
    raw = str(value or "").strip()
    if not raw:
        if default is None:
            raise ValueError(f"{label} is required.")
        raw = str(default)
    try:
        port = int(raw)
    except (TypeError, ValueError):
        raise ValueError(f"{label} must be a whole number.") from None
    if port < 1 or port > 65535:
        raise ValueError(f"{label} must be between 1 and 65535.")
    return port


def _validate_float(value, label: str, *, default: float = 0, min_val: float | None = None, max_val: float | None = None) -> float:
    raw = str(value or "").strip()
    if not raw:
        return default
    try:
        v = float(raw)
    except (TypeError, ValueError):
        raise ValueError(f"{label} must be a number.") from None
    if min_val is not None and v < min_val:
        raise ValueError(f"{label} must be at least {min_val}.")
    if max_val is not None and v > max_val:
        raise ValueError(f"{label} must be at most {max_val}.")
    return v


def _validate_modbus_unit(value, label: str) -> int:
    try:
        unit = int(str(value or "").strip() or "0")
    except (TypeError, ValueError):
        raise ValueError(f"{label} must be a whole number.") from None
    if unit < 1 or unit > 247:
        raise ValueError(f"{label} must be between 1 and 247.")
    return unit


def _validate_topic(value: str, label: str, *, allow_empty: bool = False) -> str:
    topic = (value or "").strip()
    if not topic:
        if allow_empty:
            return ""
        raise ValueError(f"{label} is required.")
    if any(ch.isspace() for ch in topic):
        raise ValueError(f"{label} must not contain whitespace.")
    return topic


def _validate_source_type(value: str, label: str) -> str:
    source_type = (value or "").strip().lower()
    if source_type not in _BATTERY_SOURCE_TYPE_OPTIONS:
        raise ValueError(f"{label} must be one of: {', '.join(_BATTERY_SOURCE_TYPE_OPTIONS)}.")
    return source_type


def _normalize_base_path(value: str) -> str:
    base_path = (value or "").strip()
    if not base_path:
        return "/"
    if "://" in base_path:
        raise ValueError("Direct HTTP base path must not include a URL scheme.")
    return base_path if base_path.startswith("/") else f"/{base_path}"


def _settings_values(overrides: dict | None = None) -> dict:
    saved = load_settings()
    values = {
        "solis_host": saved.get("solis_host", get_solis_host()),
        "solis_port": int(saved.get("solis_port", get_solis_port())),
        "solis_modbus_unit": int(saved.get("solis_modbus_unit", get_solis_modbus_unit())),
        "solark1_host": saved.get("solark1_host", get_solark1_host()),
        "solark1_port": int(saved.get("solark1_port", get_solark1_port())),
        "solark1_http_port": int(saved.get("solark1_http_port", get_solark1_http_port())),
        "solark1_modbus_unit": int(saved.get("solark1_modbus_unit", get_solark1_modbus_unit())),
        "solark2_modbus_unit": int(saved.get("solark2_modbus_unit", get_solark2_modbus_unit())),
        "esphome_solark_host": saved.get("esphome_solark_host", get_esphome_solark_host()),
        "esphome_solark_port": int(saved.get("esphome_solark_port", get_esphome_solark_port())),
        "mqtt_host": saved.get("mqtt_host", MQTT_HOST or "10.10.53.92"),
        "mqtt_port": int(saved.get("mqtt_port", MQTT_PORT or 1883)),
        "solark_mqtt_topic": saved.get("solark_mqtt_topic", SOLARK_MQTT_TOPIC or "solar/solark"),
        "solark_soc_automation_enabled": _as_bool(saved.get("solark_soc_automation_enabled", get_solark_soc_automation_enabled())),
        "tesla_source_enabled": _as_bool(saved.get("tesla_source_enabled", True)),
        "tesla_source_type": str(saved.get("tesla_source_type", "mqtt") or "mqtt").strip().lower(),
        "tesla_source_host": saved.get("tesla_source_host", ""),
        "tesla_source_port": int(saved.get("tesla_source_port", 80) or 80),
        "tesla_source_base_path": saved.get("tesla_source_base_path", "/"),
        "tesla_source_info_topic": saved.get("tesla_source_info_topic", "BE/info"),
        "tesla_source_spec_topic": saved.get("tesla_source_spec_topic", "BE/spec_data"),
        "tesla_source_balancing_topic": saved.get("tesla_source_balancing_topic", "BE/balancing_data"),
        "ruxiu_source_enabled": _as_bool(saved.get("ruxiu_source_enabled", True)),
        "ruxiu_source_type": str(saved.get("ruxiu_source_type", "mock") or "mock").strip().lower(),
        "ruxiu_source_host": saved.get("ruxiu_source_host", ""),
        "ruxiu_source_port": int(saved.get("ruxiu_source_port", 80) or 80),
        "ruxiu_source_base_path": saved.get("ruxiu_source_base_path", "/"),
        "ruxiu_source_info_topic": saved.get("ruxiu_source_info_topic", ""),
        "ruxiu_source_spec_topic": saved.get("ruxiu_source_spec_topic", ""),
        "ruxiu_source_balancing_topic": saved.get("ruxiu_source_balancing_topic", ""),
        "tabuchi_today_pv_kwh": float(saved.get("tabuchi_today_pv_kwh", get_tabuchi_today_pv_kwh())),
    }
    if overrides:
        values.update(overrides)
    return values


def _settings_form_overrides(form) -> dict:
    return {
        "solis_host": (form.get("solis_host") or "").strip(),
        "solis_port": (form.get("solis_port") or "").strip(),
        "solis_modbus_unit": (form.get("solis_modbus_unit") or "").strip(),
        "solark1_host": (form.get("solark1_host") or "").strip(),
        "solark1_port": (form.get("solark1_port") or "").strip(),
        "solark1_http_port": (form.get("solark1_http_port") or "").strip(),
        "solark1_modbus_unit": (form.get("solark1_modbus_unit") or "").strip(),
        "solark2_modbus_unit": (form.get("solark2_modbus_unit") or "").strip(),
        "esphome_solark_host": (form.get("esphome_solark_host") or "").strip(),
        "esphome_solark_port": (form.get("esphome_solark_port") or "").strip(),
        "mqtt_host": (form.get("mqtt_host") or "").strip(),
        "mqtt_port": (form.get("mqtt_port") or "").strip(),
        "solark_mqtt_topic": (form.get("solark_mqtt_topic") or "").strip(),
        "solark_soc_automation_enabled": _bool_from_form(form, "solark_soc_automation_enabled"),
        "tesla_source_enabled": _bool_from_form(form, "tesla_source_enabled"),
        "tesla_source_type": (form.get("tesla_source_type") or "mqtt").strip().lower(),
        "tesla_source_host": (form.get("tesla_source_host") or "").strip(),
        "tesla_source_port": (form.get("tesla_source_port") or "").strip(),
        "tesla_source_base_path": (form.get("tesla_source_base_path") or "/").strip(),
        "tesla_source_info_topic": (form.get("tesla_source_info_topic") or "").strip(),
        "tesla_source_spec_topic": (form.get("tesla_source_spec_topic") or "").strip(),
        "tesla_source_balancing_topic": (form.get("tesla_source_balancing_topic") or "").strip(),
        "ruxiu_source_enabled": _bool_from_form(form, "ruxiu_source_enabled"),
        "ruxiu_source_type": (form.get("ruxiu_source_type") or "mock").strip().lower(),
        "ruxiu_source_host": (form.get("ruxiu_source_host") or "").strip(),
        "ruxiu_source_port": (form.get("ruxiu_source_port") or "").strip(),
        "ruxiu_source_base_path": (form.get("ruxiu_source_base_path") or "/").strip(),
        "ruxiu_source_info_topic": (form.get("ruxiu_source_info_topic") or "").strip(),
        "ruxiu_source_spec_topic": (form.get("ruxiu_source_spec_topic") or "").strip(),
        "ruxiu_source_balancing_topic": (form.get("ruxiu_source_balancing_topic") or "").strip(),
        "tabuchi_today_pv_kwh": (form.get("tabuchi_today_pv_kwh") or "").strip(),
    }


def _validate_settings(form) -> dict:
    overrides = _settings_form_overrides(form)
    data = {
        "solis_host": _validate_host(overrides["solis_host"], "Solis IP / host"),
        "solis_port": _validate_port(overrides["solis_port"], "Solis port", default=502),
        "solis_modbus_unit": _validate_modbus_unit(overrides["solis_modbus_unit"], "Solis Modbus ID"),
        "solark1_host": _validate_host(overrides["solark1_host"], "Solark board IP / host", allow_empty=True),
        "solark1_port": _validate_port(overrides["solark1_port"], "Solark Modbus port", default=502),
        "solark1_http_port": _validate_port(overrides["solark1_http_port"], "Solark HTTP port", default=80),
        "solark1_modbus_unit": _validate_modbus_unit(overrides["solark1_modbus_unit"], "Solark1 Modbus ID"),
        "solark2_modbus_unit": _validate_modbus_unit(overrides["solark2_modbus_unit"], "Solark2 Modbus ID"),
        "esphome_solark_host": _validate_host(overrides["esphome_solark_host"], "ESPHome IP / host", allow_empty=True),
        "esphome_solark_port": _validate_port(overrides["esphome_solark_port"], "ESPHome port", default=80),
        "mqtt_host": _validate_host(overrides["mqtt_host"], "MQTT broker IP / host"),
        "mqtt_port": _validate_port(overrides["mqtt_port"], "MQTT broker port", default=1883),
        "solark_mqtt_topic": _validate_topic(overrides["solark_mqtt_topic"], "Solark MQTT topic", allow_empty=True),
        "solark_soc_automation_enabled": overrides["solark_soc_automation_enabled"],
        "tesla_source_enabled": overrides["tesla_source_enabled"],
        "tesla_source_type": _validate_source_type(overrides["tesla_source_type"], "Tesla source mode"),
        "tesla_source_host": _validate_host(overrides["tesla_source_host"], "Tesla direct HTTP host", allow_empty=True),
        "tesla_source_port": _validate_port(overrides["tesla_source_port"], "Tesla direct HTTP port", default=80),
        "tesla_source_base_path": _normalize_base_path(overrides["tesla_source_base_path"]),
        "tesla_source_info_topic": _validate_topic(overrides["tesla_source_info_topic"], "Tesla MQTT info topic", allow_empty=True),
        "tesla_source_spec_topic": _validate_topic(overrides["tesla_source_spec_topic"], "Tesla MQTT cell topic", allow_empty=True),
        "tesla_source_balancing_topic": _validate_topic(overrides["tesla_source_balancing_topic"], "Tesla MQTT balancing topic", allow_empty=True),
        "ruxiu_source_enabled": overrides["ruxiu_source_enabled"],
        "ruxiu_source_type": _validate_source_type(overrides["ruxiu_source_type"], "Ruxiu source mode"),
        "ruxiu_source_host": _validate_host(overrides["ruxiu_source_host"], "Ruxiu direct HTTP host", allow_empty=True),
        "ruxiu_source_port": _validate_port(overrides["ruxiu_source_port"], "Ruxiu direct HTTP port", default=80),
        "ruxiu_source_base_path": _normalize_base_path(overrides["ruxiu_source_base_path"]),
        "ruxiu_source_info_topic": _validate_topic(overrides["ruxiu_source_info_topic"], "Ruxiu MQTT info topic", allow_empty=True),
        "ruxiu_source_spec_topic": _validate_topic(overrides["ruxiu_source_spec_topic"], "Ruxiu MQTT cell topic", allow_empty=True),
        "ruxiu_source_balancing_topic": _validate_topic(overrides["ruxiu_source_balancing_topic"], "Ruxiu MQTT balancing topic", allow_empty=True),
        "tabuchi_today_pv_kwh": _validate_float(overrides["tabuchi_today_pv_kwh"], "Tabuchi today PV (kWh)", default=3.0, min_val=0, max_val=100),
    }

    if data["tesla_source_enabled"] and data["tesla_source_type"] == "mqtt":
        if not data["tesla_source_info_topic"]:
            raise ValueError("Tesla MQTT info topic is required when Tesla source mode is mqtt.")
        if not data["tesla_source_spec_topic"]:
            raise ValueError("Tesla MQTT cell topic is required when Tesla source mode is mqtt.")
    if data["tesla_source_enabled"] and data["tesla_source_type"] == "direct_http" and not data["tesla_source_host"]:
        raise ValueError("Tesla direct HTTP host is required when Tesla source mode is direct_http.")

    if data["ruxiu_source_enabled"] and data["ruxiu_source_type"] == "mqtt" and not data["ruxiu_source_spec_topic"]:
        raise ValueError("Ruxiu MQTT cell topic is required when Ruxiu source mode is mqtt.")
    if data["ruxiu_source_enabled"] and data["ruxiu_source_type"] == "direct_http" and not data["ruxiu_source_host"]:
        raise ValueError("Ruxiu direct HTTP host is required when Ruxiu source mode is direct_http.")

    return data


def _format_freshness(ms) -> str:
    if ms is None:
        return "—"
    if ms < 1000:
        return "<1s"
    seconds = int(ms / 1000)
    if seconds < 60:
        return f"{seconds}s"
    minutes, seconds = divmod(seconds, 60)
    if minutes < 60:
        return f"{minutes}m {seconds}s"
    hours, minutes = divmod(minutes, 60)
    return f"{hours}h {minutes}m"


def _format_endpoint_text(source: dict) -> str:
    endpoint = source.get("endpoint") or {}
    kind = endpoint.get("kind")
    if kind == "mqtt":
        topics = [endpoint.get("infoTopic"), endpoint.get("specTopic"), endpoint.get("balancingTopic")]
        topics = [topic for topic in topics if topic]
        return ", ".join(topics) if topics else "MQTT topics not configured"
    if kind == "direct_http":
        host = endpoint.get("host") or "(not set)"
        port = endpoint.get("port") or "—"
        base_path = endpoint.get("basePath") or "/"
        return f"{host}:{port}{base_path}"
    return "—"


def _load_battery_source_statuses():
    try:
        req = UrllibRequest("http://127.0.0.1:3008/api/battery-sources", headers={"User-Agent": "solis-s6-ui"})
        with urlopen(req, timeout=2.5) as resp:
            payload = json.loads(resp.read().decode("utf-8", "replace"))
        sources = payload.get("sources") or {}
        rows = []
        for battery in ("tesla", "ruxiu"):
            source = dict(sources.get(battery) or {})
            if not source:
                continue
            status = str(source.get("status") or "unknown")
            if status in ("live", "healthy", "mock", "historical", "fallback"):
                status_class = "status-ok"
            elif status in ("stale", "warning", "disabled"):
                status_class = "muted"
            else:
                status_class = "status-fail"
            source["status_class"] = status_class
            source["freshness_text"] = _format_freshness(source.get("freshnessMs"))
            source["endpoint_text"] = _format_endpoint_text(source)
            source["error_text"] = source.get("lastError") or source.get("error") or "—"
            rows.append(source)
        return rows, None
    except URLError as exc:
        return [], f"Could not reach battery-dashboard source status API on :3008: {exc.reason}"
    except Exception as exc:
        return [], f"Could not load battery-source status from :3008: {exc}"


def _build_settings_context(request: Request, *, form_overrides: dict | None = None, flash_error: str | None = None) -> dict:
    values = _settings_values(form_overrides)
    statuses, statuses_error = _load_battery_source_statuses()
    mqtt_runtime_restart_pending = (
        (values.get("mqtt_host") or "") != (MQTT_HOST or "")
        or int(values.get("mqtt_port") or 0) != int(MQTT_PORT or 0)
        or (values.get("solark_mqtt_topic") or "") != (SOLARK_MQTT_TOPIC or "")
    )
    return _page_ctx(
        request,
        flash_error=flash_error,
        mqtt_enabled=bool(values.get("mqtt_host")),
        runtime_mqtt_host=MQTT_HOST or "(disabled)",
        runtime_mqtt_port=MQTT_PORT if MQTT_HOST else None,
        runtime_solark_mqtt_topic=SOLARK_MQTT_TOPIC,
        battery_source_statuses=statuses,
        battery_source_status_error=statuses_error,
        mqtt_runtime_restart_pending=mqtt_runtime_restart_pending,
        source_type_options=_BATTERY_SOURCE_TYPE_OPTIONS,
        **values,
    )


@app.get("/api/settings")
async def api_get_settings():
    """Return current Solis/Solark/MQTT/battery-source settings (for UI or scripting)."""
    return _settings_values()


@app.get("/settings", response_class=HTMLResponse)
async def settings_page(request: Request):
    return templates.TemplateResponse("settings.html", _build_settings_context(request))


@app.post("/settings")
async def settings_save(request: Request):
    """Save integration settings with validation; render errors inline for operators."""
    form = await request.form()
    try:
        data = _validate_settings(form)
        current = load_settings()
        current.update(data)
        save_settings(current)
        return RedirectResponse(url="/settings?saved=1", status_code=303)
    except ValueError as exc:
        logger.warning("settings save validation: %s", exc)
        ctx = _build_settings_context(request, form_overrides=_settings_form_overrides(form), flash_error=str(exc))
        return templates.TemplateResponse("settings.html", ctx, status_code=400)
    except Exception as exc:
        logger.exception("settings save: %s", exc)
        ctx = _build_settings_context(
            request,
            form_overrides=_settings_form_overrides(form),
            flash_error=f"Save failed: {exc}",
        )
        return templates.TemplateResponse("settings.html", ctx, status_code=500)


@app.post("/settings/automation")
async def settings_automation_toggle(request: Request):
    """Toggle Solark SOC automation on/off; redirect back to dashboard."""
    try:
        form = await request.form()
        enabled = form.get("enabled") in ("1", "on", "true", "yes")
        current = load_settings()
        current["solark_soc_automation_enabled"] = enabled
        save_settings(current)
        return RedirectResponse(url=f"/?automation={'on' if enabled else 'off'}", status_code=303)
    except Exception as e:
        logger.exception("automation toggle: %s", e)
        return RedirectResponse(url="/?error=automation_failed", status_code=303)


@app.get("/api/automation")
async def api_get_automation():
    """Return whether Solark SOC automation is enabled."""
    return {"enabled": get_solark_soc_automation_enabled()}


@app.post("/api/automation")
async def api_set_automation(request: Request):
    """Set Solark SOC automation on/off. Body: {"enabled": true|false}."""
    try:
        body = await request.json()
        enabled = bool(body.get("enabled", True))
        current = load_settings()
        current["solark_soc_automation_enabled"] = enabled
        save_settings(current)
        return {"ok": True, "enabled": get_solark_soc_automation_enabled()}
    except Exception as e:
        logger.exception("api automation: %s", e)
        return JSONResponse({"ok": False, "error": str(e)}, status_code=500)


@app.get("/api/power-limit")
async def api_get_power_limit():
    """Return current Solis active power limit state."""
    loop = asyncio.get_event_loop()
    try:
        result = await asyncio.wait_for(
            loop.run_in_executor(None, get_active_power_limit),
            timeout=10,
        )
        return result
    except Exception as e:
        return {"ok": False, "error": str(e)}


@app.post("/api/power-limit")
async def api_set_power_limit(request: Request):
    """
    Set Solis active power output limit (0–100%).
    Body: {"limit_pct": 0}   → curtail to 0W
    Body: {"limit_pct": 100} → disable limit (full output)
    """
    try:
        body = await request.json()
        limit_pct = float(body.get("limit_pct", 100))
        if not (0.0 <= limit_pct <= 100.0):
            return JSONResponse({"ok": False, "error": "limit_pct must be 0–100"}, status_code=400)
        loop = asyncio.get_event_loop()
        ok = await asyncio.wait_for(
            loop.run_in_executor(None, lambda: set_active_power_limit(limit_pct)),
            timeout=15,
        )
        return {"ok": ok, "limit_pct": limit_pct}
    except asyncio.TimeoutError:
        return JSONResponse({"ok": False, "error": "Modbus timeout"}, status_code=504)
    except Exception as e:
        logger.exception("api set power limit: %s", e)
        return JSONResponse({"ok": False, "error": str(e)}, status_code=500)


@app.post("/api/grid-charge")
async def api_set_grid_charge(request: Request):
    """
    Enable grid charge at full 11.4 kW. Sets 43110 BIT05=0, 43117, 43130, 43027,
    43132=2, 43128 for forced import. Re-run every ~4 min (dead-man) or enable cron.
    Body: {"import_watts": 11400, "charge_limit_watts": 11400, "max_amps": 70}
    """
    try:
        try:
            body = await request.json()
        except Exception:
            body = {}
        import_w = int(body.get("import_watts", 11400))
        charge_limit = int(body.get("charge_limit_watts", 11400))
        max_amps = float(body.get("max_amps", 70))
        loop = asyncio.get_event_loop()
        result = await asyncio.wait_for(
            loop.run_in_executor(None, lambda: set_grid_charge_limits(import_w, charge_limit, max_amps)),
            timeout=15,
        )
        return result
    except asyncio.TimeoutError:
        return JSONResponse({"ok": False, "message": "Modbus timeout", "writes": []}, status_code=504)
    except Exception as e:
        logger.exception("api grid charge: %s", e)
        return JSONResponse({"ok": False, "message": str(e), "writes": []}, status_code=500)


@app.get("/api/power-control")
async def api_get_power_control():
    """Return current power control state (mode, watts). Persisted; background refreshes Solis every 2 min."""
    return _load_power_control()


@app.post("/api/power-control")
async def api_set_power_control(request: Request):
    """
    Set power control mode. Persists to power_control.json. Background task refreshes Solis every 2 min.
    Body: {"mode": "import"|"export"|"off", "watts": 0-11400}
    - import: charge battery from grid (watts = target import, e.g. 11400 for 11.4 kW)
    - export: force power to grid (watts = target export, e.g. 3000 for 3 kW)
    - off: stop remote control, inverter follows normal mode
    """
    try:
        body = await request.json()
    except Exception:
        body = {}
    mode = str(body.get("mode", "off")).lower()
    if mode not in ("off", "import", "export"):
        return JSONResponse({"ok": False, "error": "mode must be import, export, or off"}, status_code=400)
    watts = int(body.get("watts", 0))
    watts = max(0, min(11400, watts))
    state = {"mode": mode, "watts": watts}
    _save_power_control(state)
    # Apply immediately (background will keep refreshing)
    loop = asyncio.get_event_loop()
    try:
        if mode == "off":
            await asyncio.wait_for(loop.run_in_executor(None, set_power_control_off), timeout=_MODBUS_WRITE_TIMEOUT)
        elif mode == "import" and watts > 0:
            await asyncio.wait_for(
                loop.run_in_executor(
                    None,
                    lambda: set_grid_charge_limits(import_watts=watts, charge_limit_watts=watts, max_amps=70),
                ),
                timeout=_MODBUS_WRITE_TIMEOUT,
            )
        elif mode == "export" and watts > 0:
            await asyncio.wait_for(
                loop.run_in_executor(None, lambda: set_export_target(watts)),
                timeout=_MODBUS_WRITE_TIMEOUT,
            )
    except asyncio.TimeoutError:
        pass  # State saved; background will retry
    except Exception as e:
        logger.warning("power_control immediate apply: %s", e)
    return {"ok": True, "mode": mode, "watts": watts}


@app.get("/debug", response_class=HTMLResponse)
async def debug_page(request: Request):
    """Modbus debug stream: live log of reads/writes and errors."""
    if not _debug_available:
        reason = (_debug_unavailable_reason or "module failed to load").replace("<", "&lt;").replace(">", "&gt;")
        return HTMLResponse(
            "<!DOCTYPE html><html><head><title>Debug</title></head><body>"
            f"<p>Debug stream unavailable: {reason}</p>"
            "<p><a href='/'>Dashboard</a></p></body></html>",
            status_code=503,
        )
    try:
        return templates.TemplateResponse("debug.html", _page_ctx(request))
    except Exception as e:
        logger.exception("debug page: %s", e)
        return HTMLResponse("<p>Error loading debug page.</p><a href='/'>Dashboard</a>", status_code=500)


@app.get("/api/debug/modbus")
async def api_debug_modbus():
    """Return recent Modbus log lines (from solis_modbus logger) for debug stream."""
    if not _debug_available or debug_get_lines is None:
        return JSONResponse({"lines": [], "error": "debug unavailable"}, status_code=503)
    try:
        lines = debug_get_lines()
        return {"lines": [{"ts": ts, "level": level, "msg": msg} for ts, level, msg in lines]}
    except Exception as e:
        logger.exception("api_debug_modbus: %s", e)
        return {"lines": [], "error": str(e)}


@app.post("/api/debug/modbus/clear")
async def api_debug_modbus_clear():
    """Clear the Modbus debug ring buffer."""
    if not _debug_available or debug_clear is None:
        return JSONResponse({"ok": False, "error": "debug unavailable"}, status_code=503)
    try:
        debug_clear()
        return {"ok": True}
    except Exception:
        return {"ok": False}


# API
@app.get("/api/health")
async def api_health():
    """Lightweight health check (no Modbus). Use to confirm app is running."""
    return {"ok": True, "version": APP_VERSION}


@app.get("/api/status")
async def api_status():
    """Report which components loaded (for debugging broken startup)."""
    return {
        "ok": True,
        "version": APP_VERSION,
        "modbus_available": _modbus_available,
        "debug_available": _debug_available,
    }


@app.get("/api/version")
async def api_version():
    """Return UI version (from VERSION file or SOLIS_UI_VERSION env)."""
    return {"version": APP_VERSION}


@app.get("/api/dashboard")
async def api_dashboard():
    solis_inverters = _build_solis_inverters()
    first_ent = solis_inverters[0] if solis_inverters else {"data": {}, "ok": False, "ts": 0}
    data = first_ent.get("data", {}) or {}
    storage_bits = _merge_bits({n: False for n in _STORAGE_BIT_NAMES}, data.get("storage_bits"))
    hybrid_bits  = _merge_bits({n: False for n in _HYBRID_BIT_NAMES},  data.get("hybrid_bits"))
    out = dict(data)
    out["storage_mode"] = _storage_mode_label(storage_bits)
    out["storage_bits"] = storage_bits
    out["hybrid_bits"]  = hybrid_bits
    solark_data = _solark_cache.get("data", {}) or {}
    tabuchi_kwh = get_tabuchi_today_pv_kwh()
    solis_today = (data.get("energy_today_pv_kWh") or 0) if isinstance(data.get("energy_today_pv_kWh"), (int, float)) else 0
    return {
        "ok": first_ent.get("ok"),
        "data": out,
        "ts": first_ent.get("ts"),
        "solis": {"data": out, "ok": first_ent.get("ok"), "ts": first_ent.get("ts")},
        "solis_inverters": solis_inverters,
        "solark": {
            "data": solark_data,
            "ok": _solark_cache.get("ok"),
            "ts": _solark_cache.get("ts"),
            "source": _solark_cache.get("source"),
            "last_error": _solark_cache.get("last_error"),
        },
        "automation": {
            "enabled": get_solark_soc_automation_enabled(),
            "solark_soc_self_use_active": _solark_auto_self_use_active,
        },
        "grid_status": {
            "tabuchi_today_pv_kWh": tabuchi_kwh,
            "solis_today_pv_kWh": solis_today,
            "total_today_pv_kWh": solis_today + tabuchi_kwh,
        },
    }


@app.get("/api/grid-status")
async def api_grid_status():
    """Generation today for grid-status page: Solis today PV + Tabuchi static. Tabuchi value is configurable in Settings (default 3 kWh)."""
    data = _solis_cache_first().get("data", {}) or {}
    solis_today = (data.get("energy_today_pv_kWh") or 0) if isinstance(data.get("energy_today_pv_kWh"), (int, float)) else 0
    tabuchi_kwh = get_tabuchi_today_pv_kwh()
    return {
        "solis_today_pv_kWh": solis_today,
        "tabuchi_today_pv_kWh": tabuchi_kwh,
        "total_today_pv_kWh": solis_today + tabuchi_kwh,
        "note": "Change Tabuchi value in Settings (or TABUCHI_TODAY_PV_KWH env). Default 3.",
    }


@app.get("/api/sensors")
async def api_sensors():
    return _solis_cache_first().get("data", {})


@app.get("/api/debug/register/33035")
async def api_debug_register_33035():
    """Debug: raw value of register 33035 (PV today) and computed kWh. Use when daily PV seems wrong."""
    data = _solis_cache_first().get("data", {}) or {}
    raw = data.get("_raw_33035")
    computed = data.get("energy_today_pv_kWh")
    return {
        "register": 33035,
        "raw_value": raw,
        "computed_kWh": computed,
        "scale": SOLIS_DAILY_PV_SCALE,
        "note": "If raw≈260 but inverter shows 15.2, set SOLIS_DAILY_PV_SCALE=0.585 in .env (or correct ratio)",
    }


@app.get("/api/debug/register/33029-33030")
async def api_debug_register_33029_33030():
    """Debug: raw 33029-33030 (total PV) and computed kWh. S6 uses 0.01 kWh → divisor 100. If total was ~10x too high, app now uses SOLIS_TOTAL_PV_DIVISOR=100."""
    data = _solis_cache_first().get("data", {}) or {}
    raw = data.get("_raw_33029_30")
    computed = data.get("total_pv_energy_kWh")
    return {
        "registers": "33029, 33030",
        "raw_32bit": raw,
        "computed_kWh": computed,
        "note": "SOLIS_TOTAL_PV_DIVISOR=100 (0.01 kWh). Set to 10 if total is 10x too small.",
    }


@app.get("/api/debug/registers/energy")
async def api_debug_registers_energy():
    """Debug: dump energy block 33000 (registers 33029-33040) to find which holds today's PV.
    Per Solis docs: 33029-30=Total, 33031-32=CurrMonth, 33033-34=LastMonth, 33035=Today, 33036=Yesterday.
    If inverter shows 15.2 kWh, look for raw value 152 (0.1 kWh units) in the dump."""
    data = _solis_cache_first().get("data", {}) or {}
    blocks = data.get("raw_blocks", {})
    b0 = blocks.get("33000")
    if not b0 or len(b0) < 41:
        return {"ok": False, "error": "Block 33000 not in cache or too short", "hint": "Poll once, then retry"}
    # Indices 28-40 = registers 33028-33040
    reg_names = {
        28: "33028 (reserved)",
        29: "33029-30 Total PV (hi)",
        30: "33029-30 Total PV (lo)",
        31: "33031-32 Curr Month (hi)",
        32: "33031-32 Curr Month (lo)",
        33: "33033-34 Last Month (hi)",
        34: "33033-34 Last Month (lo)",
        35: "33035 TODAY ← we use this",
        36: "33036 Yesterday",
        37: "33037-38 This Year (hi)",
        38: "33037-38 This Year (lo)",
        39: "33039-40 Last Year (hi)",
        40: "33039-40 Last Year (lo)",
    }
    dump = []
    for i in range(28, 41):
        raw = b0[i] if i < len(b0) else None
        kWh = (raw / 10.0) if raw is not None else None
        dump.append({"idx": i, "reg": 33000 + i, "name": reg_names.get(i), "raw": raw, "kWh_if_0_1": kWh})
    return {
        "ok": True,
        "current_energy_today_pv_kWh": data.get("energy_today_pv_kWh"),
        "registers": dump,
        "note": "If inverter shows 15.2 kWh, find raw=152. That register is the correct one for Today PV.",
    }


@app.get("/api/storage_bits")
async def api_storage_bits():
    return get_storage_control_bits()


@app.get("/api/hybrid_bits")
async def api_hybrid_bits():
    return get_hybrid_control_bits()


@app.post("/api/storage_toggle")
async def api_storage_toggle(request: Request):
    """Toggle a storage bit (43110). Work-mode bits are applied with one-hot exclusions."""
    ct = request.headers.get("content-type", "").strip()
    if ct.startswith("application/json"):
        try:
            body = await request.json()
            bit_index = body.get("bit_index")
            on = body.get("on", True)
            if isinstance(on, bool):
                on = "true" if on else "false"
            else:
                on = str(on) if on is not None else "true"
        except Exception:
            return JSONResponse({"ok": False, "error": "Invalid JSON"}, status_code=400)
    else:
        form = await request.form()
        bit_index = form.get("bit_index")
        on = form.get("on", "true")
        if on is None:
            on = "true"
    if bit_index is None:
        return JSONResponse({"ok": False, "error": "bit_index required"}, status_code=400)
    try:
        bit_index = int(bit_index)
    except (TypeError, ValueError):
        return JSONResponse({"ok": False, "error": "bit_index must be integer"}, status_code=400)
    if on is None:
        on = "true"
    if bit_index < 0 or bit_index > 11:
        return JSONResponse({"ok": False, "error": "bit_index 0..11"}, status_code=400)
    on_bool = str(on).strip().lower() in ("1", "true", "on", "yes")
    ok, error = _apply_control_change("storage", bit_index, on_bool)
    return {"ok": ok, "error": error, "bit_index": bit_index, "on": on_bool}


@app.post("/api/hybrid_toggle")
async def api_hybrid_toggle(request: Request):
    """Toggle a hybrid bit (43483). Accepts application/json or form: bit_index (0–7), on (true/false)."""
    ct = request.headers.get("content-type", "").strip()
    if ct.startswith("application/json"):
        try:
            body = await request.json()
            bit_index = body.get("bit_index")
            on = body.get("on", True)
            if isinstance(on, bool):
                on = "true" if on else "false"
            else:
                on = str(on) if on is not None else "true"
        except Exception:
            return JSONResponse({"ok": False, "error": "Invalid JSON"}, status_code=400)
    else:
        form = await request.form()
        bit_index = form.get("bit_index")
        on = form.get("on", "true")
        if on is None:
            on = "true"
    if bit_index is None:
        return JSONResponse({"ok": False, "error": "bit_index required"}, status_code=400)
    try:
        bit_index = int(bit_index)
    except (TypeError, ValueError):
        return JSONResponse({"ok": False, "error": "bit_index must be integer"}, status_code=400)
    if on is None:
        on = "true"
    if bit_index < 0 or bit_index > 7:
        return JSONResponse({"ok": False, "error": "bit_index 0..7"}, status_code=400)
    on_bool = str(on).strip().lower() in ("1", "true", "on", "yes")
    ok, error = _apply_control_change("hybrid", bit_index, on_bool)
    return {"ok": ok, "error": error, "bit_index": bit_index, "on": on_bool}


@app.post("/control")
async def control_toggle_redirect(request: Request):
    """Toggle a bit on 43110 (storage) or 43483 (hybrid), or apply a preset; redirect back with status."""
    try:
        form = await request.form()
        preset = form.get("preset")
        if preset == "use_all_solar":
            try:
                loop = asyncio.get_event_loop()
                ok = await asyncio.wait_for(
                    loop.run_in_executor(None, apply_use_all_solar_preset),
                    timeout=_MODBUS_WRITE_TIMEOUT,
                )
            except asyncio.TimeoutError:
                logger.warning("Modbus preset timed out")
                return RedirectResponse(url="/control?error=timeout", status_code=303)
            return RedirectResponse(
                url="/control?saved=1" if ok else "/control?error=preset_failed",
                status_code=303,
            )
        bit_index_raw = form.get("bit_index")
        if bit_index_raw is None:
            return RedirectResponse(url="/control", status_code=303)
        try:
            bit_index = int(bit_index_raw)
        except (TypeError, ValueError):
            return RedirectResponse(url="/control?error=invalid_bit", status_code=303)
        on_bool = (form.get("on") or "true").strip().lower() in ("1", "true", "on", "yes")
        register = (form.get("register") or "storage").strip().lower()
        ok = False
        error = None
        try:
            loop = asyncio.get_event_loop()
            timeout = _control_write_timeout(register, bit_index, on_bool)
            ok, error = await asyncio.wait_for(
                loop.run_in_executor(None, lambda: _apply_control_change(register, bit_index, on_bool)),
                timeout=timeout,
            )
        except asyncio.TimeoutError:
            logger.warning("Modbus write timed out (control)")
            return RedirectResponse(url="/control?error=timeout", status_code=303)
        return RedirectResponse(
            url="/control?saved=1" if ok else f"/control?error={error or 'write_failed'}",
            status_code=303,
        )
    except Exception as e:
        logger.exception("POST /control: %s", e)
        return RedirectResponse(url="/control?error=server_error", status_code=303)


@app.post("/api/control")
async def api_control(request: Request):
    """JSON control endpoint — same logic as POST /control but returns {ok, error} instead of redirect."""
    try:
        body = await request.json()
        preset = body.get("preset")
        if preset == "use_all_solar":
            loop = asyncio.get_event_loop()
            try:
                ok = await asyncio.wait_for(
                    loop.run_in_executor(None, apply_use_all_solar_preset),
                    timeout=_MODBUS_WRITE_TIMEOUT,
                )
            except asyncio.TimeoutError:
                return {"ok": False, "error": "timeout"}
            return {"ok": bool(ok)}
        bit_index = int(body.get("bit_index", -1))
        on_bool = bool(body.get("on", True))
        register = str(body.get("register", "storage")).lower()
        ok = False
        error = None
        loop = asyncio.get_event_loop()
        try:
            timeout = _control_write_timeout(register, bit_index, on_bool)
            ok, error = await asyncio.wait_for(
                loop.run_in_executor(None, lambda: _apply_control_change(register, bit_index, on_bool)),
                timeout=timeout,
            )
        except asyncio.TimeoutError:
            return {"ok": False, "error": "timeout"}
        return {"ok": bool(ok), "error": error}
    except Exception as e:
        logger.exception("POST /api/control: %s", e)
        return {"ok": False, "error": "server_error"}


_ENVOY_API_URL = "http://localhost:3004/api/envoy/debug"
_ENVOY_FETCH_TIMEOUT = 22  # per-fetch socket timeout (port 3004 polls real Envoys, takes ~15s)

# Background cache — refreshed every 30s so page/API requests return instantly.
_envoy_cache: dict = {"data": {}, "ts": 0.0, "error": None}
_ENVOY_CACHE_TTL = 60  # seconds before cache is considered stale for display purposes


def _fetch_envoy_data_sync() -> dict:
    """Blocking fetch of envoy debug API (run in executor or background thread)."""
    import urllib.request
    with urllib.request.urlopen(_ENVOY_API_URL, timeout=_ENVOY_FETCH_TIMEOUT) as resp:
        return json.loads(resp.read())


def _refresh_envoy_cache() -> None:
    """Called from background thread — fetches envoy data and updates cache."""
    global _envoy_cache
    try:
        data = _fetch_envoy_data_sync()
        _envoy_cache = {"data": data, "ts": time.time(), "error": None}
        logger.debug("envoy cache refreshed")
    except Exception as exc:
        _envoy_cache["error"] = str(exc)
        _envoy_cache["ts"] = time.time()
        logger.warning("envoy cache refresh failed: %s", exc)


async def _background_envoy_refresher() -> None:
    """Refresh the envoy cache every 30s in the background without blocking request handlers."""
    while True:
        loop = asyncio.get_event_loop()
        try:
            await asyncio.wait_for(
                loop.run_in_executor(None, _refresh_envoy_cache),
                timeout=_ENVOY_FETCH_TIMEOUT + 5,
            )
        except asyncio.TimeoutError:
            logger.warning("envoy background refresh timed out")
        except Exception as exc:
            logger.warning("envoy background refresh error: %s", exc)
        await asyncio.sleep(30)


@app.get("/envoys", response_class=HTMLResponse)
async def page_envoys(request: Request):
    """Per-micro-inverter live data from Enphase Envoy gateways (cached, refreshes every 30s)."""
    cached = _envoy_cache
    envoy_data = cached.get("data", {})
    age_s = time.time() - cached.get("ts", 0)
    error: str | None = None

    if not envoy_data and cached.get("error"):
        error = f"Envoy API unavailable: {cached['error']}"
    elif age_s > _ENVOY_CACHE_TTL and not envoy_data:
        error = "Envoy data not yet loaded — refresh in a moment"

    envoys = {k: v for k, v in envoy_data.items() if k.startswith("envoy")}
    total_watts = sum(e.get("production", 0) for e in envoys.values())
    total_active = sum(e.get("active_inverters", 0) for e in envoys.values())
    total_offline = sum(e.get("offline_inverters", 0) for e in envoys.values())
    total_count = sum(len(e.get("inverters", [])) for e in envoys.values())
    ts_raw = envoy_data.get("timestamp", "")
    try:
        ts = datetime.fromisoformat(ts_raw).strftime("%Y-%m-%d %H:%M:%S") if ts_raw else "—"
    except Exception:
        ts = ts_raw

    ctx = _page_ctx(
        request,
        envoys=envoys,
        total_watts=total_watts,
        total_active=total_active,
        total_offline=total_offline,
        total_count=total_count,
        timestamp=ts,
        error=error,
    )
    return templates.TemplateResponse("envoys.html", ctx)


@app.get("/api/envoys")
async def api_envoys():
    """Cached envoy data — per-micro-inverter. Refreshed every 30s in background."""
    cached = _envoy_cache
    age_s = time.time() - cached.get("ts", 0)
    return {**cached["data"], "cache_age_s": round(age_s, 1), "error": cached.get("error")}


# ── Curtailment (HA switch control) ─────────────────────────────────────────

_CURTAIL_SWITCHES = [
    {"key": "iq8",     "entity": HA_SWITCH_IQ8,     "label": "IQ8 Micro-inverters",  "sub": "Envoy 2 · 10.10.53.186 · 14 units"},
    {"key": "mseries", "entity": HA_SWITCH_MSERIES,  "label": "Shed Micro-inverters", "sub": "Envoy 1 · 10.10.53.194 · 12 units"},
    {"key": "tabuchi", "entity": HA_SWITCH_TABUCHI,  "label": "Tabuchi Export",       "sub": "Grid export switch · sonoff_1001204d65_1"},
]


def _ha_headers() -> dict:
    return {"Authorization": f"Bearer {HA_TOKEN}", "Content-Type": "application/json"}


def _fetch_curtailment_sync() -> dict:
    """Blocking: fetch state of all curtailment switches + HA auth status."""
    import urllib.request, urllib.error
    result: dict = {"auth_ok": False, "auth_error": None, "switches": {}}
    if not HA_TOKEN:
        result["auth_error"] = "HA_TOKEN not configured"
        return result
    try:
        req = UrllibRequest(f"{HA_URL}/api/", headers=_ha_headers())
        with urlopen(req, timeout=6) as r:
            body = json.loads(r.read())
        result["auth_ok"] = body.get("message") == "API running."
    except URLError as e:
        result["auth_error"] = f"Cannot reach HA: {e}"
        return result
    except Exception as e:
        result["auth_error"] = f"HA auth check failed: {e}"
        return result

    for sw in _CURTAIL_SWITCHES:
        try:
            req = UrllibRequest(f"{HA_URL}/api/states/{sw['entity']}", headers=_ha_headers())
            with urlopen(req, timeout=6) as r:
                data = json.loads(r.read())
            result["switches"][sw["key"]] = {
                "entity": sw["entity"],
                "label":  sw["label"],
                "sub":    sw["sub"],
                "state":  data.get("state", "unknown"),
                "last_changed": data.get("last_changed", ""),
            }
        except Exception as e:
            result["switches"][sw["key"]] = {
                "entity": sw["entity"],
                "label":  sw["label"],
                "sub":    sw["sub"],
                "state":  "error",
                "error":  str(e),
            }
    return result


def _ha_switch_sync(entity_id: str, turn_on: bool) -> dict:
    """Blocking: call HA service to turn a switch on or off."""
    import urllib.request, urllib.error
    action = "turn_on" if turn_on else "turn_off"
    url = f"{HA_URL}/api/services/switch/{action}"
    payload = json.dumps({"entity_id": entity_id}).encode()
    req = UrllibRequest(url, data=payload, headers=_ha_headers(), method="POST")
    try:
        with urlopen(req, timeout=8) as r:
            r.read()
        return {"ok": True}
    except URLError as e:
        return {"ok": False, "error": str(e)}
    except Exception as e:
        return {"ok": False, "error": str(e)}


@app.get("/curtailment", response_class=HTMLResponse)
async def page_curtailment(request: Request):
    """Curtailment control page — SmartLife/Tuya switch states and on/off buttons."""
    loop = asyncio.get_event_loop()
    try:
        data = await asyncio.wait_for(loop.run_in_executor(None, _fetch_curtailment_sync), timeout=15)
    except asyncio.TimeoutError:
        data = {"auth_ok": False, "auth_error": "HA request timed out", "switches": {}}
    except Exception as exc:
        data = {"auth_ok": False, "auth_error": str(exc), "switches": {}}

    ctx = _page_ctx(
        request,
        auth_ok=data.get("auth_ok", False),
        auth_error=data.get("auth_error"),
        switches=data.get("switches", {}),
        ha_url=HA_URL,
    )
    return templates.TemplateResponse("curtailment.html", ctx)


@app.get("/api/curtailment")
async def api_curtailment_get():
    """JSON: state of all curtailment switches + HA auth status + automation state."""
    loop = asyncio.get_event_loop()
    try:
        data = await asyncio.wait_for(loop.run_in_executor(None, _fetch_curtailment_sync), timeout=15)
    except Exception as exc:
        data = {"auth_ok": False, "auth_error": str(exc), "switches": {}}
    # Append live automation state
    solark = _solark_cache.get("data") or {}
    data["auto"] = {
        "enabled":        get_solark_soc_automation_enabled(),
        "curtail_active": _ha_curtail_active,
        "solis_active":   _solark_auto_self_use_active,
        "soc_pct":        round(int(solark.get("battery_soc_pptt", 0)) / 100.0, 1) if solark.get("battery_soc_pptt") is not None else None,
        "threshold_pct":  SOLARK_SOC_SELF_USE_THRESHOLD_PCT,
        "restore_pct":    SOLARK_SOC_FEEDIN_BELOW_PCT,
    }
    return data


@app.post("/api/curtailment/{key}/{action}")
async def api_curtailment_set(key: str, action: str):
    """Turn a curtailment switch on or off. key=iq8|mseries|tabuchi|all, action=on|off."""
    global _ha_curtail_active, _last_ha_curtail_ts
    if action not in ("on", "off"):
        return JSONResponse({"ok": False, "error": "action must be on or off"}, status_code=400)
    turn_on = action == "on"
    loop = asyncio.get_event_loop()

    # Bulk action: key == "all"
    if key == "all":
        errors = []
        for sw in _CURTAIL_SWITCHES:
            try:
                result = await asyncio.wait_for(
                    loop.run_in_executor(None, lambda e=sw["entity"]: _ha_switch_sync(e, turn_on)),
                    timeout=12,
                )
                if not result.get("ok"):
                    errors.append(f"{sw['key']}: {result.get('error')}")
            except asyncio.TimeoutError:
                errors.append(f"{sw['key']}: timeout")
        # Sync automation flag to manual action
        _ha_curtail_active = not turn_on
        _last_ha_curtail_ts = time.time()
        return {"ok": len(errors) == 0, "errors": errors}

    sw = next((s for s in _CURTAIL_SWITCHES if s["key"] == key), None)
    if not sw:
        return JSONResponse({"ok": False, "error": "unknown switch key"}, status_code=400)
    try:
        result = await asyncio.wait_for(
            loop.run_in_executor(None, lambda: _ha_switch_sync(sw["entity"], turn_on)),
            timeout=12,
        )
    except asyncio.TimeoutError:
        result = {"ok": False, "error": "timeout"}
    # If manually overriding, reset automation active flag so it can re-evaluate next cycle
    if result.get("ok"):
        _last_ha_curtail_ts = time.time()
    return result


if __name__ == "__main__":
    import uvicorn
    from config import HOST, PORT
    uvicorn.run("main:app", host=HOST, port=PORT, reload=False)

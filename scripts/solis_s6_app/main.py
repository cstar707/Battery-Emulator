"""
Solis S6 + Solark UI: FastAPI backend on port 3007.
Dashboard, sensors, storage toggles (43110), settings.
Run: uvicorn main:app --host 0.0.0.0 --port 3007
All optional deps (config, mqtt, modbus, debug) have fallbacks so the app always starts.
"""
from __future__ import annotations

import asyncio
import json
import logging
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
        SOLARK_HTTP_PASSWORD,
        SOLARK_HTTP_USER,
        SOLARK_MQTT_TOPIC,
        SOLARK_SOC_FEEDIN_BELOW_PCT,
        SOLARK_SOC_SELF_USE_THRESHOLD_PCT,
    )
except Exception as e:
    logging.warning("Config import failed, using defaults: %s", e)
    APP_VERSION = "0.0.0"
    INVERTER_LABEL_SOLIS1 = "Solis1"
    INVERTER_LABEL_SOLARK1, INVERTER_LABEL_SOLARK2 = "Solark1", "Solark2"
    MQTT_HOST, POLL_INTERVAL_SEC = "", 5.0
    SOLARK_SOC_SELF_USE_THRESHOLD_PCT, SOLARK_SOC_FEEDIN_BELOW_PCT = 98, 95
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
        get_hybrid_control_bits,
        get_storage_control_bits,
        poll_solis,
        set_hybrid_control_bit,
        set_storage_control_bit,
    )
    _STORAGE_BIT_NAMES, _HYBRID_BIT_NAMES = _SM, _HM
    _modbus_available = True
except Exception as e:
    logging.warning("Modbus unavailable, using stubs: %s", e)
    _modbus_available = False
    poll_solis = _modbus_stub_poll
    set_storage_control_bit = set_hybrid_control_bit = _modbus_stub_return_false
    apply_use_all_solar_preset = _modbus_stub_return_false
    get_storage_control_bits = _modbus_stub_return_dict_bits
    get_hybrid_control_bits = _modbus_stub_return_hybrid_bits

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

# In-memory cache for Solis data (updated by background poll)
_solis_cache: dict = {"data": {}, "ts": 0, "ok": False}
# Solark data: from HTTP (GET /solark_data) and/or MQTT (solar/solark). source = "http" | "mqtt", last_error = str or None.
_solark_cache: dict = {"data": {}, "ts": 0, "ok": False, "source": None, "last_error": None}

# Automation: when Solark SOC >= threshold, switch Solis to self-use; hysteresis below SOLARK_SOC_FEEDIN_BELOW_PCT
_solark_auto_self_use_active: bool = False
_last_solis_auto_switch_ts: float = 0
_AUTO_SWITCH_COOLDOWN_SEC = 300  # don't flip Solis mode more than once per 5 min

# Modbus write timeout (seconds) so a stuck inverter doesn't hang the request
_MODBUS_WRITE_TIMEOUT = 15.0


def _on_solark_mqtt_message(_client, _userdata, msg):
    """Update _solark_cache from MQTT solar/solark message (same JSON as /solark_data)."""
    global _solark_cache
    try:
        payload = msg.payload.decode()
        data = json.loads(payload) if payload else {}
        if data.get("battery_soc_pptt") is not None:
            _solark_cache["data"] = data
            _solark_cache["ts"] = time.time()
            _solark_cache["ok"] = True
            _solark_cache["source"] = "mqtt"
            _solark_cache["last_error"] = None
    except (ValueError, TypeError, json.JSONDecodeError) as e:
        logger.debug("Solark MQTT payload parse: %s", e)


def _run_solark_mqtt_subscriber() -> None:
    """Run MQTT client loop in this thread; subscribe to SOLARK_MQTT_TOPIC and update _solark_cache."""
    if not MQTT_HOST or not SOLARK_MQTT_TOPIC:
        return
    try:
        import paho.mqtt.client as mqtt
        client = mqtt.Client(client_id=f"{MQTT_CLIENT_ID}-solark", protocol=mqtt.MQTTv311)
        if MQTT_USER:
            client.username_pw_set(MQTT_USER, MQTT_PASSWORD or "")
        client.on_message = _on_solark_mqtt_message
        client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
        client.subscribe(SOLARK_MQTT_TOPIC, qos=0)
        logger.info("Solark MQTT subscriber: %s on %s", SOLARK_MQTT_TOPIC, MQTT_HOST)
        client.loop_forever()
    except Exception as e:
        logger.warning("Solark MQTT subscriber failed: %s", e)
        _solark_cache["last_error"] = f"MQTT: {e}"
        _solark_cache["source"] = "mqtt"


def _fetch_solark_data_sync() -> tuple[dict, str | None]:
    """GET /solark_data from Solark board; return (data, None) on success or ({}, error_str) on failure."""
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


def _run_solark_solis_automation(solis_data: dict, solark_data: dict) -> None:
    """If Solark SOC >= threshold, set Solis to self-use; if below hysteresis and we auto-switched, set feed-in."""
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
    storage_bits = (solis_data or {}).get("storage_bits") or {}
    self_use = bool(storage_bits.get("self_use"))
    feed_in = bool(storage_bits.get("feed_in_priority"))
    threshold_pptt = SOLARK_SOC_SELF_USE_THRESHOLD_PCT * 100
    below_pptt = SOLARK_SOC_FEEDIN_BELOW_PCT * 100
    if soc_pptt >= threshold_pptt and (feed_in or not self_use):
        # Switch Solis to self-use (bit 0 on, bit 6 off)
        try:
            if set_storage_control_bit(0, True) and set_storage_control_bit(6, False):
                _solark_auto_self_use_active = True
                _last_solis_auto_switch_ts = now
                logger.info("Solark SOC %.1f%% >= %d%%: Solis switched to self-use", soc_pct, SOLARK_SOC_SELF_USE_THRESHOLD_PCT)
        except Exception as e:
            logger.warning("Automation set self-use: %s", e)
    elif _solark_auto_self_use_active and soc_pptt < below_pptt and self_use:
        # Hysteresis: switch back to feed-in
        try:
            if set_storage_control_bit(6, True) and set_storage_control_bit(0, False):
                _solark_auto_self_use_active = False
                _last_solis_auto_switch_ts = now
                logger.info("Solark SOC %.1f%% < %d%%: Solis switched to feed-in", soc_pct, SOLARK_SOC_FEEDIN_BELOW_PCT)
        except Exception as e:
            logger.warning("Automation set feed-in: %s", e)


def _poll_sync() -> None:
    global _solis_cache, _solark_cache
    try:
        data = poll_solis()
        ts = time.time()
        _solis_cache = {"data": data, "ts": ts, "ok": data.get("ok", False)}
        try:
            publish_solis_sensors(data, ts=ts)
        except Exception as mqtt_e:
            logger.warning("MQTT publish: %s", mqtt_e)
        # Fetch Solark data from board (HTTP); MQTT subscriber may also update _solark_cache
        solark_host = get_solark1_host()
        if solark_host:
            solark_data, http_err = _fetch_solark_data_sync()
            if not http_err and solark_data:
                _solark_cache["data"] = solark_data
                _solark_cache["ts"] = ts
                _solark_cache["ok"] = True
                _solark_cache["source"] = "http"
                _solark_cache["last_error"] = None
                if SOLARK_SOC_SELF_USE_THRESHOLD_PCT > 0:
                    _run_solark_solis_automation(data, solark_data)
            else:
                _solark_cache["last_error"] = http_err
                _solark_cache["source"] = "http"
                if not _solark_cache.get("data") and not _solark_cache.get("ok"):
                    _solark_cache["ts"] = ts
        else:
            if not _solark_cache.get("ok"):
                _solark_cache["last_error"] = "Solark board IP not set (Settings)"
                _solark_cache["source"] = None
                _solark_cache["ts"] = ts
    except Exception as e:
        logger.exception("poll: %s", e)
        _solis_cache = {"data": {}, "ts": time.time(), "ok": False}


async def _background_poller() -> None:
    while True:
        try:
            await asyncio.get_event_loop().run_in_executor(None, _poll_sync)
        except Exception as e:
            logger.exception("background poll: %s", e)
        await asyncio.sleep(max(1, POLL_INTERVAL_SEC))


@asynccontextmanager
async def lifespan(app: FastAPI):
    asyncio.create_task(_background_poller())
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
templates = Jinja2Templates(directory=str(BASE / "templates"))
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
    if storage_bits.get("peak_shaving"):
        extras.append("peak-shaving")
    if extras:
        mode = f"{mode} + {', '.join(extras)}"
    return mode


@app.get("/", response_class=HTMLResponse)
async def index(request: Request):
    data = _solis_cache.get("data", {}) or {}
    storage_bits = _merge_bits({n: False for n in _STORAGE_BIT_NAMES}, data.get("storage_bits"))
    ts = _solis_cache.get("ts", 0) or 0
    last_updated = datetime.fromtimestamp(ts, tz=timezone.utc).strftime("%H:%M:%S UTC") if ts else "—"
    solark_data = _solark_cache.get("data", {}) or {}
    solark_ok = _solark_cache.get("ok", False)
    solark_source = _solark_cache.get("source")
    solark_last_error = _solark_cache.get("last_error")
    q = request.query_params
    ctx = _page_ctx(
        request,
        data=data,
        ok=_solis_cache.get("ok", False),
        storage_bits=storage_bits,
        storage_mode_label=_storage_mode_label(storage_bits),
        last_updated=last_updated,
        solark_data=solark_data,
        solark_ok=solark_ok,
        solark_source=solark_source,
        solark_last_error=solark_last_error,
        solark_auto_self_use=_solark_auto_self_use_active,
        automation_enabled=get_solark_soc_automation_enabled(),
        flash_saved=q.get("saved") == "1",
        flash_error=q.get("error"),
        flash_automation=q.get("automation"),  # "on" or "off" after toggle
    )
    return templates.TemplateResponse(request, "dashboard.html", ctx)


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
        try:
            loop = asyncio.get_event_loop()
            if register == "hybrid" and 0 <= bit_index <= 7:
                ok = await asyncio.wait_for(
                    loop.run_in_executor(None, lambda: set_hybrid_control_bit(bit_index, on)),
                    timeout=_MODBUS_WRITE_TIMEOUT,
                )
            elif register == "storage" and 0 <= bit_index <= 11:
                ok = await asyncio.wait_for(
                    loop.run_in_executor(None, lambda: set_storage_control_bit(bit_index, on)),
                    timeout=_MODBUS_WRITE_TIMEOUT,
                )
        except asyncio.TimeoutError:
            logger.warning("Modbus write timed out (dashboard)")
            return RedirectResponse(url="/?error=timeout", status_code=303)
        return RedirectResponse(url="/?saved=1" if ok else "/?error=write_failed", status_code=303)
    except Exception as e:
        logger.exception("POST /: %s", e)
        return RedirectResponse(url="/?error=server_error", status_code=303)


@app.get("/sensors", response_class=HTMLResponse)
async def sensors_page(request: Request):
    ctx = _page_ctx(request, data=_solis_cache.get("data", {}), ok=_solis_cache.get("ok", False))
    return templates.TemplateResponse(request, "sensors.html", ctx)


@app.get("/control", response_class=HTMLResponse)
async def control_page(request: Request):
    data = _solis_cache.get("data", {})
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
    return templates.TemplateResponse(request, "control.html", ctx)


def _settings_values() -> dict:
    """Current inverter/IP/port/Modbus values for settings form and API."""
    return {
        "solis_host": get_solis_host(),
        "solis_port": get_solis_port(),
        "solis_modbus_unit": get_solis_modbus_unit(),
        "solark1_host": get_solark1_host(),
        "solark1_port": get_solark1_port(),
        "solark1_modbus_unit": get_solark1_modbus_unit(),
        "solark2_modbus_unit": get_solark2_modbus_unit(),
        "solark_soc_automation_enabled": get_solark_soc_automation_enabled(),
    }


@app.get("/api/settings")
async def api_get_settings():
    """Return current Solis/Solark IP, port, and Modbus ID (for UI or scripting)."""
    return _settings_values()


@app.get("/settings", response_class=HTMLResponse)
async def settings_page(request: Request):
    ctx = _page_ctx(
        request,
        mqtt_enabled=bool(MQTT_HOST),
        mqtt_host=MQTT_HOST or "(disabled)",
        **_settings_values(),
    )
    return templates.TemplateResponse(request, "settings.html", ctx)


@app.post("/settings")
async def settings_save(request: Request):
    """Save inverter IP/port/Modbus from form; redirect back to settings."""
    try:
        form = await request.form()
        data = {
            "solis_host": (form.get("solis_host") or "").strip() or "10.10.53.16",
            "solis_port": int((form.get("solis_port") or "502").strip() or "502"),
            "solis_modbus_unit": int((form.get("solis_modbus_unit") or "1").strip() or "1"),
            "solark1_host": (form.get("solark1_host") or "").strip(),
            "solark1_port": int((form.get("solark1_port") or "502").strip() or "502"),
            "solark1_modbus_unit": int((form.get("solark1_modbus_unit") or "1").strip() or "1"),
            "solark2_modbus_unit": int((form.get("solark2_modbus_unit") or "2").strip() or "2"),
            "solark_soc_automation_enabled": form.get("solark_soc_automation_enabled") in ("1", "on", "true", "yes"),
        }
        save_settings(data)
        return RedirectResponse(url="/settings?saved=1", status_code=303)
    except (ValueError, TypeError) as e:
        logger.warning("settings save validation: %s", e)
        return RedirectResponse(url="/settings?error=invalid", status_code=303)
    except Exception as e:
        logger.exception("settings save: %s", e)
        return RedirectResponse(url="/settings?error=save_failed", status_code=303)


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
        return templates.TemplateResponse(request, "debug.html", _page_ctx(request))
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
    data = _solis_cache.get("data", {}) or {}
    storage_bits = _merge_bits({n: False for n in _STORAGE_BIT_NAMES}, data.get("storage_bits"))
    out = dict(data)
    out["storage_mode"] = _storage_mode_label(storage_bits)
    solark_data = _solark_cache.get("data", {}) or {}
    return {
        "ok": _solis_cache.get("ok"),
        "data": out,
        "ts": _solis_cache.get("ts"),
        "solis": {"data": out, "ok": _solis_cache.get("ok"), "ts": _solis_cache.get("ts")},
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
    }


@app.get("/api/sensors")
async def api_sensors():
    return _solis_cache.get("data", {})


@app.get("/api/storage_bits")
async def api_storage_bits():
    return get_storage_control_bits()


@app.get("/api/hybrid_bits")
async def api_hybrid_bits():
    return get_hybrid_control_bits()


@app.post("/api/storage_toggle")
async def api_storage_toggle(request: Request):
    """Toggle a storage bit (43110). Accepts application/json or form: bit_index (0–11), on (true/false)."""
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
    ok = set_storage_control_bit(bit_index, on_bool)
    return {"ok": ok, "bit_index": bit_index, "on": on_bool}


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
    ok = set_hybrid_control_bit(bit_index, on_bool)
    return {"ok": ok, "bit_index": bit_index, "on": on_bool}


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
        try:
            loop = asyncio.get_event_loop()
            if register == "hybrid" and 0 <= bit_index <= 7:
                ok = await asyncio.wait_for(
                    loop.run_in_executor(None, lambda: set_hybrid_control_bit(bit_index, on_bool)),
                    timeout=_MODBUS_WRITE_TIMEOUT,
                )
            elif register == "storage" and 0 <= bit_index <= 11:
                ok = await asyncio.wait_for(
                    loop.run_in_executor(None, lambda: set_storage_control_bit(bit_index, on_bool)),
                    timeout=_MODBUS_WRITE_TIMEOUT,
                )
        except asyncio.TimeoutError:
            logger.warning("Modbus write timed out (control)")
            return RedirectResponse(url="/control?error=timeout", status_code=303)
        return RedirectResponse(
            url="/control?saved=1" if ok else "/control?error=write_failed",
            status_code=303,
        )
    except Exception as e:
        logger.exception("POST /control: %s", e)
        return RedirectResponse(url="/control?error=server_error", status_code=303)


if __name__ == "__main__":
    import uvicorn
    from config import HOST, PORT
    uvicorn.run("main:app", host=HOST, port=PORT, reload=False)

"""Configuration for Solis S6 app. Use env vars or .env (in this dir) for server 10.10.53.92.
   Settings page can persist IP/port/Modbus ID to settings.json; env overrides file."""
import json
import os
from pathlib import Path

_CONFIG_DIR = Path(__file__).resolve().parent
_VERSION_FILE = _CONFIG_DIR / "VERSION"
_SETTINGS_FILE = _CONFIG_DIR / "settings.json"

def _load_settings_overrides() -> dict:
    """Load UI-saved settings from settings.json. Env vars still override at runtime."""
    out = {}
    try:
        if _SETTINGS_FILE.exists():
            with open(_SETTINGS_FILE) as f:
                out = json.load(f)
    except Exception:
        pass
    return out

_settings_overrides = _load_settings_overrides()

try:
    _v = os.environ.get("SOLIS_UI_VERSION", "").strip()
    if _v:
        APP_VERSION = _v
    elif _VERSION_FILE.exists():
        APP_VERSION = _VERSION_FILE.read_text().strip() or "0.0.0"
    else:
        APP_VERSION = "0.0.0"
except Exception:
    APP_VERSION = "0.0.0"

# Inverter display labels
INVERTER_LABEL_SOLIS1 = "Solis1"
INVERTER_LABEL_SOLARK1 = "Solark1"
INVERTER_LABEL_SOLARK2 = "Solark2"

# Load .env so env overrides are available before get_* / backwards-compat
_env_file = _CONFIG_DIR / ".env"
try:
    if _env_file.exists():
        with open(_env_file) as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith("#") and "=" in line:
                    k, v = line.split("=", 1)
                    k, v = k.strip(), v.strip().strip('"').strip("'")
                    if k and k not in os.environ:
                        os.environ.setdefault(k, v)
except Exception:
    pass

def _get(key: str, env_key: str, default: str | int) -> str | int:
    """Prefer env, then settings.json, then default."""
    v = os.environ.get(env_key)
    if v is not None and str(v).strip() != "":
        return int(v) if isinstance(default, int) else str(v).strip()
    o = _settings_overrides.get(key)
    if o is not None and str(o).strip() != "":
        return int(o) if isinstance(default, int) else str(o).strip()
    return default


def _get_bool(key: str, env_key: str, default: bool) -> bool:
    """Prefer env, then settings.json, then default. Accepts 1/0, true/false, yes/no, on/off."""
    v = os.environ.get(env_key)
    if v is not None and str(v).strip() != "":
        return str(v).strip().lower() in ("1", "true", "yes", "on")
    o = _settings_overrides.get(key)
    if o is not None:
        if isinstance(o, bool):
            return o
        return str(o).strip().lower() in ("1", "true", "yes", "on")
    return default

# 10.10.53.90 = Tesla Battery Emulator (BE) board 1; Solis inverters are at .16 (and optional second). Solark is a separate data source.
def get_solis_host() -> str:
    return str(_get("solis_host", "SOLIS_INVERTER_HOST", "10.10.53.16"))

def get_solis_port() -> int:
    return int(_get("solis_port", "SOLIS_INVERTER_PORT", 502))

def get_solis_modbus_unit() -> int:
    return int(_get("solis_modbus_unit", "SOLIS_MODBUS_UNIT", 1))


def get_solis2_host() -> str:
    """Second Solis inverter; empty = not configured."""
    return str(_get("solis2_host", "SOLIS2_INVERTER_HOST", "")).strip()


def get_solis2_port() -> int:
    return int(_get("solis2_port", "SOLIS2_INVERTER_PORT", 502))


def get_solis2_modbus_unit() -> int:
    return int(_get("solis2_modbus_unit", "SOLIS2_MODBUS_UNIT", 1))


def get_solis_inverters() -> list[dict]:
    """List of Solis inverter configs for multi-connection polling. Each dict: host, port, unit, topic_id, label."""
    inv1 = {
        "host": get_solis_host(),
        "port": get_solis_port(),
        "unit": get_solis_modbus_unit(),
        "topic_id": "s6-inv-1",
        "label": "Solis S6-INV-1",
    }
    out = [inv1]
    host2 = get_solis2_host()
    if host2:
        out.append({
            "host": host2,
            "port": get_solis2_port(),
            "unit": get_solis2_modbus_unit(),
            "topic_id": "s6-inv-2",
            "label": "Solis S6-INV-2",
        })
    return out


# Solark data source: separate from Tesla BE. 10.10.53.90 = Tesla Battery Emulator (BE) board; set SOLARK1_HOST to the device that serves Solark (e.g. /solark_data or MQTT).
def get_solark1_host() -> str:
    return str(_get("solark1_host", "SOLARK1_HOST", "")).strip()

def get_solark1_port() -> int:
    return int(_get("solark1_port", "SOLARK1_PORT", 502))

def get_solark1_http_port() -> int:
    """HTTP port for GET /solark_data (board web server; default 80)."""
    return int(_get("solark1_http_port", "SOLARK1_HTTP_PORT", 80))

def get_solark1_modbus_unit() -> int:
    return int(_get("solark1_modbus_unit", "SOLARK1_MODBUS_UNIT", 1))

def get_solark2_modbus_unit() -> int:
    return int(_get("solark2_modbus_unit", "SOLARK2_MODBUS_UNIT", 2))

# Solis Modbus always talks to the Solis inverter. Solark data is fetched separately via HTTP.
def get_inverter_host() -> str:
    return get_solis_host()

def get_inverter_port() -> int:
    return get_solis_port()

def get_modbus_unit() -> int:
    return get_solis_modbus_unit()

# Optional HTTP auth for fetching Solark data from the board (GET /solark_data).
SOLARK_HTTP_USER = os.environ.get("SOLARK_HTTP_USER", "").strip() or None
SOLARK_HTTP_PASSWORD = os.environ.get("SOLARK_HTTP_PASSWORD", "").strip() or None

# ESPHome device at 10.10.53.32 — second HTTP source for Solark data.
# Host is stored in settings.json; credentials are env/`.env` only.
def get_esphome_solark_host() -> str:
    return str(_get("esphome_solark_host", "ESPHOME_SOLARK_HOST", "")).strip()

def get_esphome_solark_port() -> int:
    return int(_get("esphome_solark_port", "ESPHOME_SOLARK_PORT", 80))

ESPHOME_SOLARK_USER: str | None = os.environ.get("ESPHOME_SOLARK_USER", "").strip() or None
ESPHOME_SOLARK_PASSWORD: str | None = os.environ.get("ESPHOME_SOLARK_PASSWORD", "").strip() or None
# MQTT topic for Solark data (board publishes same JSON as /solark_data). Empty = don't subscribe.
SOLARK_MQTT_TOPIC = str(_get("solark_mqtt_topic", "SOLARK_MQTT_TOPIC", "solar/solark") or "").strip()
# When Solark SOC >= this (percent), switch Solis to self-use. 0 = disabled.
SOLARK_SOC_SELF_USE_THRESHOLD_PCT = int(os.environ.get("SOLARK_SOC_SELF_USE_THRESHOLD_PCT", "98"))
# When Solark SOC drops below this, allow switching Solis back to feed-in (hysteresis).
SOLARK_SOC_FEEDIN_BELOW_PCT = int(os.environ.get("SOLARK_SOC_FEEDIN_BELOW_PCT", "95"))


def get_solark_soc_automation_enabled() -> bool:
    """Whether to switch Solis to self-use when Solark SOC >= threshold. Can be toggled in Settings or via API."""
    return _get_bool("solark_soc_automation_enabled", "SOLARK_SOC_AUTOMATION_ENABLED", True)


def load_settings() -> dict:
    """Return current settings from settings.json (for merging and saving partial updates)."""
    return dict(_load_settings_overrides())


def save_settings(data: dict) -> None:
    """Persist settings to settings.json and reload in-memory overrides."""
    global _settings_overrides
    try:
        with open(_SETTINGS_FILE, "w") as f:
            json.dump(data, f, indent=2)
        _settings_overrides = _load_settings_overrides()
    except Exception:
        raise

# Backwards compatibility
SOLARK1_HOST = get_solark1_host()
SOLARK1_MODBUS_UNIT = get_solark1_modbus_unit()
SOLARK2_MODBUS_UNIT = get_solark2_modbus_unit()
INVERTER_HOST = get_inverter_host()
INVERTER_PORT = get_inverter_port()
MODBUS_UNIT = get_modbus_unit()

# Optional scale for daily PV (register 33035). Use if Modbus returns aggregated/multiple-inverter
# value. E.g. SOLIS_DAILY_PV_SCALE=0.585 converts 26 → 15.2 when inverter display shows 15.2.
try:
    _s = os.environ.get("SOLIS_DAILY_PV_SCALE", "").strip()
    SOLIS_DAILY_PV_SCALE = float(_s) if _s else 1.0
except (ValueError, TypeError):
    SOLIS_DAILY_PV_SCALE = 1.0

# Divisor for total PV energy (33029-33030). S6 uses 0.01 kWh → 100. If total shows ~10x too high, keep 100.
try:
    _d = os.environ.get("SOLIS_TOTAL_PV_DIVISOR", "").strip()
    SOLIS_TOTAL_PV_DIVISOR = float(_d) if _d else 100.0
except (ValueError, TypeError):
    SOLIS_TOTAL_PV_DIVISOR = 100.0

# Home Assistant — curtailment switch control via SmartLife/Tuya
HA_URL   = os.environ.get("HA_URL",   "http://10.10.53.179:8123").strip().rstrip("/")
HA_TOKEN = os.environ.get("HA_TOKEN", "").strip()
HA_SWITCH_IQ8     = os.environ.get("HA_SWITCH_IQ8",     "switch.enphase_iq8_micro_socket_1").strip()
HA_SWITCH_MSERIES = os.environ.get("HA_SWITCH_MSERIES", "switch.shed_micro_inverters_socket_1").strip()
HA_SWITCH_TABUCHI = os.environ.get("HA_SWITCH_TABUCHI", "switch.sonoff_1001204d65_1").strip()

# Tabuchi export: static today PV (kWh) for grid-status totals. Default 3; change in Settings or env.
try:
    _t = os.environ.get("TABUCHI_TODAY_PV_KWH", "").strip()
    TABUCHI_TODAY_PV_KWH_DEFAULT = float(_t) if _t else 3.0
except (ValueError, TypeError):
    TABUCHI_TODAY_PV_KWH_DEFAULT = 3.0


# Battery capacity (kWh) per inverter: solis1, solis2, solark1. Editable in Settings. Used for Remaining kWh and Time to empty/full.
def _get_battery_kwh(key: str, env_key: str, default: float) -> float:
    """Read battery capacity from env, then settings.json, else default."""
    v = os.environ.get(env_key, "").strip()
    if v:
        try:
            return float(v)
        except (ValueError, TypeError):
            pass
    o = _settings_overrides.get(key)
    if o is not None:
        try:
            return float(o)
        except (ValueError, TypeError):
            pass
    return default


def get_solis1_battery_kwh() -> float:
    """First Solis inverter (s6-inv-1) battery capacity (kWh)."""
    # Backward compat: SOLIS_TOTAL_BATTERY_KWH → solis1
    v = os.environ.get("SOLIS1_BATTERY_KWH") or os.environ.get("SOLIS_TOTAL_BATTERY_KWH", "").strip()
    if v:
        try:
            return float(v)
        except (ValueError, TypeError):
            pass
    return _get_battery_kwh("solis1_battery_kwh", "SOLIS1_BATTERY_KWH", 48.0)


def get_solis2_battery_kwh() -> float:
    """Second Solis inverter (s6-inv-2) battery capacity (kWh)."""
    return _get_battery_kwh("solis2_battery_kwh", "SOLIS2_BATTERY_KWH", 48.0)


def get_solark1_battery_kwh() -> float:
    """Solark battery capacity (kWh)."""
    # Backward compat: migrate solark_total_battery_kwh → solark1_battery_kwh
    o = _settings_overrides.get("solark1_battery_kwh")
    if o is None:
        o = _settings_overrides.get("solark_total_battery_kwh")
    if o is not None:
        try:
            return float(o)
        except (ValueError, TypeError):
            pass
    v = os.environ.get("SOLARK1_BATTERY_KWH") or os.environ.get("SOLARK_TOTAL_BATTERY_KWH", "").strip()
    if v:
        try:
            return float(v)
        except (ValueError, TypeError):
            pass
    return 64.0


def get_solis_battery_kwh(topic_id: str) -> float:
    """Battery capacity (kWh) for Solis inverter by topic_id (s6-inv-1, s6-inv-2)."""
    return get_solis2_battery_kwh() if topic_id == "s6-inv-2" else get_solis1_battery_kwh()


def get_tabuchi_today_pv_kwh() -> float:
    """Static Tabuchi today PV (kWh) used when totalling generation. Env overrides settings.json."""
    v = os.environ.get("TABUCHI_TODAY_PV_KWH", "").strip()
    if v:
        try:
            return float(v)
        except (ValueError, TypeError):
            pass
    o = _settings_overrides.get("tabuchi_today_pv_kwh")
    if o is not None:
        try:
            return float(o)
        except (ValueError, TypeError):
            pass
    return TABUCHI_TODAY_PV_KWH_DEFAULT


# App server
HOST = os.environ.get("SOLIS_APP_HOST", "0.0.0.0")
PORT = int(os.environ.get("SOLIS_APP_PORT", "3007"))
POLL_INTERVAL_SEC = float(os.environ.get("SOLIS_POLL_INTERVAL_SEC", "5"))

# MQTT broker – same as solar stack (typically Mosquitto on dashboard server).
# Broker: same as solar-monitoring (10.10.53.92, api/test12345). Override via env or .env.
# TODO: Secure MQTT credentials (strong password, env-only, no defaults) once cutover is complete.
MQTT_HOST = str(_get("mqtt_host", "MQTT_HOST", "10.10.53.92") or "").strip()
MQTT_PORT = int(_get("mqtt_port", "MQTT_PORT", 1883))
# Primary topic: per-inverter naming (solar/solis/s6-inv-1, s6-inv-2, ...). For second inverter set MQTT_TOPIC_PREFIX=solar/solis/s6-inv-2.
MQTT_TOPIC_PREFIX = str(_get("mqtt_topic_prefix", "MQTT_TOPIC_PREFIX", "solar/solis/s6-inv-1")).strip() or "solar/solis/s6-inv-1"
# Legacy prefix: we also publish here so Battery-Emulator display (subscribes to solar/solis/sensors/#) works without firmware change. Set empty to disable.
MQTT_LEGACY_PREFIX = str(_get("mqtt_legacy_prefix", "MQTT_LEGACY_PREFIX", "solar/solis")).strip()
MQTT_USER = os.environ.get("MQTT_USER", "api").strip() or None
MQTT_PASSWORD = os.environ.get("MQTT_PASSWORD", "test12345").strip() or None
MQTT_CLIENT_ID = os.environ.get("MQTT_CLIENT_ID", "solis-s6-ui").strip()

# Forecast.Solar (Stage 4) — two toggles: API fetches, prediction mode (use forecast in automations)
def get_solar_forecast_api_enabled() -> bool:
    """Whether to fetch Forecast.Solar API. When OFF, no external calls."""
    return _get_bool("solar_forecast_api_enabled", "SOLAR_FORECAST_API_ENABLED", False)


def get_solar_prediction_enabled() -> bool:
    """Whether automations use forecast data. When OFF, automations ignore forecast (data may still show in UI)."""
    return _get_bool("solar_prediction_enabled", "SOLAR_PREDICTION_ENABLED", False)


def get_solar_llm_automations_enabled() -> bool:
    """Master toggle for LLM-based automations and assistant features."""
    return _get_bool("solar_llm_automations_enabled", "SOLAR_LLM_AUTOMATIONS_ENABLED", False)


def get_solar_prediction_lat() -> float:
    """Latitude for Forecast.Solar (-90 to 90)."""
    v = _get("solar_prediction_lat", "SOLAR_PREDICTION_LAT", 40.7)
    try:
        return float(v)
    except (TypeError, ValueError):
        return 40.7


def get_solar_prediction_lon() -> float:
    """Longitude for Forecast.Solar (-180 to 180)."""
    v = _get("solar_prediction_lon", "SOLAR_PREDICTION_LON", -74.0)
    try:
        return float(v)
    except (TypeError, ValueError):
        return -74.0


def get_solar_prediction_dec() -> int:
    """Panel declination/tilt in degrees (0=horizontal, 90=vertical). Default 45 (south-facing)."""
    v = _get("solar_prediction_dec", "SOLAR_PREDICTION_DEC", 45)
    try:
        return int(v)
    except (TypeError, ValueError):
        return 45


def get_solar_prediction_az() -> int:
    """Panel azimuth (-180 to 180; 0=south). Default 0 (south)."""
    v = _get("solar_prediction_az", "SOLAR_PREDICTION_AZ", 0)
    try:
        return int(v)
    except (TypeError, ValueError):
        return 0


def get_solar_prediction_kwp() -> float:
    """Combined installed kWp (Solark + Solis + Envoy + Tabuchi). Default 25."""
    v = _get("solar_prediction_kwp", "SOLAR_PREDICTION_KWP", "25")
    try:
        return float(v)
    except (TypeError, ValueError):
        return 25.0

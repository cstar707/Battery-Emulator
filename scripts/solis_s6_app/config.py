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

def get_solis_host() -> str:
    return str(_get("solis_host", "SOLIS_INVERTER_HOST", "10.10.53.16"))

def get_solis_port() -> int:
    return int(_get("solis_port", "SOLIS_INVERTER_PORT", 502))

def get_solis_modbus_unit() -> int:
    return int(_get("solis_modbus_unit", "SOLIS_MODBUS_UNIT", 1))

def get_solark1_host() -> str:
    return str(_get("solark1_host", "SOLARK1_HOST", "")).strip()

def get_solark1_port() -> int:
    return int(_get("solark1_port", "SOLARK1_PORT", 502))

def get_solark1_http_port() -> int:
    """HTTP port for GET /solark_data (board web server; default 80)."""
    return int(os.environ.get("SOLARK1_HTTP_PORT", "80").strip() or "80")

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
# MQTT topic for Solark data (board publishes same JSON as /solark_data). Empty = don't subscribe.
SOLARK_MQTT_TOPIC = (os.environ.get("SOLARK_MQTT_TOPIC", "solar/solark") or "").strip()
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

# App server
HOST = os.environ.get("SOLIS_APP_HOST", "0.0.0.0")
PORT = int(os.environ.get("SOLIS_APP_PORT", "3007"))
POLL_INTERVAL_SEC = float(os.environ.get("SOLIS_POLL_INTERVAL_SEC", "5"))

# MQTT broker – same as solar stack (typically Mosquitto on dashboard server).
# Broker: same as solar-monitoring (10.10.53.92, api/test12345). Override via env or .env.
# TODO: Secure MQTT credentials (strong password, env-only, no defaults) once cutover is complete.
MQTT_HOST = os.environ.get("MQTT_HOST", "10.10.53.92").strip()
MQTT_PORT = int(os.environ.get("MQTT_PORT", "1883"))
MQTT_TOPIC_PREFIX = os.environ.get("MQTT_TOPIC_PREFIX", "solis").strip() or "solis"
MQTT_USER = os.environ.get("MQTT_USER", "api").strip() or None
MQTT_PASSWORD = os.environ.get("MQTT_PASSWORD", "test12345").strip() or None
MQTT_CLIENT_ID = os.environ.get("MQTT_CLIENT_ID", "solis-s6-ui").strip()

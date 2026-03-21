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


def _get_float_with_legacy(
    key: str,
    env_key: str,
    default: float,
    *,
    min_val: float,
    max_val: float,
    legacy_key: str | None = None,
    legacy_env_key: str | None = None,
) -> float:
    raw = os.environ.get(env_key)
    if (raw is None or str(raw).strip() == "") and legacy_env_key:
        raw = os.environ.get(legacy_env_key)
    if (raw is None or str(raw).strip() == ""):
        raw = _settings_overrides.get(key)
    if (raw is None or str(raw).strip() == "") and legacy_key:
        raw = _settings_overrides.get(legacy_key)
    if raw is None or str(raw).strip() == "":
        return default
    try:
        value = float(raw)
    except (TypeError, ValueError):
        return default
    if value < min_val:
        return min_val
    if value > max_val:
        return max_val
    return value

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
# When Solark SOC >= this (percent), the legacy Solis protection path curtails PV output. 0 = disabled.
# Curtail headroom: set lower (e.g. 96) to shut down earlier and avoid overshooting 98% at peak solar.
SOLARK_SOC_SELF_USE_THRESHOLD_PCT = int(os.environ.get("SOLARK_SOC_SELF_USE_THRESHOLD_PCT", "98"))
# When Solark SOC drops below this, allow legacy PV output restoration (hysteresis).
SOLARK_SOC_FEEDIN_BELOW_PCT = int(os.environ.get("SOLARK_SOC_FEEDIN_BELOW_PCT", "95"))


def get_solark_soc_automation_enabled() -> bool:
    """Whether to run the legacy Solis PV-curtailment protection path from Settings or API."""
    return _get_bool("solark_soc_automation_enabled", "SOLARK_SOC_AUTOMATION_ENABLED", True)


def get_solark_soc_scale() -> float:
    """
    Scale factor applied to Solark SOC for automations and display.
    Example: 0.98 makes a raw 98.0% read as 96.0%.
    """
    v = _get("solark_soc_scale", "SOLARK_SOC_SCALE", "1.0")
    try:
        f = float(v)
    except (TypeError, ValueError):
        return 1.0
    # Keep sane bounds so a typo can't break safety logic.
    if f < 0.5:
        return 0.5
    if f > 1.5:
        return 1.5
    return f


def get_solark_soc_offset_pct() -> float:
    """
    Offset (percentage points) applied after scale.
    Example: -3.0 makes a raw 98.0% read as 95.0% (before clamping).
    """
    v = _get("solark_soc_offset_pct", "SOLARK_SOC_OFFSET_PCT", "0.0")
    try:
        f = float(v)
    except (TypeError, ValueError):
        return 0.0
    if f < -20.0:
        return -20.0
    if f > 20.0:
        return 20.0
    return f


# HA curtailment restore “sweet spot” (Option B):
# When we curtailed HA devices because Solark SOC >= threshold, we can restore them early
# once the Solark battery starts drawing power (battery_total_power_W <= some threshold)
# and that condition persists for a configurable hold time.
def get_ha_restore_on_batt_draw_enabled() -> bool:
    """Enable early restore when battery indicates draw (< threshold) while SOC is still high."""
    return _get_bool("ha_restore_on_batt_draw_enabled", "SOLARK_HA_RESTORE_ON_BATT_DRAW_ENABLED", False)


def get_ha_restore_batt_draw_power_threshold_w() -> float:
    """Restore when normalized Solark battery_total_power_W <= this value (default 0W)."""
    v = _get("ha_restore_batt_draw_power_threshold_w", "SOLARK_HA_RESTORE_BATT_DRAW_POWER_THRESHOLD_W", "0.0")
    try:
        f = float(v)
    except (TypeError, ValueError):
        return 0.0
    if f < -100000:
        return -100000.0
    if f > 100000:
        return 100000.0
    return f


def get_ha_restore_batt_draw_hold_sec() -> int:
    """Hold time (seconds) required before restoring when battery draw condition is met."""
    v = _get("ha_restore_batt_draw_hold_sec", "SOLARK_HA_RESTORE_BATT_DRAW_HOLD_SEC", "60")
    try:
        i = int(v)
    except (TypeError, ValueError):
        return 60
    if i < 0:
        return 0
    if i > 3600:
        return 3600
    return i


def get_solis_power_controls_enabled() -> bool:
    return _get_bool("solis_power_controls_enabled", "SOLIS_POWER_CONTROLS_ENABLED", True)


def get_solis_offgrid_automation_enabled() -> bool:
    return _get_bool("solis_offgrid_automation_enabled", "SOLIS_OFFGRID_AUTOMATION_ENABLED", False)


def get_solis_offgrid_enter_solark_soc_pct() -> float:
    v = _get("solis_offgrid_enter_solark_soc_pct", "SOLIS_OFFGRID_ENTER_SOLARK_SOC_PCT", "90.0")
    try:
        f = float(v)
    except (TypeError, ValueError):
        return 90.0
    if f < 0.0:
        return 0.0
    if f > 100.0:
        return 100.0
    return f


def get_solis_manual_offgrid_auto_release_enabled() -> bool:
    return _get_bool(
        "solis_manual_offgrid_auto_release_enabled",
        "SOLIS_MANUAL_OFFGRID_AUTO_RELEASE_ENABLED",
        True,
    )


def get_solis_manual_offgrid_release_pv_w() -> float:
    return _get_float_with_legacy(
        "solis_manual_offgrid_release_pv_w",
        "SOLIS_MANUAL_OFFGRID_RELEASE_PV_W",
        5000.0,
        min_val=0.0,
        max_val=100000.0,
        legacy_key="solis_restore_self_use_available_pv_w",
        legacy_env_key="SOLIS_RESTORE_SELF_USE_AVAILABLE_PV_W",
    )


def get_solis_manual_offgrid_release_solark_soc_pct() -> float:
    return _get_float_with_legacy(
        "solis_manual_offgrid_release_solark_soc_pct",
        "SOLIS_MANUAL_OFFGRID_RELEASE_SOLARK_SOC_PCT",
        30.0,
        min_val=0.0,
        max_val=100.0,
    )


def get_solis_restore_self_use_available_pv_w() -> float:
    """Deprecated alias preserved for older settings/env names."""
    return get_solis_manual_offgrid_release_pv_w()


def get_solis_tou_charge_automation_enabled() -> bool:
    return _get_bool("solis_tou_charge_automation_enabled", "SOLIS_TOU_CHARGE_AUTOMATION_ENABLED", False)


def get_solis_tou_charge_available_pv_w() -> float:
    v = _get("solis_tou_charge_available_pv_w", "SOLIS_TOU_CHARGE_AVAILABLE_PV_W", "3000.0")
    try:
        f = float(v)
    except (TypeError, ValueError):
        return 3000.0
    if f < 0.0:
        return 0.0
    if f > 100000.0:
        return 100000.0
    return f


def get_solis_tou_charge_amps() -> float:
    v = _get("solis_tou_charge_amps", "SOLIS_TOU_CHARGE_AMPS", "52.0")
    try:
        f = float(v)
    except (TypeError, ValueError):
        return 52.0
    if f < 0.0:
        return 0.0
    if f > 70.0:
        return 70.0
    return f


def get_solis_tou_discharge_amps() -> float:
    v = _get("solis_tou_discharge_amps", "SOLIS_TOU_DISCHARGE_AMPS", "1.0")
    try:
        f = float(v)
    except (TypeError, ValueError):
        return 1.0
    if f < 0.0:
        return 0.0
    if f > 70.0:
        return 70.0
    return f


# --- Solis priority and export logic ---
def get_solark_full_soc_pct() -> float:
    """Solark SOC at which we stop allowing export (no export above this)."""
    return _get_float_with_legacy(
        "solark_full_soc_pct", "SOLARK_FULL_SOC_PCT", 98.0,
        min_val=0.0, max_val=100.0,
    )


def get_solis_full_soc_pct() -> float:
    """Solis battery SOC at which we consider it full; only then curtail Solis PV when Solark also full."""
    return _get_float_with_legacy(
        "solis_full_soc_pct", "SOLIS_FULL_SOC_PCT", 95.0,
        min_val=0.0, max_val=100.0,
    )


def get_solis_curtail_when_both_full() -> bool:
    """When both Solark and Solis full, curtail Solis PV to 0%."""
    return _get_bool("solis_curtail_when_both_full", "SOLIS_CURTAIL_WHEN_BOTH_FULL", True)


def get_solis_grid_charge_when_solark_full() -> bool:
    """When Solark full, allow Solis to charge from grid."""
    return _get_bool("solis_grid_charge_when_solark_full", "SOLIS_GRID_CHARGE_WHEN_SOLARK_FULL", True)


# --- TOU schedule (configurable) ---
def _get_int(key: str, env_key: str, default: int, min_val: int, max_val: int) -> int:
    v = os.environ.get(env_key)
    if v is not None and str(v).strip() != "":
        try:
            return max(min_val, min(max_val, int(v)))
        except (TypeError, ValueError):
            pass
    o = _settings_overrides.get(key)
    if o is not None:
        try:
            return max(min_val, min(max_val, int(o)))
        except (TypeError, ValueError):
            pass
    return default


def get_solis_tou_charge_start_h() -> int:
    return _get_int("solis_tou_charge_start_h", "SOLIS_TOU_CHARGE_START_H", 7, 0, 23)


def get_solis_tou_charge_start_m() -> int:
    return _get_int("solis_tou_charge_start_m", "SOLIS_TOU_CHARGE_START_M", 0, 0, 59)


def get_solis_tou_charge_end_h() -> int:
    return _get_int("solis_tou_charge_end_h", "SOLIS_TOU_CHARGE_END_H", 19, 0, 23)


def get_solis_tou_charge_end_m() -> int:
    return _get_int("solis_tou_charge_end_m", "SOLIS_TOU_CHARGE_END_M", 0, 0, 59)


def get_solis_tou_discharge_start_h() -> int:
    return _get_int("solis_tou_discharge_start_h", "SOLIS_TOU_DISCHARGE_START_H", 19, 0, 23)


def get_solis_tou_discharge_start_m() -> int:
    return _get_int("solis_tou_discharge_start_m", "SOLIS_TOU_DISCHARGE_START_M", 1, 0, 59)


def get_solis_tou_discharge_end_h() -> int:
    return _get_int("solis_tou_discharge_end_h", "SOLIS_TOU_DISCHARGE_END_H", 6, 0, 23)


def get_solis_tou_discharge_end_m() -> int:
    return _get_int("solis_tou_discharge_end_m", "SOLIS_TOU_DISCHARGE_END_M", 0, 0, 59)


# --- Charge ramp (morning PV follow) ---
def get_solis_tou_charge_ramp_enabled() -> bool:
    return _get_bool("solis_tou_charge_ramp_enabled", "SOLIS_TOU_CHARGE_RAMP_ENABLED", True)


def get_solis_tou_charge_amps_min() -> float:
    return _get_float_with_legacy(
        "solis_tou_charge_amps_min", "SOLIS_TOU_CHARGE_AMPS_MIN", 10.0,
        min_val=0.0, max_val=70.0,
    )


def get_solis_tou_charge_amps_max() -> float:
    return _get_float_with_legacy(
        "solis_tou_charge_amps_max", "SOLIS_TOU_CHARGE_AMPS_MAX", 52.0,
        min_val=0.0, max_val=70.0,
    )


def get_solis_tou_charge_ramp_pv_threshold_w() -> float:
    return _get_float_with_legacy(
        "solis_tou_charge_ramp_pv_threshold_w", "SOLIS_TOU_CHARGE_RAMP_PV_THRESHOLD_W", 1000.0,
        min_val=0.0, max_val=100000.0,
    )


def get_solis_tou_charge_ramp_pv_max_w() -> float:
    return _get_float_with_legacy(
        "solis_tou_charge_ramp_pv_max_w", "SOLIS_TOU_CHARGE_RAMP_PV_MAX_W", 8000.0,
        min_val=0.0, max_val=100000.0,
    )


# --- Safe window (7am) ---
def get_safe_window_start_h() -> int:
    return _get_int("safe_window_start_h", "SAFE_WINDOW_START_H", 7, 0, 23)


def get_safe_window_start_m() -> int:
    return _get_int("safe_window_start_m", "SAFE_WINDOW_START_M", 0, 0, 59)


def get_safe_window_ensure_ha_on() -> bool:
    return _get_bool("safe_window_ensure_ha_on", "SAFE_WINDOW_ENSURE_HA_ON", True)


def get_safe_window_allow_solis_grid_charge() -> bool:
    return _get_bool("safe_window_allow_solis_grid_charge", "SAFE_WINDOW_ALLOW_SOLIS_GRID_CHARGE", True)


# --- HA generation max peak ---
def get_iq8_max_peak_kw() -> float:
    return _get_float_with_legacy(
        "iq8_max_peak_kw", "IQ8_MAX_PEAK_KW", 6.5,
        min_val=0.0, max_val=100.0,
    )


def get_mseries_max_peak_kw() -> float:
    return _get_float_with_legacy(
        "mseries_max_peak_kw", "MSERIES_MAX_PEAK_KW", 2.5,
        min_val=0.0, max_val=100.0,
    )


def get_tabuchi_max_peak_kw() -> float:
    return _get_float_with_legacy(
        "tabuchi_max_peak_kw", "TABUCHI_MAX_PEAK_KW", 3.0,
        min_val=0.0, max_val=100.0,
    )


# --- Discharge ramp (evening load sharing) ---
def get_solis_discharge_ramp_enabled() -> bool:
    return _get_bool("solis_discharge_ramp_enabled", "SOLIS_DISCHARGE_RAMP_ENABLED", True)


def get_solark_soc_discharge_ramp_threshold_pct() -> float:
    return _get_float_with_legacy(
        "solark_soc_discharge_ramp_threshold_pct", "SOLARK_SOC_DISCHARGE_RAMP_THRESHOLD_PCT", 50.0,
        min_val=0.0, max_val=100.0,
    )


def get_solark_soc_discharge_ramp_floor_pct() -> float:
    return _get_float_with_legacy(
        "solark_soc_discharge_ramp_floor_pct", "SOLARK_SOC_DISCHARGE_RAMP_FLOOR_PCT", 30.0,
        min_val=0.0, max_val=100.0,
    )


def get_solis_tou_discharge_amps_min() -> float:
    return _get_float_with_legacy(
        "solis_tou_discharge_amps_min", "SOLIS_TOU_DISCHARGE_AMPS_MIN", 1.0,
        min_val=0.0, max_val=70.0,
    )


def get_solis_tou_discharge_amps_max() -> float:
    return _get_float_with_legacy(
        "solis_tou_discharge_amps_max", "SOLIS_TOU_DISCHARGE_AMPS_MAX", 15.0,
        min_val=0.0, max_val=70.0,
    )


# --- Discharge export ---
def get_solis_discharge_allow_export() -> bool:
    return _get_bool("solis_discharge_allow_export", "SOLIS_DISCHARGE_ALLOW_EXPORT", True)


def get_solis_discharge_export_cap_w() -> float:
    return _get_float_with_legacy(
        "solis_discharge_export_cap_w", "SOLIS_DISCHARGE_EXPORT_CAP_W", 0.0,
        min_val=0.0, max_val=50000.0,
    )


def get_solis_discharge_export_mode() -> str:
    v = str(_get("solis_discharge_export_mode", "SOLIS_DISCHARGE_EXPORT_MODE", "controlled")).strip().lower()
    return v if v in ("zero", "controlled", "full") else "controlled"


# --- Grid import during discharge ---
def get_solis_discharge_allow_grid_import() -> bool:
    return _get_bool("solis_discharge_allow_grid_import", "SOLIS_DISCHARGE_ALLOW_GRID_IMPORT", True)


def get_solis_discharge_grid_import_max_w() -> float:
    return _get_float_with_legacy(
        "solis_discharge_grid_import_max_w", "SOLIS_DISCHARGE_GRID_IMPORT_MAX_W", 0.0,
        min_val=0.0, max_val=50000.0,
    )


def get_solis_discharge_grid_import_mode() -> str:
    v = str(_get("solis_discharge_grid_import_mode", "SOLIS_DISCHARGE_GRID_IMPORT_MODE", "auto")).strip().lower()
    return v if v in ("off", "auto", "target") else "auto"


# --- Load subsidy mode ---
def get_solis_discharge_load_subsidy_pct() -> float:
    return _get_float_with_legacy(
        "solis_discharge_load_subsidy_pct", "SOLIS_DISCHARGE_LOAD_SUBSIDY_PCT", 50.0,
        min_val=0.0, max_val=100.0,
    )


def get_solis_discharge_load_subsidy_enabled() -> bool:
    return _get_bool("solis_discharge_load_subsidy_enabled", "SOLIS_DISCHARGE_LOAD_SUBSIDY_ENABLED", True)


def get_solis_discharge_prioritize_higher_soc() -> bool:
    """When true, if Solark SOC > Solis SOC, use 0 discharge amps so Solark supplies the load."""
    return _get_bool("solis_discharge_prioritize_higher_soc", "SOLIS_DISCHARGE_PRIORITIZE_HIGHER_SOC", True)


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
def get_ha_url() -> str:
    return str(_get("ha_url", "HA_URL", "http://10.10.53.179:8123")).strip().rstrip("/")


def get_ha_token() -> str:
    return str(_get("ha_token", "HA_TOKEN", "")).strip()


# Defaults (may change at runtime via settings.json overrides; backend should call getters).
HA_URL = get_ha_url()
HA_TOKEN = get_ha_token()
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

# InfluxDB — for /api/influx-health and /api/influx/query
def get_influx_url() -> str:
    return str(_get("influx_url", "INFLUX_URL", "http://localhost:8086")).strip()

def get_influx_token() -> str:
    return str(os.environ.get("INFLUX_TOKEN", "") or _settings_overrides.get("influx_token") or "").strip()

def get_influx_bucket() -> str:
    return str(_get("influx_bucket", "INFLUX_BUCKET", "solar")).strip()

def get_influx_org() -> str:
    return str(os.environ.get("INFLUX_ORG", "") or _settings_overrides.get("influx_org") or "").strip()

# Forecast.Solar (Stage 4) — two toggles: API fetches, prediction mode (use forecast in automations)
def get_solar_forecast_api_enabled() -> bool:
    """Whether to fetch Forecast.Solar API. When OFF, no external calls."""
    return _get_bool("solar_forecast_api_enabled", "SOLAR_FORECAST_API_ENABLED", False)


def get_solar_prediction_enabled() -> bool:
    """Whether automations use forecast data. When OFF, automations ignore forecast (data may still show in UI)."""
    return _get_bool("solar_prediction_enabled", "SOLAR_PREDICTION_ENABLED", False)


def get_solar_llm_automations_enabled() -> bool:
    """Master toggle for LLM-based automations and assistant features."""
    return _get_bool("solar_llm_automations_enabled", "SOLAR_LLM_AUTOMATIONS_ENABLED", True)


def get_assistant_system_prompt() -> str:
    """Optional system prompt for the chat assistant. Documents setup for context-aware answers."""
    return str(
        os.environ.get("ASSISTANT_SYSTEM_PROMPT", "")
        or _settings_overrides.get("assistant_system_prompt", "")
        or (
            "You are an assistant for a solar + storage system. Setup: Solis hybrid inverter(s) and Solark, "
            "with Enphase IQ8 and M-series micro-inverters (Envoy). TOU charge/discharge windows control "
            "when batteries charge from PV and when they discharge. Storage modes: self_use, off_grid, tou, etc. "
            "Curtailment switches (IQ8, M-series, Tabuchi) can be turned off when SOC is full. "
            "For historical PV/battery data, direct the user to the Data page or Grafana."
        )
    ).strip()


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

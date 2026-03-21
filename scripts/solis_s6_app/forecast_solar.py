"""Forecast.Solar API integration for solar production forecast.
   Free tier: 12 calls/60 min per IP; today + tomorrow (limit=2); 1h resolution.
   Fetches when SOLAR_FORECAST_API_ENABLED; caches result for UI and optional automation use."""
from __future__ import annotations

import json
import logging
import time
from urllib.request import Request as UrllibRequest, urlopen
from urllib.error import URLError, HTTPError

logger = logging.getLogger(__name__)

_FORECAST_BASE = "https://api.forecast.solar"
_FORECAST_CACHE: dict = {
    "data": None,
    "ts": 0.0,
    "error": None,
    "ratelimit": {},
}


def _build_forecast_url(lat: float, lon: float, dec: int, az: int, kwp: float) -> str:
    """Build estimate URL. limit=2 for today + tomorrow."""
    path = f"/estimate/{lat}/{lon}/{dec}/{az}/{kwp}"
    return _FORECAST_BASE + path + "?limit=2"


def fetch_forecast(lat: float, lon: float, dec: int, az: int, kwp: float) -> tuple[dict | None, str | None]:
    """
    Fetch Forecast.Solar estimate (watts, watt_hours_day, etc.).
    Returns (result_dict, None) on success or (None, error_str) on failure.
    """
    url = _build_forecast_url(lat, lon, dec, az, kwp)
    req = UrllibRequest(url, headers={"Accept": "application/json"})
    try:
        with urlopen(req, timeout=15) as r:
            raw = r.read().decode()
            payload = json.loads(raw) if raw else {}
        msg = payload.get("message") or {}
        if msg.get("code", 0) != 0:
            return None, msg.get("text", "API error") or str(msg)
        result = payload.get("result")
        if not result:
            return None, "Empty result"
        ratelimit = msg.get("ratelimit", {})
        return {"result": result, "ratelimit": ratelimit}, None
    except HTTPError as e:
        body = ""
        try:
            body = e.read().decode()[:500]
        except Exception:
            pass
        err = f"HTTP {e.code}: {body or str(e)}"
        logger.warning("Forecast.Solar fetch %s: %s", url, err)
        return None, err
    except (URLError, OSError, ValueError, json.JSONDecodeError) as e:
        err = str(e)
        logger.warning("Forecast.Solar fetch %s: %s", url, err)
        return None, err


def update_cache(lat: float, lon: float, dec: int, az: int, kwp: float) -> None:
    """Fetch and update in-memory cache. Safe to call from background thread."""
    global _FORECAST_CACHE
    data, err = fetch_forecast(lat, lon, dec, az, kwp)
    _FORECAST_CACHE["ts"] = time.time()
    _FORECAST_CACHE["error"] = err
    _FORECAST_CACHE["ratelimit"] = {}
    if data:
        _FORECAST_CACHE["data"] = data
        _FORECAST_CACHE["ratelimit"] = data.get("ratelimit", {})
    else:
        _FORECAST_CACHE["data"] = None


def get_cached_forecast() -> dict:
    """Return current cache state for UI/API. Does not fetch."""
    return dict(_FORECAST_CACHE)

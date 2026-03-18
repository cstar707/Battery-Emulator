"""
Local LLM client for Solis/Sol-Ark decision assistance.

Talks to a local LLM HTTP API running on the LAN, defaulting to 10.10.53.164.
Supports either an OpenAI-compatible chat API or Ollama's /api/chat API. All
prompts must result in strict JSON that matches the controlled schema below;
this module validates and normalises the result before the automation layer
consumes it.
"""
from __future__ import annotations

import json
import os
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen


_CONFIG_DIR = Path(__file__).resolve().parent
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


@dataclass
class LlmConfig:
    """Config for the local LLM endpoint."""

    host: str = os.environ.get("SOLAR_LLM_HOST", "10.10.53.164").strip()
    port: int = int(os.environ.get("SOLAR_LLM_PORT", "8000"))
    model: str = os.environ.get("SOLAR_LLM_MODEL", "energy-assistant").strip() or "energy-assistant"
    api_style: str = os.environ.get("SOLAR_LLM_API_STYLE", "").strip().lower() or "auto"
    api_key: Optional[str] = os.environ.get("SOLAR_LLM_API_KEY", "").strip() or None
    timeout_sec: float = float(os.environ.get("SOLAR_LLM_TIMEOUT_SEC", "45"))

    def base_url(self) -> str:
        return f"http://{self.host}:{self.port}"

    def resolved_api_style(self) -> str:
        if self.api_style in ("openai", "ollama"):
            return self.api_style
        return "ollama" if self.port == 11434 else "openai"


def _build_system_prompt() -> str:
    return (
        "You are an energy control assistant for a residential system with Solis and "
        "Sol-Ark inverters. You receive curated JSON summaries of recent behaviour, "
        "tariffs, forecasts, and user preferences.\n\n"
        "Respond ONLY with a single JSON object matching this exact schema:\n"
        "{\n"
        '  "issue_summary": string,\n'
        '  "recommended_action": string,\n'
        "  \"confidence\": number between 0.0 and 1.0,\n"
        "  \"requires_human_approval\": boolean,\n"
        "  \"proposed_settings\": {\n"
        "    \"target_soc_pct\": number or null,\n"
        "    \"export_limit_kw\": number or null,\n"
        "    \"grid_charge_schedule\": [\n"
        "      {\"start\": \"HH:MM\", \"end\": \"HH:MM\", \"max_power_kw\": number}\n"
        "    ]\n"
        "  },\n"
        "  \"tags\": array of strings,\n"
        '  "notes": string,\n'
        "  \"control_intent\": {\n"
        "    \"inverter\": \"solis1\" | null,\n"
        "    \"work_mode\": \"self_use\" | \"feed_in_priority\" | \"off_grid\" | \"time_of_use\" | null\n"
        "  }\n"
        "}\n\n"
        "When describing unusual or high/low values, cite the actual observed readings in "
        "\"notes\" using concrete numbers from the provided summary, such as load power, PV power, "
        "grid power, and SOC. Do not invent thresholds.\n\n"
        "Do not include any explanation outside of this JSON. If you are unsure, set "
        '"requires_human_approval": true and "confidence" <= 0.6.'
    )


def _http_json_post(url: str, payload: Dict[str, Any], headers: Dict[str, str], timeout: float) -> Dict[str, Any]:
    body = json.dumps(payload).encode("utf-8")
    req = Request(url, data=body, headers={"Content-Type": "application/json", **headers}, method="POST")
    try:
        with urlopen(req, timeout=timeout) as resp:
            data = resp.read().decode("utf-8")
    except HTTPError as e:
        raise RuntimeError(f"LLM HTTP error {e.code}: {e.reason}") from e
    except URLError as e:
        raise RuntimeError(f"LLM unreachable: {e.reason}") from e

    try:
        return json.loads(data)
    except json.JSONDecodeError as e:
        raise RuntimeError("LLM returned non-JSON response") from e


def _extract_assistant_content(openai_response: Dict[str, Any]) -> str:
    """
    Extract the assistant message content from an OpenAI-compatible /v1/chat/completions
    response. Returns the raw string which should be a JSON object.
    """
    try:
        choices: List[Dict[str, Any]] = openai_response["choices"]  # type: ignore[assignment]
        if not choices:
            raise KeyError("no choices")
        content = choices[0]["message"]["content"]
        if not isinstance(content, str):
            raise TypeError("content is not a string")
        return content.strip()
    except Exception as e:  # noqa: BLE001
        raise RuntimeError("Unexpected LLM response format") from e


def _extract_ollama_content(ollama_response: Dict[str, Any]) -> str:
    """Extract the assistant content from an Ollama /api/chat response."""
    try:
        message = ollama_response["message"]
        content = message["content"]
        if not isinstance(content, str):
            raise TypeError("content is not a string")
        return content.strip()
    except Exception as e:  # noqa: BLE001
        raise RuntimeError("Unexpected Ollama response format") from e


def _coerce_float(value: Any, default: float) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def _ensure_bool(value: Any, default: bool) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        v = value.strip().lower()
        if v in ("true", "1", "yes", "on"):
            return True
        if v in ("false", "0", "no", "off"):
            return False
    return default


def _validate_and_normalise(output: Dict[str, Any]) -> Dict[str, Any]:
    """
    Enforce the strict JSON schema and apply conservative defaults / clamping.
    This is the main guardrail before the automation layer consumes the result.
    """
    issue_summary = str(output.get("issue_summary") or "").strip()
    recommended_action = str(output.get("recommended_action") or "").strip()
    confidence = _coerce_float(output.get("confidence"), 0.0)
    if confidence < 0.0:
        confidence = 0.0
    if confidence > 1.0:
        confidence = 1.0

    requires_human_approval = _ensure_bool(output.get("requires_human_approval"), True)

    proposed = output.get("proposed_settings") or {}
    if not isinstance(proposed, dict):
        proposed = {}

    target_soc_pct_val = proposed.get("target_soc_pct")
    target_soc_pct: Optional[float]
    if target_soc_pct_val is None:
        target_soc_pct = None
    else:
        target_soc_pct = _coerce_float(target_soc_pct_val, 0.0)
        # Hard clamp to safe range; automation layer can narrow further.
        if target_soc_pct < 40.0:
            target_soc_pct = 40.0
        if target_soc_pct > 90.0:
            target_soc_pct = 90.0

    export_limit_val = proposed.get("export_limit_kw")
    export_limit_kw: Optional[float]
    if export_limit_val is None:
        export_limit_kw = None
    else:
        export_limit_kw = _coerce_float(export_limit_val, 0.0)
        if export_limit_kw < 0.0:
            export_limit_kw = 0.0

    grid_charge_schedule_raw = proposed.get("grid_charge_schedule") or []
    grid_charge_schedule: List[Dict[str, Any]] = []
    if isinstance(grid_charge_schedule_raw, list):
        for item in grid_charge_schedule_raw:
            if not isinstance(item, dict):
                continue
            start = str(item.get("start") or "").strip()
            end = str(item.get("end") or "").strip()
            if not start or not end:
                continue
            max_power_kw = _coerce_float(item.get("max_power_kw"), 0.0)
            if max_power_kw <= 0:
                continue
            grid_charge_schedule.append(
                {
                    "start": start,
                    "end": end,
                    "max_power_kw": max_power_kw,
                }
            )

    tags_raw = output.get("tags") or []
    tags: List[str] = []
    if isinstance(tags_raw, list):
        for t in tags_raw:
            if isinstance(t, str):
                tt = t.strip()
                if tt:
                    tags.append(tt)

    notes = str(output.get("notes") or "").strip()

    # Optional control_intent block, used by automation layer to map to real writes.
    ci_raw = output.get("control_intent") or {}
    control_intent: Dict[str, Optional[str]] = {"inverter": None, "work_mode": None}
    if isinstance(ci_raw, dict):
        inv = str(ci_raw.get("inverter") or "").strip().lower()
        if inv in ("solis1",):
            control_intent["inverter"] = inv
        wm = str(ci_raw.get("work_mode") or "").strip().lower()
        if wm in ("self_use", "feed_in_priority", "off_grid", "time_of_use"):
            control_intent["work_mode"] = wm

    # If we failed to get any meaningful text, force human approval and low confidence.
    if not issue_summary and not recommended_action:
        requires_human_approval = True
        confidence = min(confidence, 0.5)

    return {
        "issue_summary": issue_summary,
        "recommended_action": recommended_action,
        "confidence": confidence,
        "requires_human_approval": requires_human_approval,
        "proposed_settings": {
            "target_soc_pct": target_soc_pct,
            "export_limit_kw": export_limit_kw,
            "grid_charge_schedule": grid_charge_schedule,
        },
        "tags": tags,
        "notes": notes,
        "control_intent": control_intent,
    }


def call_llm_task(task_type: str, summary: Dict[str, Any], config: Optional[LlmConfig] = None) -> Dict[str, Any]:
    """
    Call the local LLM with a curated summary and a task type.

    task_type examples:
      - "explain_alert"
      - "morning_plan"
      - "evening_strategy"
      - "unexpected_behavior"
    """
    cfg = config or LlmConfig()
    api_style = cfg.resolved_api_style()

    system_prompt = _build_system_prompt()
    user_payload = {
        "task_type": task_type,
        "summary": summary,
    }

    headers: Dict[str, str] = {}
    if cfg.api_key:
        headers["Authorization"] = f"Bearer {cfg.api_key}"

    if api_style == "ollama":
        url = f"{cfg.base_url().rstrip('/')}/api/chat"
        payload: Dict[str, Any] = {
            "model": cfg.model,
            "messages": [
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": json.dumps(user_payload, separators=(",", ":"))},
            ],
            "stream": False,
            "format": "json",
            "options": {
                "temperature": 0.2,
            },
        }
        raw_response = _http_json_post(url, payload, headers, timeout=cfg.timeout_sec)
        content_str = _extract_ollama_content(raw_response)
    else:
        url = f"{cfg.base_url().rstrip('/')}/v1/chat/completions"
        payload = {
            "model": cfg.model,
            "messages": [
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": json.dumps(user_payload, separators=(",", ":"))},
            ],
            "temperature": 0.2,
        }
        raw_response = _http_json_post(url, payload, headers, timeout=cfg.timeout_sec)
        content_str = _extract_assistant_content(raw_response)

    try:
        parsed = json.loads(content_str)
        if not isinstance(parsed, dict):
            raise TypeError("top-level JSON is not an object")
    except Exception as e:  # noqa: BLE001
        raise RuntimeError("LLM did not return a valid JSON object") from e

    return _validate_and_normalise(parsed)


def explain_alert_example(alert: Dict[str, Any]) -> Dict[str, Any]:
    """
    Convenience wrapper for an "explain this alert" call.
    The caller is responsible for constructing a curated alert summary.
    """
    summary = {
        "question": "Explain this alert and whether any immediate action is required.",
        "alert": alert,
    }
    return call_llm_task("explain_alert", summary)


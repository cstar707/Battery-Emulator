"""
Optional LLM integration for Solis S6 app (dashboard chat + automations).

This stub keeps the web app importable when no LLM backend is present.
Replace or extend with a real client (e.g. OpenAI-compatible HTTP) and set env/config as needed.
"""

from __future__ import annotations

import logging
import os
from typing import Any

logger = logging.getLogger(__name__)


def call_llm_task(kind: str, payload: dict[str, Any]) -> str | dict[str, Any]:
    """
    Run an LLM task. Kinds used by main.py:
    - dashboard_chat -> str (shown as assistant answer)
    - morning_plan, evening_strategy, unexpected_behavior -> dict with at least optional "confidence"
    """
    if os.environ.get("SOLIS_LLM_DISABLED", "").strip().lower() in ("1", "true", "yes"):
        return _stub_result(kind)

    # Hook: if you add a real client, gate it on API keys here and fall back to stub.
    logger.debug("llm_client stub: kind=%s (no external LLM configured)", kind)
    return _stub_result(kind)


def _stub_result(kind: str) -> str | dict[str, Any]:
    if kind == "dashboard_chat":
        return (
            "Assistant is not configured: add a real `llm_client.py` implementation or set "
            "SOLIS_LLM_DISABLED=1 to silence this message. The dashboard and APIs work without LLM."
        )
    return {
        "confidence": 0.0,
        "stub": True,
        "kind": kind,
        "message": "LLM automations stub — no external model called.",
    }

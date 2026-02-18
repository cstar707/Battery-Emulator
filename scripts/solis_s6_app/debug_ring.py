"""
Ring buffer for Modbus debug stream. Captures log records from solis_modbus logger.
"""
from __future__ import annotations

import logging
import threading
import time
from collections import deque

# Max lines to keep (oldest dropped)
MAX_LINES = 300

_lock = threading.Lock()
_lines: deque[tuple[float, str, str]] = deque(maxlen=MAX_LINES)  # (ts, level, message)


def add(level: str, message: str) -> None:
    try:
        with _lock:
            _lines.append((time.time(), str(level)[:10], str(message)[:2000]))
    except Exception:
        pass


def get_lines() -> list[tuple[float, str, str]]:
    try:
        with _lock:
            return list(_lines)
    except Exception:
        return []


def clear() -> None:
    try:
        with _lock:
            _lines.clear()
    except Exception:
        pass


class ModbusDebugHandler(logging.Handler):
    """Sends solis_modbus log records to the debug ring buffer."""

    def emit(self, record: logging.LogRecord) -> None:
        try:
            msg = self.format(record) if record else ""
            level = getattr(record, "levelname", "LOG")
            add(level, msg)
        except Exception:
            try:
                self.handleError(record)
            except Exception:
                pass

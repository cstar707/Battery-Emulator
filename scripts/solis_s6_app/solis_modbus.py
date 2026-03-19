"""
Modbus client for Solis S6 hybrid inverter (input registers 33xxx, holding 43xxx).
Connects to INVERTER_HOST:INVERTER_PORT (Solis1). Read-only; writes for storage toggles (43110).

Solis inverter/datalogger does not follow Modbus TCP spec strictly:
  - Does not echo the transaction_id (returns 0 or a different ID).
  - Returns protocol_id=0x0100 (256) instead of 0x0000.
Both cause pymodbus to discard the response. We patch the framer and transaction layer to
accept any transaction_id and any protocol_id so the inverter's replies are processed.
"""
from __future__ import annotations

import logging
import os
import threading
import time
from typing import Any

from pymodbus.client import ModbusTcpClient
from pymodbus.exceptions import ModbusException

from config import (
    get_inverter_host,
    get_inverter_port,
    get_modbus_unit,
    SOLIS_DAILY_PV_SCALE,
    SOLIS_TOTAL_PV_DIVISOR,
)

# Solis inverter does not follow Modbus TCP spec:
#   1. Does not echo transaction_id — pymodbus raises/skips the response.
#   2. Returns protocol_id=256 (0x0100) — pymodbus logs "Invalid Modbus protocol id: 256"
#      and returns empty data, silently discarding every response.
# We patch both so responses are accepted and logs stay clean.
def _solis_modbus_patches():
    # Patch 1: accept any transaction_id in framer
    try:
        from pymodbus.framer import base as _framer_base
        _orig_framer = _framer_base.FramerBase.handleFrame
        def _relaxed_handle(self, data: bytes, exp_devid: int, exp_tid: int):
            return _orig_framer(self, data, exp_devid, 0)  # accept any transaction_id
        _framer_base.FramerBase.handleFrame = _relaxed_handle
    except Exception as e:
        logging.getLogger(__name__).warning("Modbus framer patch failed: %s", e)
    # Patch 2: accept any protocol_id in FramerSocket.decode (Solis returns 0x0100 = 256)
    try:
        from pymodbus.framer.socket import FramerSocket as _FramerSocket
        _orig_decode = _FramerSocket.decode
        def _relaxed_decode(self, data: bytes):
            # Zero out the protocol_id bytes (bytes 2-3) before decoding
            if len(data) >= 4:
                data = data[:2] + b'\x00\x00' + data[4:]
            return _orig_decode(self, data)
        _FramerSocket.decode = _relaxed_decode
    except Exception as e:
        logging.getLogger(__name__).warning("Modbus socket protocol_id patch failed: %s", e)
    # Patch 3: normalize transaction_id on responses
    try:
        from pymodbus.transaction import transaction as _tx
        _orig_sync_get = _tx.TransactionManager.sync_get_response
        def _normalize_tid_sync_get(self, dev_id, tid):
            response = _orig_sync_get(self, dev_id, tid)
            if response and hasattr(response, "transaction_id"):
                response.transaction_id = tid
            return response
        _tx.TransactionManager.sync_get_response = _normalize_tid_sync_get
    except Exception as e:
        logging.getLogger(__name__).warning("Modbus transaction patch failed: %s", e)
    # Silence pymodbus noise — expected with non-compliant Solis firmware
    try:
        logging.getLogger("pymodbus").setLevel(logging.CRITICAL)
    except Exception:
        pass

_solis_modbus_patches()

logger = logging.getLogger(__name__)

# Solis doc register numbers. Default: raw (33000 sent as-is). Set SOLIS_MODBUS_ZERO_BASED=1 for 0-based.
_USE_ZERO_BASED = os.environ.get("SOLIS_MODBUS_ZERO_BASED", "").strip().lower() in ("1", "true", "yes")
_INPUT_BASE = 30001
_HOLDING_BASE = 40001

_BLOCKS = [
    (33000, 41),   # product, serial, time, energy totals
    (33049, 36),   # DC/AC voltages, currents, power
    (33091, 5),    # working mode, temp, grid freq, inverter state
    (33126, 25),   # meter power, battery V/I/SOC, house load, battery power (33149-33150 = 32-bit)
    (33161, 20),   # battery charge/discharge energy, grid import/export, load
]
_REG_STORAGE_CONTROL = 43110   # Storage mode bits
_REG_HYBRID_CONTROL = 43483    # Hybrid function: export, peak-shaving, etc.
_REG_TOU_CHARGE_CURRENT = 43141
_REG_TOU_DISCHARGE_CURRENT = 43142
_REG_TOU_SLOT1_BASE = 43143
_TOU_SLOT_REG_COUNT = 8


def _input_addr(solis_reg: int) -> int:
    return (solis_reg - _INPUT_BASE) if _USE_ZERO_BASED else solis_reg


def _holding_addr(solis_reg: int) -> int:
    return (solis_reg - _HOLDING_BASE) if _USE_ZERO_BASED else solis_reg


# Serialise all Modbus I/O so the background poll and any write never interleave.
# Without this the TID-mismatch bypass lets a poll response get consumed by a write
# request (or vice versa), returning the wrong number of registers and corrupting writes.
_modbus_lock = threading.Lock()


def _get_client(host: str | None = None, port: int | None = None) -> ModbusTcpClient:
    h = host or get_inverter_host()
    p = port if port is not None else get_inverter_port()
    return ModbusTcpClient(host=h, port=p, timeout=4)


def _is_tid_mismatch(exc: Exception) -> bool:
    """True if exception is the known Solis transaction_id mismatch (no need to warn)."""
    try:
        msg = str(exc).lower()
    except Exception:
        return False
    return "transaction" in msg and ("but received" in msg or "but got" in msg)


def _read_input_registers(
    client: ModbusTcpClient, solis_reg: int, count: int, device_id: int | None = None
) -> list[int] | None:
    try:
        address = _input_addr(solis_reg)
        unit = device_id if device_id is not None else get_modbus_unit()
        rr = client.read_input_registers(address=address, count=count, device_id=unit)
        if rr.isError():
            return None
        out = list(rr.registers)
        if len(out) < count:
            # Truncated — reject to avoid wrong/partial data (would show as zeros or garbage).
            logger.warning("read_input_registers %s: expected at least %s registers, got %s (discarding)", solis_reg, count, len(out))
            return None
        # Accept if we got at least count (inverter may return a few extra); use first count for parsing.
        return out[:count] if len(out) > count else out
    except (ModbusException, Exception) as e:
        if _is_tid_mismatch(e):
            logger.debug("read_input_registers %s %s: %s", solis_reg, count, e)
        else:
            logger.warning("read_input_registers %s %s: %s", solis_reg, count, e)
        return None


def _read_holding(
    client: ModbusTcpClient, solis_reg: int, count: int = 1, device_id: int | None = None
) -> list[int] | None:
    try:
        address = _holding_addr(solis_reg)
        unit = device_id if device_id is not None else get_modbus_unit()
        rr = client.read_holding_registers(address=address, count=count, device_id=unit)
        if rr.isError():
            return None
        out = list(rr.registers)
        if len(out) != count:
            # Response register count doesn't match request — likely a cross-contaminated
            # response from a concurrent poll. Reject it rather than using corrupt data.
            logger.warning("read_holding %s: expected %s registers, got %s (discarding)", solis_reg, count, len(out))
            return None
        logger.debug("read_holding %s = %s", solis_reg, out)
        return out
    except (ModbusException, Exception) as e:
        if _is_tid_mismatch(e):
            logger.debug("read_holding %s: %s", solis_reg, e)
        else:
            logger.warning("read_holding %s: %s", solis_reg, e)
        return None


def _write_holding(client: ModbusTcpClient, solis_reg: int, value: int) -> bool:
    try:
        address = _holding_addr(solis_reg)
        wr = client.write_register(address=address, value=value, device_id=get_modbus_unit())
        ok = not wr.isError()
        logger.debug("write_holding %s=%s %s", solis_reg, value, "OK" if ok else "FAIL")
        return ok
    except (ModbusException, Exception) as e:
        if _is_tid_mismatch(e):
            logger.debug("write_holding %s=%s: %s", solis_reg, value, e)
        else:
            logger.warning("write_holding %s=%s: %s", solis_reg, value, e)
        return False


def _write_holding_many(client: ModbusTcpClient, solis_reg: int, values: list[int]) -> bool:
    try:
        address = _holding_addr(solis_reg)
        wr = client.write_registers(address=address, values=values, device_id=get_modbus_unit())
        ok = not wr.isError()
        logger.debug("write_holding_many %s=%s %s", solis_reg, values, "OK" if ok else "FAIL")
        return ok
    except (ModbusException, Exception) as e:
        if _is_tid_mismatch(e):
            logger.debug("write_holding_many %s=%s: %s", solis_reg, values, e)
        else:
            logger.warning("write_holding_many %s=%s: %s", solis_reg, values, e)
        return False


def _reg(regs: list[int] | None, offset: int, default: int = 0) -> int:
    if not regs or offset >= len(regs):
        return default
    return regs[offset]


def _reg_s32(regs: list[int] | None, offset: int) -> int:
    if not regs or offset + 1 >= len(regs):
        return 0
    hi, lo = regs[offset], regs[offset + 1]
    return (hi << 16) | (lo & 0xFFFF)


def _reg_s32_signed(regs: list[int] | None, offset: int) -> int:
    """32-bit signed (power: negative = export / discharge)."""
    u = _reg_s32(regs, offset)
    return u if u < 0x8000_0000 else u - 0x1_0000_0000


_STORAGE_BIT_NAMES = [
    "self_use", "time_of_use", "off_grid", "battery_wakeup", "reserve_battery",
    "allow_grid_charge", "feed_in_priority", "batt_ovc", "forcecharge_peakshaving",
    "battery_current_correction", "battery_healing", "peak_shaving",
]

_STORAGE_SELF_USE_BIT = 0
_STORAGE_OFF_GRID_BIT = 2
_STORAGE_RESERVE_BATTERY_BIT = 4
_STORAGE_ALLOW_GRID_CHARGE_BIT = 5
_STORAGE_FEED_IN_PRIORITY_BIT = 6
_STORAGE_PEAK_SHAVING_BIT = 11
_HYBRID_ALLOW_EXPORT_BIT = 3

# Preserve only benign service/calibration flags when building an exact work-mode value.
# We intentionally clear TOU / reserve / grid-charge / forcecharge helper bits unless the
# caller explicitly requests them so app-driven writes do not inherit stale Solis Cloud state.
_STORAGE_EXACT_PRESERVE_MASK = (
    (1 << 7)   # batt_ovc
    | (1 << 9)  # battery_current_correction
    | (1 << 10)  # battery_healing
)


def _decode_storage_bits(val: int) -> dict[str, bool]:
    return {_STORAGE_BIT_NAMES[i]: bool((val >> i) & 1) for i in range(min(12, len(_STORAGE_BIT_NAMES)))}


def _format_storage_bits(val: int) -> str:
    names = [
        name
        for i, name in enumerate(_STORAGE_BIT_NAMES)
        if bool((val >> i) & 1)
    ]
    return f"{val} ({', '.join(names) if names else 'none'})"


def _build_exact_storage_control_value(
    current_val: int,
    *,
    primary_bits: tuple[int, ...] = (),
    allow_grid_charge: bool = False,
    reserve_battery: bool = False,
) -> int:
    """
    Build a deterministic 43110 value for app-driven control paths.

    We keep only benign service flags from the current register and explicitly rebuild the
    requested operating mode so stale cloud/app helper bits do not leak into new writes.
    """
    val = current_val & _STORAGE_EXACT_PRESERVE_MASK
    for bit in primary_bits:
        val |= 1 << bit
    if allow_grid_charge:
        val |= 1 << _STORAGE_ALLOW_GRID_CHARGE_BIT
    if reserve_battery:
        val |= 1 << _STORAGE_RESERVE_BATTERY_BIT
    return val & 0xFFFF


def _decode_hybrid_bits(val: int) -> dict[str, bool]:
    return {_HYBRID_BIT_NAMES[i]: bool((val >> i) & 1) for i in range(min(8, len(_HYBRID_BIT_NAMES)))}


def poll_solis() -> dict[str, Any]:
    """Read first Solis inverter (backward compat). Prefer poll_solis_for_inverter for multi-connection."""
    with _modbus_lock:
        return _poll_solis_locked(
            get_inverter_host(), get_inverter_port(), get_modbus_unit()
        )


def poll_solis_for_inverter(host: str, port: int, unit: int) -> dict[str, Any]:
    """Read one Solis inverter at host:port with Modbus unit id. Thread-safe."""
    with _modbus_lock:
        return _poll_solis_locked(host, port, unit)


def _poll_solis_locked(
    host: str | None = None, port: int | None = None, unit: int | None = None
) -> dict[str, Any]:
    h = host or get_inverter_host()
    p = port if port is not None else get_inverter_port()
    u = unit if unit is not None else get_modbus_unit()
    client = _get_client(h, p)
    out: dict[str, Any] = {
        "ok": False,
        "grid_power_W": 0,
        "load_power_W": 0,
        "pv_power_W": 0,
        "battery_power_W": 0,
        "battery_soc_pct": 0,
        "battery_voltage_V": 0,
        "battery_current_A": 0,
        "meter_power_W": 0,
        "grid_freq_Hz": 0,
        "inverter_temp_C": 0,
        "inverter_state": 0,
        "storage_control_raw": 0,
        "energy_today_pv_kWh": 0,
        "total_pv_energy_kWh": 0,
        "energy_today_load_kWh": 0,
        "raw_blocks": {},
    }
    try:
        if not client.connect():
            logger.warning("poll_solis: connect failed to %s:%s", h, p)
            return out
        try:
            all_regs: dict[int, list[int]] = {}
            for addr, count in _BLOCKS:
                r = _read_input_registers(client, addr, count, device_id=u)
                if r is None:
                    time.sleep(0.12)
                    r = _read_input_registers(client, addr, count, device_id=u)
                if r is None and addr == 33049:
                    time.sleep(0.12)
                    r = _read_input_registers(client, addr, count, device_id=u)
                if r is None and addr == 33000:
                    time.sleep(0.12)
                    r = _read_input_registers(client, addr, count, device_id=u)
                if r is not None:
                    all_regs[addr] = r
                time.sleep(0.06)
            time.sleep(0.06)
            h_reg = _read_holding(client, _REG_STORAGE_CONTROL, 1, device_id=u)
            if h_reg is not None:
                all_regs[_REG_STORAGE_CONTROL] = h_reg
            time.sleep(0.06)
            h2 = _read_holding(client, _REG_HYBRID_CONTROL, 1, device_id=u)
            if h2 is not None:
                all_regs[_REG_HYBRID_CONTROL] = h2

            out["raw_blocks"] = {str(k): v for k, v in all_regs.items()}
            out["ok"] = len(all_regs) > 0  # only true if we got at least one block

            # Log poll result so Debug stream shows misses (truncated/TID mix = missing block)
            expected = len(_BLOCKS) + 2  # input blocks + 43110 + 43483
            got = len(all_regs)
            if got < expected:
                missing = [a for a, _ in _BLOCKS if a not in all_regs]
                missing.extend(
                    [r for r in (_REG_STORAGE_CONTROL, _REG_HYBRID_CONTROL) if r not in all_regs]
                )
                logger.info("poll_solis: %s/%s blocks (missing %s — check WARNING above for truncation/TID)", got, expected, missing)
            else:
                logger.debug("poll_solis ok blocks=%s", got)

            # Block 33000-33040 (33029-30=Total, 33035=Today, 0.1 kWh units)
            b0 = all_regs.get(33000)
            if b0 and len(b0) >= 37:
                raw_33035 = _reg(b0, 35, 0)
                out["energy_today_pv_kWh"] = (raw_33035 / 10.0) * SOLIS_DAILY_PV_SCALE  # 33035 = today 0.1 kWh
                out["_raw_33035"] = raw_33035  # for debug when value seems wrong
                out["product_model"] = _reg(b0, 0)
                if len(b0) >= 31:
                    raw_total = (_reg(b0, 29, 0) << 16) | _reg(b0, 30, 0)  # 33029-33030 32-bit total PV (S6: 0.01 kWh)
                    out["total_pv_energy_kWh"] = (raw_total / SOLIS_TOTAL_PV_DIVISOR) * SOLIS_DAILY_PV_SCALE
                    out["_raw_33029_30"] = raw_total

            # Block 33049-33084
            b1 = all_regs.get(33049)
            if b1 and len(b1) >= 32:
                out["pv_power_W"] = _reg_s32(b1, 8)   # 33057-33058 total DC (unsigned)
                out["active_power_W"] = _reg_s32_signed(b1, 30)  # 33079-33080 signed (export negative)
                out["pv_voltage_1_V"] = _reg(b1, 0) / 10.0
                out["pv_current_1_A"] = _reg(b1, 1) / 10.0
                out["pv_voltage_2_V"] = _reg(b1, 2) / 10.0
                out["pv_current_2_A"] = _reg(b1, 3) / 10.0
                out["ac_voltage_V"] = _reg(b1, 24) / 10.0   # 33073 phase A
                out["ac_current_A"] = _reg(b1, 27) / 10.0   # 33076 phase A

            # Block 33091-33095
            b2 = all_regs.get(33091)
            if b2 and len(b2) >= 5:
                out["inverter_temp_C"] = _reg(b2, 2) / 10.0
                out["grid_freq_Hz"] = _reg(b2, 3) / 100.0
                out["inverter_state"] = _reg(b2, 4)

            # Block 33126-33150 (25 regs so 33149-33150 are both present for 32-bit battery power)
            b3 = all_regs.get(33126)
            if b3 and len(b3) >= 25:
                out["meter_power_W"] = _reg_s32_signed(b3, 4)   # 33130 signed (+ import, - export)
                out["battery_voltage_V"] = _reg(b3, 7) / 10.0
                out["battery_current_A"] = _reg(b3, 8) / 10.0
                out["battery_soc_pct"] = _reg(b3, 13)
                out["battery_soh_pct"] = _reg(b3, 14)
                out["house_load_W"] = _reg(b3, 21)
                out["battery_power_W"] = _reg_s32_signed(b3, 23)  # 33149-33150 signed (charge/discharge)
                out["storage_control_raw"] = _reg(b3, 6)  # 33132 mirror of 43110

            # If we have holding 43110, prefer it for storage_control
            if all_regs.get(_REG_STORAGE_CONTROL):
                out["storage_control_raw"] = all_regs[_REG_STORAGE_CONTROL][0]
            if all_regs.get(_REG_HYBRID_CONTROL):
                out["hybrid_control_raw"] = all_regs[_REG_HYBRID_CONTROL][0]
            else:
                out["hybrid_control_raw"] = 0

            # Block 33161-33180
            b4 = all_regs.get(33161)
            if b4 and len(b4) >= 20:
                out["energy_today_load_kWh"] = _reg(b4, 18) / 10.0  # 33179
                out["energy_today_bat_charge_kWh"] = _reg(b4, 2) / 10.0
                out["energy_today_bat_discharge_kWh"] = _reg(b4, 6) / 10.0
                out["energy_today_grid_import_kWh"] = _reg(b4, 10) / 10.0
                out["energy_today_grid_export_kWh"] = _reg(b4, 14) / 10.0

            # Alias for dashboard (grid = meter; PV = DC register only)
            out["grid_power_W"] = out.get("meter_power_W", 0)  # signed: - = export
            out["load_power_W"] = out.get("house_load_W", 0)
            # PV: use only the DC register (33057-33058). Do NOT fallback to active_power —
            # at night active_power can be battery export, which would wrongly show as solar.

            # Cached bits for Control page (avoid blocking Modbus on GET /control)
            if all_regs.get(_REG_STORAGE_CONTROL):
                out["storage_bits"] = _decode_storage_bits(all_regs[_REG_STORAGE_CONTROL][0])
            else:
                out["storage_bits"] = {n: False for n in _STORAGE_BIT_NAMES}
            if all_regs.get(_REG_HYBRID_CONTROL):
                out["hybrid_bits"] = _decode_hybrid_bits(all_regs[_REG_HYBRID_CONTROL][0])
            else:
                out["hybrid_bits"] = {n: False for n in _HYBRID_BIT_NAMES}
            logger.debug("poll_solis ok blocks=%s", len(all_regs))
        finally:
            client.close()
    except Exception as e:
        logger.exception("poll_solis: %s", e)
    return out


def get_storage_control_bits() -> dict[str, bool]:
    """Read holding 43110 and return dict of bit name -> on/off."""
    names = [
        "self_use", "time_of_use", "off_grid", "battery_wakeup", "reserve_battery",
        "allow_grid_charge", "feed_in_priority", "batt_ovc", "forcecharge_peakshaving",
        "battery_current_correction", "battery_healing", "peak_shaving",
    ]
    client = _get_client()
    try:
        if not client.connect():
            return {n: False for n in names}
        h = _read_holding(client, _REG_STORAGE_CONTROL, 1)
        client.close()
        if not h:
            return {n: False for n in names}
        val = h[0]
        return {names[i]: bool((val >> i) & 1) for i in range(min(12, len(names)))}
    except Exception:
        return {n: False for n in names}


def set_storage_control_bits(changes: dict) -> bool:
    """Read 43110, apply multiple bit changes atomically, write back, read back to confirm.

    Args:
        changes: dict mapping bit_index (int) -> desired_state (bool)
                 e.g. {0: True, 6: False} to enable self-use and disable feed-in together.
    """
    with _modbus_lock:
        return _set_storage_control_bits_locked(changes)


def _set_storage_control_bits_locked(changes: dict) -> bool:
    client = _get_client()
    try:
        if not client.connect():
            return False
        h = _read_holding(client, _REG_STORAGE_CONTROL, 1)
        if not h:
            client.close()
            return False
        original = h[0]
        val = original
        for bit_index, on in changes.items():
            if on:
                val |= 1 << bit_index
            else:
                val &= ~(1 << bit_index)
        expected = val & 0xFFFF
        if not _write_holding(client, _REG_STORAGE_CONTROL, expected):
            client.close()
            return False
        time.sleep(0.3)
        rb = _read_holding(client, _REG_STORAGE_CONTROL, 1)
        client.close()
        if not rb:
            logger.warning("set_storage_control_bits: read-back failed after write")
            return False
        got = rb[0]
        # Verify only the bits we explicitly changed — the inverter may autonomously adjust
        # other bits (e.g. clearing feed_in when self_use is set) which is correct behaviour.
        bit_ok = all(
            bool(got & (1 << b)) == bool(on)
            for b, on in changes.items()
        )
        if not bit_ok:
            logger.warning(
                "set_storage_control_bits: bit mismatch — changes=%s before=%s expected=%s got=%s",
                changes,
                _format_storage_bits(original),
                _format_storage_bits(expected),
                _format_storage_bits(got),
            )
        else:
            logger.info(
                "set_storage_control_bits: confirmed — changes=%s before=%s expected=%s got=%s",
                changes,
                _format_storage_bits(original),
                _format_storage_bits(expected),
                _format_storage_bits(got),
            )
        return bit_ok
    except Exception as e:
        logger.exception("set_storage_control_bits: %s", e)
        return False


def set_storage_control_bit(bit_index: int, on: bool) -> bool:
    """Read 43110, set or clear one bit, write back, then read back to confirm. bit_index 0..11."""
    with _modbus_lock:
        return _set_storage_control_bit_locked(bit_index, on)


def _set_storage_control_bit_locked(bit_index: int, on: bool) -> bool:
    client = _get_client()
    try:
        if not client.connect():
            return False
        h = _read_holding(client, _REG_STORAGE_CONTROL, 1)
        if not h:
            client.close()
            return False
        original = h[0]
        val = original
        if on:
            val |= 1 << bit_index
        else:
            val &= ~(1 << bit_index)
        expected = val & 0xFFFF
        if not _write_holding(client, _REG_STORAGE_CONTROL, expected):
            client.close()
            return False
        # Read back to confirm the inverter applied the change
        time.sleep(0.3)
        rb = _read_holding(client, _REG_STORAGE_CONTROL, 1)
        client.close()
        if not rb:
            logger.warning("set_storage_control_bit: read-back failed after write")
            return False
        confirmed = (rb[0] == expected)
        if not confirmed:
            logger.warning(
                "set_storage_control_bit: write mismatch — bit=%s on=%s before=%s expected=%s got=%s",
                bit_index,
                on,
                _format_storage_bits(original),
                _format_storage_bits(expected),
                _format_storage_bits(rb[0]),
            )
        else:
            logger.info(
                "set_storage_control_bit: confirmed — bit=%s on=%s before=%s expected=%s got=%s",
                bit_index,
                on,
                _format_storage_bits(original),
                _format_storage_bits(expected),
                _format_storage_bits(rb[0]),
            )
        return confirmed
    except Exception as e:
        logger.exception("set_storage_control_bit: %s", e)
        return False


# 43483 bit names (Hybrid function control)
_HYBRID_BIT_NAMES = [
    "dual_backup", "ac_coupling", "smart_load_forced", "allow_export",
    "backup2load_auto", "backup2load_manual", "smart_load_offgrid_stop", "grid_peakshaving",
]


def get_hybrid_control_bits() -> dict[str, bool]:
    """Read holding 43483 and return dict of bit name -> on/off (export, peak-shaving, etc.)."""
    client = _get_client()
    try:
        if not client.connect():
            return {n: False for n in _HYBRID_BIT_NAMES}
        h = _read_holding(client, _REG_HYBRID_CONTROL, 1)
        client.close()
        if not h:
            return {n: False for n in _HYBRID_BIT_NAMES}
        val = h[0]
        return {_HYBRID_BIT_NAMES[i]: bool((val >> i) & 1) for i in range(min(8, len(_HYBRID_BIT_NAMES)))}
    except Exception:
        return {n: False for n in _HYBRID_BIT_NAMES}


def set_hybrid_control_bit(bit_index: int, on: bool) -> bool:
    """Read 43483, set or clear one bit, write back, then read back to confirm. bit_index 0..7."""
    with _modbus_lock:
        return _set_hybrid_control_bit_locked(bit_index, on)


def _set_hybrid_control_bit_locked(bit_index: int, on: bool) -> bool:
    client = _get_client()
    try:
        if not client.connect():
            return False
        h = _read_holding(client, _REG_HYBRID_CONTROL, 1)
        if not h:
            client.close()
            return False
        val = h[0]
        if on:
            val |= 1 << bit_index
        else:
            val &= ~(1 << bit_index)
        expected = val & 0xFFFF
        if not _write_holding(client, _REG_HYBRID_CONTROL, expected):
            client.close()
            return False
        time.sleep(0.3)
        rb = _read_holding(client, _REG_HYBRID_CONTROL, 1)
        client.close()
        if not rb:
            logger.warning("set_hybrid_control_bit: read-back failed after write")
            return False
        confirmed = (rb[0] == expected)
        if not confirmed:
            logger.warning("set_hybrid_control_bit: write mismatch — expected %s, got %s", expected, rb[0])
        return confirmed
    except Exception as e:
        logger.exception("set_hybrid_control_bit: %s", e)
        return False


# ── Active power limit (export curtailment) ─────────────────────────────────
# Register 43070: power limit switch.  0xAA (170) = enabled, 0x55 (85) = disabled.
# Register 43052: power limit percentage. 10000 = 100%, 0 = 0% (full curtailment).
# Scale: value = desired_percent * 100  (e.g. 50% → 5000)
_REG_POWER_LIMIT_SWITCH = 43070
_REG_POWER_LIMIT_PCT    = 43052
_POWER_LIMIT_ENABLE     = 0xAA   # 170 — enable the limit
_POWER_LIMIT_DISABLE    = 0x55   #  85 — disable (restore to rated power)

_TOU_CURRENT_MIN_A = 0.0
_TOU_CURRENT_MAX_A = 70.0


def get_active_power_limit() -> dict:
    """Read current power limit state: {'enabled': bool, 'limit_pct': float, 'switch_raw': int}."""
    with _modbus_lock:
        client = _get_client()
        try:
            if not client.connect():
                return {"enabled": False, "limit_pct": 100.0, "switch_raw": 0, "ok": False}
            sw = _read_holding(client, _REG_POWER_LIMIT_SWITCH, 1)
            pct = _read_holding(client, _REG_POWER_LIMIT_PCT, 1)
            client.close()
            sw_val = sw[0] if sw else 0
            pct_val = pct[0] if pct else 10000
            return {
                "ok": True,
                "enabled": sw_val == _POWER_LIMIT_ENABLE,
                "limit_pct": round(pct_val / 100.0, 1),
                "switch_raw": sw_val,
            }
        except Exception as e:
            logger.exception("get_active_power_limit: %s", e)
            return {"enabled": False, "limit_pct": 100.0, "switch_raw": 0, "ok": False}


def set_active_power_limit(limit_pct: float) -> bool:
    """
    Set active power output limit as a percentage of rated power (0–100).
    limit_pct=0   → full curtailment (0W output, limit enabled)
    limit_pct=100 → disable limit (restore to rated power)
    Intermediate values ramp output proportionally (e.g. 50 → 50% of rated watts).
    """
    with _modbus_lock:
        return _set_active_power_limit_locked(limit_pct)


def _set_active_power_limit_locked(limit_pct: float) -> bool:
    limit_pct = max(0.0, min(100.0, float(limit_pct)))
    client = _get_client()
    try:
        if not client.connect():
            return False

        if limit_pct >= 100.0:
            # Disable the limit entirely — restore to full rated output
            if not _write_holding(client, _REG_POWER_LIMIT_SWITCH, _POWER_LIMIT_DISABLE):
                client.close()
                return False
            time.sleep(0.2)
            rb = _read_holding(client, _REG_POWER_LIMIT_SWITCH, 1)
            client.close()
            ok = bool(rb and rb[0] == _POWER_LIMIT_DISABLE)
            if ok:
                logger.info("set_active_power_limit: limit disabled (full power restored)")
            else:
                logger.warning("set_active_power_limit: disable mismatch — got %s", rb[0] if rb else None)
            return ok

        # Enable limit and set percentage
        limit_val = round(limit_pct * 100)   # 50% → 5000
        if not _write_holding(client, _REG_POWER_LIMIT_PCT, limit_val):
            client.close()
            return False
        time.sleep(0.1)
        if not _write_holding(client, _REG_POWER_LIMIT_SWITCH, _POWER_LIMIT_ENABLE):
            client.close()
            return False
        time.sleep(0.3)
        # Read back both registers to confirm
        rb_sw  = _read_holding(client, _REG_POWER_LIMIT_SWITCH, 1)
        rb_pct = _read_holding(client, _REG_POWER_LIMIT_PCT, 1)
        client.close()
        sw_ok  = bool(rb_sw  and rb_sw[0]  == _POWER_LIMIT_ENABLE)
        pct_ok = bool(rb_pct and rb_pct[0] == limit_val)
        if sw_ok and pct_ok:
            logger.info("set_active_power_limit: confirmed %.1f%% (reg=%d)", limit_pct, limit_val)
        else:
            logger.warning("set_active_power_limit: mismatch — sw=%s pct=%s", rb_sw, rb_pct)
        return sw_ok and pct_ok
    except Exception as e:
        logger.exception("set_active_power_limit: %s", e)
        return False


def get_tou_config() -> dict:
    """Read TOU current settings and slot 1 schedule."""
    with _modbus_lock:
        client = _get_client()
        try:
            if not client.connect():
                return {"ok": False, "error": "Modbus connect failed"}
            charge = _read_holding(client, _REG_TOU_CHARGE_CURRENT, 1)
            discharge = _read_holding(client, _REG_TOU_DISCHARGE_CURRENT, 1)
            slot1 = _read_holding(client, _REG_TOU_SLOT1_BASE, _TOU_SLOT_REG_COUNT)
            client.close()
            if not charge or not discharge or not slot1:
                return {"ok": False, "error": "TOU read failed"}
            slot = {
                "charge_start_h": slot1[0],
                "charge_start_m": slot1[1],
                "charge_end_h": slot1[2],
                "charge_end_m": slot1[3],
                "discharge_start_h": slot1[4],
                "discharge_start_m": slot1[5],
                "discharge_end_h": slot1[6],
                "discharge_end_m": slot1[7],
            }
            return {
                "ok": True,
                "charge_amps": round(charge[0] / 10.0, 1),
                "discharge_amps": round(discharge[0] / 10.0, 1),
                "slot1_raw": slot1,
                "slot1": slot,
            }
        except Exception as e:
            logger.exception("get_tou_config: %s", e)
            return {"ok": False, "error": str(e)}


def _clamp_tou_amps(amps: float) -> float:
    return max(_TOU_CURRENT_MIN_A, min(_TOU_CURRENT_MAX_A, float(amps)))


def _set_tou_current_locked(reg: int, amps: float, label: str) -> dict:
    client = _get_client()
    writes: list[str] = []
    try:
        if not client.connect():
            return {"ok": False, "message": "Modbus connect failed", "writes": writes}
        amps = _clamp_tou_amps(amps)
        reg_value = int(round(amps * 10.0))
        before = _read_holding(client, reg, 1)
        if not _write_holding(client, reg, reg_value):
            client.close()
            return {"ok": False, "message": f"Write {reg} failed", "writes": writes}
        writes.append(f"{reg}={reg_value} ({amps:.1f} A)")
        time.sleep(0.3)
        after = _read_holding(client, reg, 1)
        client.close()
        if not after:
            logger.warning("%s: read-back failed after write", label)
            return {"ok": False, "message": "read-back failed", "writes": writes}
        logger.info(
            "%s: confirmed — reg=%s before=%s target=%s got=%s",
            label,
            reg,
            before[0] if before else None,
            reg_value,
            after[0],
        )
        return {
            "ok": after[0] == reg_value,
            "message": f"{label} set to {amps:.1f} A",
            "writes": writes,
            "amps": amps,
        }
    except Exception as e:
        logger.exception("%s: %s", label, e)
        return {"ok": False, "message": str(e), "writes": writes}


def set_tou_charge_amps(amps: float) -> dict:
    with _modbus_lock:
        return _set_tou_current_locked(_REG_TOU_CHARGE_CURRENT, amps, "set_tou_charge_amps")


def set_tou_discharge_amps(amps: float) -> dict:
    with _modbus_lock:
        return _set_tou_current_locked(_REG_TOU_DISCHARGE_CURRENT, amps, "set_tou_discharge_amps")


def set_tou_slot1(
    *,
    charge_start_h: int,
    charge_start_m: int,
    charge_end_h: int,
    charge_end_m: int,
    discharge_start_h: int,
    discharge_start_m: int,
    discharge_end_h: int,
    discharge_end_m: int,
) -> dict:
    with _modbus_lock:
        client = _get_client()
        writes: list[str] = []
        try:
            if not client.connect():
                return {"ok": False, "message": "Modbus connect failed", "writes": writes}
            values = [
                int(charge_start_h), int(charge_start_m),
                int(charge_end_h), int(charge_end_m),
                int(discharge_start_h), int(discharge_start_m),
                int(discharge_end_h), int(discharge_end_m),
            ]
            before = _read_holding(client, _REG_TOU_SLOT1_BASE, _TOU_SLOT_REG_COUNT)
            if not _write_holding_many(client, _REG_TOU_SLOT1_BASE, values):
                client.close()
                return {"ok": False, "message": "TOU slot1 write failed", "writes": writes}
            writes.append(f"{_REG_TOU_SLOT1_BASE}={values}")
            time.sleep(0.3)
            after = _read_holding(client, _REG_TOU_SLOT1_BASE, _TOU_SLOT_REG_COUNT)
            client.close()
            if not after:
                logger.warning("set_tou_slot1: read-back failed after write")
                return {"ok": False, "message": "read-back failed", "writes": writes}
            logger.info(
                "set_tou_slot1: confirmed — before=%s target=%s got=%s",
                before,
                values,
                after,
            )
            return {"ok": after == values, "message": "TOU slot 1 updated", "writes": writes}
        except Exception as e:
            logger.exception("set_tou_slot1: %s", e)
            return {"ok": False, "message": str(e), "writes": writes}


def apply_clean_self_use() -> dict:
    """Set clean self-use and clear export/grid-charge helper state."""
    with _modbus_lock:
        client = _get_client()
        writes: list[str] = []
        try:
            if not client.connect():
                return {"ok": False, "message": "Modbus connect failed", "writes": writes}
            storage = _read_holding(client, _REG_STORAGE_CONTROL, 1)
            hybrid = _read_holding(client, _REG_HYBRID_CONTROL, 1)
            if not storage or not hybrid:
                client.close()
                return {"ok": False, "message": "Read failed", "writes": writes}
            new_storage = _build_exact_storage_control_value(
                storage[0],
                primary_bits=(_STORAGE_SELF_USE_BIT,),
            )
            new_hybrid = hybrid[0] & ~(1 << _HYBRID_ALLOW_EXPORT_BIT)
            if new_storage != storage[0] and _write_holding(client, _REG_STORAGE_CONTROL, new_storage):
                writes.append(f"43110=0x{new_storage:04X} (self-use)")
            time.sleep(0.1)
            if new_hybrid != hybrid[0] and _write_holding(client, _REG_HYBRID_CONTROL, new_hybrid):
                writes.append(f"43483=0x{new_hybrid:04X} (allow export off)")
            client.close()
            return {"ok": True, "message": "Self-use applied", "writes": writes}
        except Exception as e:
            logger.exception("apply_clean_self_use: %s", e)
            return {"ok": False, "message": str(e), "writes": writes}


def apply_tou_charge_mode(*, charge_amps: float) -> dict:
    """Apply self-use + TOU + allow-grid-charge, then set TOU charge amps."""
    with _modbus_lock:
        client = _get_client()
        writes: list[str] = []
        try:
            if not client.connect():
                return {"ok": False, "message": "Modbus connect failed", "writes": writes}
            storage = _read_holding(client, _REG_STORAGE_CONTROL, 1)
            hybrid = _read_holding(client, _REG_HYBRID_CONTROL, 1)
            if not storage or not hybrid:
                client.close()
                return {"ok": False, "message": "Read failed", "writes": writes}
            new_storage = _build_exact_storage_control_value(
                storage[0],
                primary_bits=(_STORAGE_SELF_USE_BIT,),
                allow_grid_charge=True,
            ) | (1 << 1)
            new_hybrid = hybrid[0] & ~(1 << _HYBRID_ALLOW_EXPORT_BIT)
            if new_storage != storage[0]:
                logger.info(
                    "apply_tou_charge_mode: 43110 before=%s target=%s",
                    _format_storage_bits(storage[0]),
                    _format_storage_bits(new_storage),
                )
                if _write_holding(client, _REG_STORAGE_CONTROL, new_storage):
                    writes.append(f"43110=0x{new_storage:04X} (self-use + TOU + grid charge)")
            time.sleep(0.1)
            if new_hybrid != hybrid[0] and _write_holding(client, _REG_HYBRID_CONTROL, new_hybrid):
                writes.append(f"43483=0x{new_hybrid:04X} (allow export off)")
            time.sleep(0.1)
            amps = _clamp_tou_amps(charge_amps)
            reg_value = int(round(amps * 10.0))
            if _write_holding(client, _REG_TOU_CHARGE_CURRENT, reg_value):
                writes.append(f"{_REG_TOU_CHARGE_CURRENT}={reg_value} ({amps:.1f} A)")
            time.sleep(0.3)
            rb_storage = _read_holding(client, _REG_STORAGE_CONTROL, 1)
            rb_charge = _read_holding(client, _REG_TOU_CHARGE_CURRENT, 1)
            client.close()
            ok = bool(rb_storage and rb_charge and rb_storage[0] == new_storage and rb_charge[0] == reg_value)
            if ok:
                logger.info(
                    "apply_tou_charge_mode: confirmed — storage=%s charge_current=%s",
                    rb_storage[0],
                    rb_charge[0],
                )
            else:
                logger.warning(
                    "apply_tou_charge_mode: mismatch — target_storage=%s got_storage=%s target_current=%s got_current=%s",
                    new_storage,
                    rb_storage[0] if rb_storage else None,
                    reg_value,
                    rb_charge[0] if rb_charge else None,
                )
            return {"ok": ok, "message": "TOU charge mode applied", "writes": writes}
        except Exception as e:
            logger.exception("apply_tou_charge_mode: %s", e)
            return {"ok": False, "message": str(e), "writes": writes}


def apply_use_all_solar_preset() -> bool:
    """
    Preset: Self-use, no export — use all solar for load and battery (load-based; no export to grid).
    Sets: Self-Use ON (43110.0), Feed-in OFF (43110.6), Allow export OFF (43483.3).
    """
    with _modbus_lock:
        return _apply_use_all_solar_preset_locked()


def _apply_use_all_solar_preset_locked() -> bool:
    client = _get_client()
    try:
        if not client.connect():
            return False
        ok = True
        # 43110: normalize to clean self-use.
        h = _read_holding(client, _REG_STORAGE_CONTROL, 1)
        if h:
            val = _build_exact_storage_control_value(
                h[0],
                primary_bits=(_STORAGE_SELF_USE_BIT,),
            )
            logger.info(
                "apply_use_all_solar_preset: 43110 before=%s target=%s",
                _format_storage_bits(h[0]),
                _format_storage_bits(val & 0xFFFF),
            )
            ok = _write_holding(client, _REG_STORAGE_CONTROL, val & 0xFFFF) and ok
        # 43483: clear allow_export (3) — load-based: no export
        h2 = _read_holding(client, _REG_HYBRID_CONTROL, 1)
        if h2:
            val2 = h2[0]
            val2 &= ~(1 << 3)  # allow_export off
            ok = _write_holding(client, _REG_HYBRID_CONTROL, val2 & 0xFFFF) and ok
        client.close()
        return ok
    except Exception as e:
        logger.exception("apply_use_all_solar_preset: %s", e)
        return False


# ── Grid charge limits (11.4 kW max for S6-EH1P11.4k) ─────────────────────────
# 43110 BIT05: allow grid charge (bit polarity is inverter-specific; on this system bit=1 enables)
# 43117: max charge current (0.1 A units), 70A = 700
# 43130: battery charge power limit (10 W units), 11400 W = 1140
# 43027: force-charge power limitation (W)
# 43132=2, 43128=negative: remote control import (dead-man, refresh ~4 min)
_REG_MAX_CHARGE = 43117
_REG_CHARGE_LIMIT_10W = 43130
_REG_FORCE_CHARGE_POWER = 43027
_REG_REMOTE_POWER = 43128
_REG_REMOTE_MODE = 43132


def arm_grid_charge(
    *,
    charge_limit_watts: int = 11400,
    max_amps: float = 70.0,
) -> dict:
    """
    Arm/enable grid charge capability (work-mode + limits) WITHOUT setting remote power.

    This is intended to be called rarely:
      - once when enabling charge control
      - when we detect the inverter dropped the allow-grid-charge gate

    Returns {"ok": bool, "message": str, "writes": list}.
    """
    with _modbus_lock:
        return _arm_grid_charge_locked(charge_limit_watts=charge_limit_watts, max_amps=max_amps)


def _arm_grid_charge_locked(*, charge_limit_watts: int, max_amps: float) -> dict:
    client = _get_client()
    writes: list[str] = []
    try:
        if not client.connect():
            return {"ok": False, "message": "Modbus connect failed", "writes": []}

        # Normalize 43110 to a clean grid-charge-capable state instead of OR-ing bit 5 onto
        # whatever Solis Cloud or prior tests left behind.
        h = _read_holding(client, _REG_STORAGE_CONTROL, 1)
        if not h:
            client.close()
            return {"ok": False, "message": "Read 43110 failed", "writes": writes}
        mode = h[0]
        new_mode = _build_exact_storage_control_value(
            mode,
            primary_bits=(_STORAGE_SELF_USE_BIT,),
            allow_grid_charge=True,
        )
        if new_mode != mode:
            logger.info(
                "arm_grid_charge: 43110 before=%s target=%s",
                _format_storage_bits(mode),
                _format_storage_bits(new_mode),
            )
            if _write_holding(client, _REG_STORAGE_CONTROL, new_mode):
                writes.append(f"43110=0x{new_mode:04X} (allow grid charge)")
        time.sleep(0.1)

        # 43135=0: ensure RC force charge/discharge is not overriding remote control
        if _write_holding(client, 43135, 0):
            writes.append("43135=0")
        time.sleep(0.1)

        # 43117: max charge current (0.1 A)
        new_chg = int(max(1, min(70, max_amps)) * 10)
        if _write_holding(client, _REG_MAX_CHARGE, new_chg):
            writes.append(f"43117={new_chg} ({new_chg * 0.1} A)")
        time.sleep(0.1)

        # 43130: charge limit (10 W units)
        limit_43130 = max(500, charge_limit_watts // 10)
        if _write_holding(client, _REG_CHARGE_LIMIT_10W, limit_43130):
            writes.append(f"43130={limit_43130} ({limit_43130 * 10} W)")
        time.sleep(0.1)

        # 43027: force-charge power limitation (W)
        if _write_holding(client, _REG_FORCE_CHARGE_POWER, charge_limit_watts):
            writes.append(f"43027={charge_limit_watts} W")
        time.sleep(0.1)

        client.close()
        msg = "Grid charge armed" if writes else "Already armed"
        logger.info("arm_grid_charge: %s — %s", msg, writes)
        return {"ok": True, "message": msg, "writes": writes}
    except Exception as e:
        logger.exception("arm_grid_charge: %s", e)
        return {"ok": False, "message": str(e), "writes": writes}


def set_remote_import_watts(*, import_watts: int) -> dict:
    """
    Update ONLY the remote-control power command for import.

    This is safe to call frequently (subject to your own cooldown/deadband) because it avoids
    touching 43110 work-mode bits. It keeps the dead-man alive (remote mode + power).
    """
    with _modbus_lock:
        return _set_remote_import_watts_locked(import_watts=import_watts)


def _set_remote_import_watts_locked(*, import_watts: int) -> dict:
    client = _get_client()
    writes: list[str] = []
    try:
        if not client.connect():
            return {"ok": False, "message": "Modbus connect failed", "writes": []}

        import_watts = max(0, min(11400, int(import_watts)))
        power_val = -(import_watts // 10) & 0xFFFF

        if _write_holding(client, _REG_REMOTE_MODE, 2):
            writes.append("43132=2 (remote control)")
        if _write_holding(client, _REG_REMOTE_POWER, power_val):
            writes.append(f"43128={power_val} ({import_watts} W import)")

        client.close()
        msg = f"Remote import {import_watts} W" if writes else "No changes needed"
        logger.info("set_remote_import_watts: %s — %s", msg, writes)
        return {"ok": True, "message": msg, "writes": writes}
    except Exception as e:
        logger.exception("set_remote_import_watts: %s", e)
        return {"ok": False, "message": str(e), "writes": writes}


def set_grid_charge_limits(
    import_watts: int = 11400,
    charge_limit_watts: int = 11400,
    max_amps: float = 70.0,
) -> dict:
    """
    Enable grid charge at full 11.4 kW. Sets 43110 BIT05=0, 43117, 43130, 43027,
    and optionally 43132/43128 for forced import. Uses Modbus lock.
    Returns {"ok": bool, "message": str, "writes": list}.
    """
    with _modbus_lock:
        return _set_grid_charge_limits_locked(import_watts, charge_limit_watts, max_amps)


def _set_grid_charge_limits_locked(
    import_watts: int,
    charge_limit_watts: int,
    max_amps: float,
) -> dict:
    writes: list[str] = []
    try:
        # Phase A: arm grid charge (work-mode + limits)
        r1 = _arm_grid_charge_locked(charge_limit_watts=charge_limit_watts, max_amps=max_amps)
        if not r1.get("ok"):
            return r1
        writes.extend(r1.get("writes", []))

        # Phase B: set remote import power (dead-man)
        if import_watts and import_watts > 0:
            r2 = _set_remote_import_watts_locked(import_watts=import_watts)
            if not r2.get("ok"):
                return r2
            writes.extend(r2.get("writes", []))

        msg = "Grid charge enabled" if writes else "No changes needed"
        logger.info("set_grid_charge_limits: %s — %s", msg, writes)
        return {"ok": True, "message": msg, "writes": writes}
    except Exception as e:
        logger.exception("set_grid_charge_limits: %s", e)
        return {"ok": False, "message": str(e), "writes": writes}


# ── Export (force power to grid) ─────────────────────────────────────────────
# 43132=2, 43128=positive: remote control export. Dead-man ~4 min.
# 43074: EPM export cap (100 W units). 114 = 11.4 kW.
_REG_EPM_LIMIT = 43074
_MODE_FEED_IN_ONLY = (1 << _STORAGE_FEED_IN_PRIORITY_BIT)


def set_export_target(export_watts: int) -> dict:
    """
    Force export to grid. 43110=Feed-in Priority, 43483 allow_export=ON,
    43074=EPM cap, 43132=2, 43128=positive.
    Dead-man ~4 min — must refresh. Uses Modbus lock.
    """
    with _modbus_lock:
        return _set_export_target_locked(export_watts)


def _set_export_target_locked(export_watts: int) -> dict:
    client = _get_client()
    writes: list[str] = []
    try:
        if not client.connect():
            return {"ok": False, "message": "Modbus connect failed", "writes": []}
        export_watts = max(0, min(11400, export_watts))
        power_val = (export_watts // 10) & 0xFFFF

        # Use a feed-in-first strategy instead of reintroducing self_use. This more closely
        # matches the cloud-side behavior the operator sees when export holds successfully.
        h = _read_holding(client, _REG_STORAGE_CONTROL, 1)
        if h:
            mode = h[0]
            new_mode = _build_exact_storage_control_value(
                mode,
                primary_bits=(_STORAGE_FEED_IN_PRIORITY_BIT,),
            )
            if new_mode != mode:
                logger.info(
                    "set_export_target: 43110 before=%s target=%s",
                    _format_storage_bits(mode),
                    _format_storage_bits(new_mode),
                )
            if new_mode != mode and _write_holding(client, _REG_STORAGE_CONTROL, new_mode):
                writes.append(f"43110=0x{new_mode:04X} (feed-in priority)")
        time.sleep(0.1)

        # 43483: explicitly enable allow_export so remote export is not immediately blocked.
        h2 = _read_holding(client, _REG_HYBRID_CONTROL, 1)
        if h2:
            hybrid_mode = h2[0]
            new_hybrid_mode = hybrid_mode | (1 << _HYBRID_ALLOW_EXPORT_BIT)
            if new_hybrid_mode != hybrid_mode:
                logger.info(
                    "set_export_target: 43483 before=%s target=%s (allow_export on)",
                    hybrid_mode,
                    new_hybrid_mode,
                )
            if new_hybrid_mode != hybrid_mode and _write_holding(client, _REG_HYBRID_CONTROL, new_hybrid_mode):
                writes.append(f"43483=0x{new_hybrid_mode:04X} (allow export)")
        time.sleep(0.1)

        # 43074: EPM cap (100 W units)
        epm_cap = max(114, (export_watts + 99) // 100)
        if _write_holding(client, _REG_EPM_LIMIT, epm_cap):
            writes.append(f"43074={epm_cap} ({epm_cap * 100} W EPM cap)")
        time.sleep(0.1)

        # 43135=0 (not force charge/discharge)
        if _write_holding(client, 43135, 0):
            writes.append("43135=0")
        time.sleep(0.1)

        # 43132=2, 43128=positive
        if _write_holding(client, _REG_REMOTE_MODE, 2):
            writes.append("43132=2 (remote control)")
        if _write_holding(client, _REG_REMOTE_POWER, power_val):
            writes.append(f"43128={power_val} ({export_watts} W export)")

        client.close()
        logger.info("set_export_target: %s W — %s", export_watts, writes)
        return {"ok": True, "message": f"Export {export_watts} W", "writes": writes}
    except Exception as e:
        logger.exception("set_export_target: %s", e)
        return {"ok": False, "message": str(e), "writes": writes}


def set_power_control_off() -> dict:
    """Stop remote control. 43132=0, 43135=0. Uses Modbus lock."""
    with _modbus_lock:
        return _set_power_control_off_locked()


def _set_power_control_off_locked() -> dict:
    client = _get_client()
    writes: list[str] = []
    try:
        if not client.connect():
            return {"ok": False, "message": "Modbus connect failed", "writes": []}
        if _write_holding(client, _REG_REMOTE_MODE, 0):
            writes.append("43132=0 (remote off)")
        if _write_holding(client, 43135, 0):
            writes.append("43135=0")
        client.close()
        logger.info("set_power_control_off: %s", writes)
        return {"ok": True, "message": "Power control off", "writes": writes}
    except Exception as e:
        logger.exception("set_power_control_off: %s", e)
        return {"ok": False, "message": str(e), "writes": writes}

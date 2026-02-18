"""
Modbus client for Solis S6 hybrid inverter (input registers 33xxx, holding 43xxx).
Connects to INVERTER_HOST:INVERTER_PORT (Solis1). Read-only; writes for storage toggles (43110).

Solis inverter/datalogger may not echo the Modbus TCP transaction ID in responses; pymodbus
then rejects replies with "request ask for transaction_id=X but got id=Y, Skipping." We relax
the framer so any transaction_id is accepted (only one client should talk to the inverter).
"""
from __future__ import annotations

import logging
import os
from typing import Any

from pymodbus.client import ModbusTcpClient
from pymodbus.exceptions import ModbusException

from config import get_inverter_host, get_inverter_port, get_modbus_unit

# Solis inverter/datalogger often does not echo the Modbus TCP transaction_id. pymodbus then
# rejects responses (framer logs "Skipping", transaction layer raises). We relax both so
# responses are accepted and logs stay clean.
def _solis_modbus_patches():
    try:
        from pymodbus.framer import base as _framer_base
        _orig_framer = _framer_base.FramerBase.handleFrame
        def _relaxed_handle(self, data: bytes, exp_devid: int, exp_tid: int):
            return _orig_framer(self, data, exp_devid, 0)  # accept any transaction_id
        _framer_base.FramerBase.handleFrame = _relaxed_handle
    except Exception as e:
        logging.getLogger(__name__).warning("Modbus framer patch failed: %s", e)
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
    # Reduce pymodbus log noise (transaction_id messages are expected with Solis)
    try:
        logging.getLogger("pymodbus").setLevel(logging.WARNING)
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
    (33126, 24),   # meter power, battery V/I/SOC, house load, battery power
    (33161, 20),   # battery charge/discharge energy, grid import/export, load
]
_REG_STORAGE_CONTROL = 43110   # Storage mode bits
_REG_HYBRID_CONTROL = 43483    # Hybrid function: export, peak-shaving, etc.


def _input_addr(solis_reg: int) -> int:
    return (solis_reg - _INPUT_BASE) if _USE_ZERO_BASED else solis_reg


def _holding_addr(solis_reg: int) -> int:
    return (solis_reg - _HOLDING_BASE) if _USE_ZERO_BASED else solis_reg


def _get_client() -> ModbusTcpClient:
    return ModbusTcpClient(host=get_inverter_host(), port=get_inverter_port(), timeout=10)


def _is_tid_mismatch(exc: Exception) -> bool:
    """True if exception is the known Solis transaction_id mismatch (no need to warn)."""
    try:
        msg = str(exc).lower()
    except Exception:
        return False
    return "transaction" in msg and ("but received" in msg or "but got" in msg)


def _read_input_registers(client: ModbusTcpClient, solis_reg: int, count: int) -> list[int] | None:
    try:
        address = _input_addr(solis_reg)
        rr = client.read_input_registers(address=address, count=count, device_id=get_modbus_unit())
        if rr.isError():
            return None
        return list(rr.registers)
    except (ModbusException, Exception) as e:
        if _is_tid_mismatch(e):
            logger.debug("read_input_registers %s %s: %s", solis_reg, count, e)
        else:
            logger.warning("read_input_registers %s %s: %s", solis_reg, count, e)
        return None


def _read_holding(client: ModbusTcpClient, solis_reg: int, count: int = 1) -> list[int] | None:
    try:
        address = _holding_addr(solis_reg)
        rr = client.read_holding_registers(address=address, count=count, device_id=get_modbus_unit())
        if rr.isError():
            return None
        out = list(rr.registers)
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


def _decode_storage_bits(val: int) -> dict[str, bool]:
    return {_STORAGE_BIT_NAMES[i]: bool((val >> i) & 1) for i in range(min(12, len(_STORAGE_BIT_NAMES)))}


def _decode_hybrid_bits(val: int) -> dict[str, bool]:
    return {_HYBRID_BIT_NAMES[i]: bool((val >> i) & 1) for i in range(min(8, len(_HYBRID_BIT_NAMES)))}


def poll_solis() -> dict[str, Any]:
    """Read all Solis input (and holding 43110) and return a flat dict for UI."""
    logger.debug("poll_solis start")
    client = _get_client()
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
        "energy_today_load_kWh": 0,
        "raw_blocks": {},
    }
    try:
        if not client.connect():
            logger.warning("poll_solis: connect failed to %s:%s", get_inverter_host(), get_inverter_port())
            return out
        try:
            all_regs: dict[int, list[int]] = {}
            for addr, count in _BLOCKS:
                r = _read_input_registers(client, addr, count)
                if r is not None:
                    all_regs[addr] = r
            # Holding 43110 (storage control) and 43483 (hybrid)
            h = _read_holding(client, _REG_STORAGE_CONTROL, 1)
            if h is not None:
                all_regs[_REG_STORAGE_CONTROL] = h
            h2 = _read_holding(client, _REG_HYBRID_CONTROL, 1)
            if h2 is not None:
                all_regs[_REG_HYBRID_CONTROL] = h2

            out["raw_blocks"] = {str(k): v for k, v in all_regs.items()}
            out["ok"] = len(all_regs) > 0  # only true if we got at least one block

            # Block 33000-33040
            b0 = all_regs.get(33000)
            if b0 and len(b0) >= 37:
                out["energy_today_pv_kWh"] = (_reg(b0, 35, 0) / 10.0)  # 33035 = today 0.1 kWh
                out["product_model"] = _reg(b0, 0)

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

            # Block 33126-33149
            b3 = all_regs.get(33126)
            if b3 and len(b3) >= 24:
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

            # Alias for dashboard (grid = meter; PV use inverter active power if DC not set)
            out["grid_power_W"] = out.get("meter_power_W", 0)  # signed: - = export
            out["load_power_W"] = out.get("house_load_W", 0)
            # PV: prefer total DC power; if missing/zero use inverter active power (signed)
            ap = out.get("active_power_W", 0)
            if out.get("pv_power_W", 0) == 0 and ap != 0:
                out["pv_power_W"] = max(0, ap)  # show PV as non-negative for display

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


def set_storage_control_bit(bit_index: int, on: bool) -> bool:
    """Read 43110, set or clear one bit, write back. bit_index 0..11."""
    client = _get_client()
    try:
        if not client.connect():
            return False
        h = _read_holding(client, _REG_STORAGE_CONTROL, 1)
        if not h:
            client.close()
            return False
        val = h[0]
        if on:
            val |= 1 << bit_index
        else:
            val &= ~(1 << bit_index)
        ok = _write_holding(client, _REG_STORAGE_CONTROL, val & 0xFFFF)
        client.close()
        return ok
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
    """Read 43483, set or clear one bit, write back. bit_index 0..7."""
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
        ok = _write_holding(client, _REG_HYBRID_CONTROL, val & 0xFFFF)
        client.close()
        return ok
    except Exception as e:
        logger.exception("set_hybrid_control_bit: %s", e)
        return False


def apply_use_all_solar_preset() -> bool:
    """
    Preset: Self-use, no export — use all solar for load and battery (load-based; no export to grid).
    Sets: Self-Use ON (43110.0), Feed-in OFF (43110.6), Allow export OFF (43483.3).
    """
    client = _get_client()
    try:
        if not client.connect():
            return False
        ok = True
        # 43110: set self_use (0), clear feed_in_priority (6)
        h = _read_holding(client, _REG_STORAGE_CONTROL, 1)
        if h:
            val = h[0]
            val |= 1 << 0   # self_use on
            val &= ~(1 << 6)  # feed_in off
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

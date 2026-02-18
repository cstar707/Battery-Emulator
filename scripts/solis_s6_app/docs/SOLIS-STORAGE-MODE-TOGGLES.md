# Solis Storage Mode Toggles (Modbus)

Reference for what we can control on the Solis S6 hybrid inverter over Modbus.  
Source: [Solis Modbus HA integration](https://solis-modbus.readthedocs.io/en/latest/sensors.html), Solis exporter, and Solis service docs.

---

## 1. Main mode register: **43110** (holding, read + write)

**43110** is a **single 16‑bit holding register** used as a **bitmask**. Each bit is a separate toggle.  
Read with **FC 0x03** (Read Holding Registers). Write with **FC 0x06** (Write Single Register).  
Current state can also be read back from **input** register **33132** (“Storage Control Switching Value”) so you can show the same value in the UI.

| Bit | Name | Description |
|-----|------|-------------|
| 0 | **Self-Use Mode** | Store excess PV in battery for later use; grid as backup. |
| 1 | **Time Of Use (TOU) Mode** | Time charging: use charge/discharge time slots (43143, 43153, 43163, etc.). |
| 2 | **OFF-Grid Mode** | No grid; supply backup loads from PV + battery. |
| 3 | **Battery Wakeup Switch** | Wakeup / recovery for battery. |
| 4 | **Reserve Battery Mode** | “Backup” mode: reserve a portion of SOC for outages (Backup SOC in 43024). |
| 5 | **Allow Grid To Charge The Battery** | Permit grid charging when enabled. |
| 6 | **Feed In Priority Mode** | Prioritize exporting solar to grid; battery only used if TOU charging is set. |
| 7 | **Batt OVC** | Battery overvoltage-related control. |
| 8 | **Battery Forcecharge Peakshaving** | Force charge for peak shaving. |
| 9 | **Battery current correction** | Current correction. |
| 10 | **Battery healing mode** | Battery conditioning/healing. |
| 11 | **Peak-shaving mode** | Discharge during peak demand to reduce grid draw. |

To **toggle one mode** without changing others:

1. Read register **43110** (or 33132 for current state).
2. Set or clear the bit for that mode (e.g. bit 1 for TOU).
3. Write the new 16‑bit value back to **43110** with Write Single Register.

Example (conceptual):

- Enable TOU: `new_value = (current_43110 | (1 << 1))`
- Disable TOU: `new_value = (current_43110 & ~(1 << 1))`

The often-cited values **35** and **33** (solis-exporter) are example **whole-register values** when only certain bits are set (e.g. 35 = run TOU, 33 = stop TOU); for a proper UI you should work with the full bitmask so other modes are not overwritten.

---

## 2. Time-of-use (TOU) run/stop (same register)

- **Time Of Use “run”**: ensure **bit 1** of 43110 is **set** (and write 43110).
- **Time Of Use “stop”**: ensure **bit 1** of 43110 is **clear** (and write 43110).

So the “timed charge/discharge” on/off is just **bit 1** of **43110**, not a separate register.

---

## 3. Time slots (charge/discharge windows)

Each slot is **8 registers**: charge start (HH, MM), charge end (HH, MM), discharge start (HH, MM), discharge end (HH, MM), plus 2 reserved (often 0).

| Slot | Start register | Registers | Content |
|------|----------------|-----------|--------|
| 1 | **43143** | 8 | Ch start H/M, Ch end H/M, Disch start H/M, Disch end H/M, 0, 0 |
| 2 | **43153** | 8 | Same layout |
| 3 | **43163** | 8 | Same layout |
| 4 | **43173** | 8 | Same layout |
| 5 | **43183** | 8 | Same layout |

Write with **FC 0x10** (Write Multiple Registers). Example for slot 1: charge 03:00–05:30, no discharge:

`write_registers(43143, [3, 0, 5, 30, 0, 0, 0, 0])`

Read with **FC 0x03** (Read Holding Registers). Do **not** use FC 0x04 (input registers) for these; use holding.

---

## 4. Other useful control registers

| Register | R/W | Description |
|----------|-----|-------------|
| **43010** | R/W | Overcharge SOC (%) |
| **43011** | R/W | Overdischarge SOC (%) |
| **43018** | R/W | Force Charge SOC (%) |
| **43024** | R/W | Backup SOC (%) – used in Reserve Battery mode |
| **43027** | R/W | Battery Force-charge Power Limitation (W) |
| **43135** | R/W | RC Force Battery Charge/discharge |
| **43137** | R/W | Off-Grid Overdischarge SOC (%) |
| **43141** | R/W | Time-Charging Charge Current (A) |
| **43142** | R/W | Time-Charging Discharge Current (A) |
| **43365** | R/W | Generator setting switch (bitmask: generator position, with generator, enable, AC coupling, etc.) |
| **43340** | R/W | Generator input mode (Manual/Auto), Generator charge enable |
| **43483** | R/W | Hybrid function control (bitmask: dual backup, AC coupling, export, peak shaving, etc.) |
| **43249** | R/W | Protection/function bits (MPPT parallel, relay, ISO, etc.) |
| **43815** | R/W | Generator charging period 1–6 switches (bits 0–5) |

---

## 5. Read-back (status)

- **33132** (input): “Storage Control Switching Value” – mirrors the 43110 bitmask so you can show current mode state without writing.
- **33091** (input): Standard working mode (enumerated).
- **33095** (input): Current inverter state (e.g. generating, standby).

---

## 6. Summary: what we can do

- **Toggle storage modes** via **43110** bits: Self-Use, TOU, OFF-Grid, Reserve Battery, Allow Grid Charge, Feed-in Priority, Peak-shaving, etc.
- **Turn TOU run/stop** by setting/clearing **bit 1** of **43110**.
- **Set charge/discharge windows** by writing the 8-register blocks at **43143**, **43153**, **43163**, **43173**, **43183**.
- **Set SOC and power limits** (backup SOC, overcharge/overdischarge, force charge, etc.) via 43xxx registers above.
- **Generator and hybrid options** via **43365**, **43340**, **43483**, **43815** as needed.

For the UI: implement **43110** as a set of checkboxes (one per bit), read 43110 or 33132 on load, and on change read–modify–write 43110 so only the clicked bit is toggled and the rest are preserved.

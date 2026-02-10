# Velar: Contactor not closing – HVBattContactorDemandT

The BMS will not close contactors until it sees **HVBattContactorRequest** / **HVBattContactorDemandT** from the vehicle. Your diagnostic screen showed this signal as "-" (not present), so the emulator was not sending the demand.

## What we changed

- **0x18B (BCCM_PMZ_A)** payload: **`03 01 00 00 00 00 00 00`**.
  - Byte 0 = 0x03: bit 0 = alive, bit 1 = contactor demand.
  - Byte 1 = 0x01: precharge request (BMS may need this before PrechargeAllowed / contactor close).
- If your DBC uses a different **byte or bit** for the contactor demand, this guess may be wrong and you'll need to set the correct one.

## If contactors still don't close

1. **Find the signal in your DBC/diagnostic DB**
   - Search for **HVBattContactorDemandT** or **HVBattContactorRequest**.
   - Note: **CAN ID**, **byte index** (0–7), **start bit**, **length** (e.g. 1 bit), **scale/offset** if any.

2. **Set that value in the emulator**
   - In `LAND-ROVER-VELAR-PHEV-BATTERY.h`, the frame that matches that CAN ID is the one to edit (likely `VELAR_18B` for 0x18B, or `VELAR_0x224_BCCMB` for 0x224, etc.).
   - In the `.data` array, set the correct byte so the contactor-demand bit(s) are 1.  
     Example: if the signal is **0x18B, byte 2, bit 3**, then set  
     `data[2] |= (1 << 3);` in code, or in the initializer put the right hex in `data[2]`.

3. **Optional: capture from a real vehicle**
   - With the key in Run/Ready (contactors closed), log CAN and find which ID and payload show **HVBattContactorDemandT** = 1 (or Request = 1).
   - Replicate that exact payload in the emulator.

## Frames we currently send (for reference)

| ID    | Name           | Interval | Notes                          |
|-------|----------------|----------|---------------------------------|
| 0x008 | GWM_PMZ_A      | 10 ms    | Gateway                         |
| 0x18B | BCCM_PMZ_A     | 50 ms    | **Contactor demand (byte 0=0x03)** |
| 0x18d | GWM_PMZ_V_HYBRID | 60 ms  | Hybrid state                    |
| 0x224 | BCCMB_PMZ_A    | 90 ms    | Second BCCM                     |
| 0xA4  | Inverter HVIL  | 20 ms    | HVIL status                     |

Once the BMS sees the correct contactor demand, **HVBattContactorDemandT** should show a value in your tool and contactors may close (if all other conditions are met).

---

## Tool shows "Contactor Closed" but contactors did not close physically

If the diagnostic tool shows **HVBattContactorStatus: Closed** and PrechargeAllowed: Yes, but the **physical contactors never close**, possible causes:

1. **Signal is "commanded" not "actual"**  
   The BMS may report "contactors closed" on CAN when it has *commanded* close, while the real state is from contactor feedback (e.g. auxiliary contacts). In the DBC, check whether the signal we decode (0x98 byte 0 bit 7) is "contactor commanded" vs "contactor feedback / actual state". If there is a separate "contactor feedback" or "auxiliary contact" signal, see whether that stays "open".

2. **Contactor enable / HV request hardwire missing**  
   Many OEM packs need both CAN and a **hardwired** signal from the vehicle to actually drive the contactor coils, e.g.:
   - A "contactor enable" or "HV request" line from the vehicle to the pack that enables the BMS contactor driver outputs.
   - 12V or a dedicated line that the vehicle only asserts when it wants HV.  
   We only emulate CAN; we do not drive any contactor-enable pin on the pack. If the real vehicle drives such a line (e.g. from BCCM or GWM), that may need to be simulated (e.g. 12V or a relay) for the contactors to close.

3. **Wiring / service manual**  
   Check the Velar/PHEV wiring diagram for:
   - Any "contactor enable", "HV request", "BCCM to battery" or "GWM to BECM" contactor-related pins.
   - Whether the pack's contactor driver gets an enable from the vehicle side; if yes, that may need to be wired or simulated in a stationary setup.

---

## Log summary: still no contactor close

With **0x18B = 03 01** (contactor demand + precharge) and vehicle frames 0x008, 0x18d, 0x224, 0xA4 being sent:

- **0x8A** (BMS): byte 6 = `00` in the capture → **PrechargeAllowed = 0** (bit 4 of byte 6). So the BMS is not granting precharge on CAN.
- **0x98** (BMS): byte 0 often has bit 7 set (e.g. `CF`, `82`) → **ContactorStatus** decoded as closed/commanded. So the BMS may be reporting "contactor commanded" or status, but physical contactors still do not close.

**Firmware changes added to try:**

1. **Rolling counters** – 0x008, 0x18d, and 0x224 now send a rolling counter in **byte 7** (0–15, then wrap). Some BMS/gateway logic expects a changing counter; static 0x00 can be rejected.
2. **0x18B byte 0 = 0x07** – We now send **0x07** instead of 0x03 (bit 2 set in addition to alive + contactor demand). If the DBC has a third bit (e.g. "HV enable" or "drive ready") in that byte, the BMS might need it before setting PrechargeAllowed. If contactors still don't close or you see odd behaviour, revert byte 0 to **0x03** in `LAND-ROVER-VELAR-PHEV-BATTERY.h`.

**If still no contactor close:**

1. **PrechargeAllowed stays 0** – The BMS may require something else (another CAN ID, correct payload from real-vehicle capture, or a **contactor-enable / HV-request hardwire**). Check DBC for what sets PrechargeAllowed; consider a CAN capture from the real vehicle with key in Run/Ready.
2. **Contactor-enable hardwire** – Many packs need a physical line from the vehicle to enable the contactor drivers. We only emulate CAN; that line may need to be pulled to 12 V (or simulated) for the contactors to actually close.
3. **Refine vehicle frames** – If you have a capture from the real car with contactors closed, compare 0x008, 0x18d, 0x224 (and any other GWM/BCCM IDs) and replicate those payloads in the emulator.

**Second test (rolling counters + 0x18B = 0x07):** With rolling counters in byte 7 of 0x008, 0x18d, 0x224 and 0x18B byte 0 = 0x07, result unchanged: 0x8A byte 6 still 0 (PrechargeAllowed false), 0x98 byte 0 bit 7 set (status reported closed), physical contactors still do not close. So the blocker is likely either a **contactor-enable hardwire** from the vehicle or one or more vehicle frames we are not replicating correctly; next step is wiring diagram / hardwire check or a full vehicle CAN capture with key in Run/Ready.

**Logger / DBC notes:**  
- **ID 0x8** can appear in the DBC as **TCM_SP_PMZ_ParkLockDataTarget** (TCM/EDPM → BCM, “Idle, challenge query” & Enable status). If the logger shows a **CAN Error (Stuff Error)** on ID 0x8, possible causes: (1) we and another node (e.g. vehicle) both send 0x8 and conflict; (2) payload or timing causes a bit-stuff violation.  
- **0x008 disabled by default:** We **no longer send ID 0x008** by default (`VELAR_SEND_FRAME_0x008` = 0 in `LAND-ROVER-VELAR-PHEV-BATTERY.h`) to avoid that conflict; the vehicle can send 0x8 alone. Re-enable by setting `VELAR_SEND_FRAME_0x008` to 1 if you need to try gateway presence again. **Test with 0x008 disabled:** No TX1 0x8 in log; 0x8A byte 6 still 0 (PrechargeAllowed false), 0x98 status unchanged—contactors still did not close. So disabling 0x8 did not resolve it; next step remains contactor-enable hardwire or full vehicle capture.  
- **ID 0x180 vs 0x18D:** We send **0x18D** (GWM_PMZ_V_HYBRID). The logger may show **0x180** as a separate ID; 0x180 (384) ≠ 0x18D (397). If the vehicle sends 0x180 with specific content for contactor logic, we are not currently emulating it. Check a full-vehicle capture for 0x180 when contactors are closed.

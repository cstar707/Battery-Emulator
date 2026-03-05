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

| ID    | Name           | Interval | Notes                                          |
|-------|----------------|----------|------------------------------------------------|
| 0x008 | GWM_PMZ_A      | 10 ms    | Gateway (disabled by default)                  |
| 0x18B | BCCM_PMZ_A     | 50 ms    | Contactor demand (byte 0), precharge (byte 1). Vehicle timing. |
| 0xA2  | PCM_PMZ_HVBatt | 50 ms    | DBC payload: PNChargingFunctionReq=0, PwrSupWakeUpAllowed=1 (HV Battery), PNFuelRefill=0. |
| 0x18d | GWM_PMZ_V_HYBRID | 60 ms  | Hybrid state. Vehicle timing.                  |
| 0x224 | BCCMB_PMZ_A    | 90 ms    | Second BCCM. Vehicle timing.                   |
| 0xA4  | Inverter HVIL  | 20 ms    | InverterHVILStatus at bit 36 (byte 4), 2-bit: 2=OK. Per DBC. |
| 0x96  | BCCM_PMZ_DCDCOperatingMode | 10 ms | ChargerHVILStatus=0 (OK) at bit 33. DCDCOpModeGpCounter. |

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

**Digital HVIL (0xA4 InverterHVILStatus):** DBC defines InverterHVILStatus at start bit 36 (byte 4, 2 bits), values 0–3. Value 2 = OK. Emulator sends byte 4 = 0x20 (value 2) at the correct bit position so the BMS sees inverter HVIL OK.

**Additional changes (battery "doing something" but contactors not closing):**
- **0x008 enabled** – GWM gateway frame re-enabled; BMS may need it for full contactor sequence.
- **0xA2 rolling counter** – Byte 7 now increments (HVBattContDemandTCount); BMS may reject static values.
- **If still no contactors:** Capture 0x180 from real vehicle when contactors closed; consider contactor-enable hardwire.

**Phased approach (current):**
1. **0–5 s after boot:** No contactor demand (0x18B/0xA2 stay open) – BMS init
2. **5–17 s after user requests close:** 0xA2 bytes 6–7 = 24 09 (wake-up pattern from Key Cycles)
3. **After 17 s:** 0xA2 bytes 6–7 = 20 01 (drive pattern from ShortDrive)
4. **When open:** 0xA2 bytes 6–7 = 04 06 (Key Cycles open)

`VELAR_CONTACTOR_DELAY_MS` (5000), `VELAR_WAKEUP_PHASE_MS` (12000) – adjust in .h if needed.

**Tuning options (LAND-ROVER-VELAR-PHEV-BATTERY.h):**
- `VELAR_18B_BYTE0_CLOSED` = 0x03 or 0x07 (0x07 = alive+contactor+bit2; try 0x03 if contactors behave oddly)
- `VELAR_A2_BYTE7_FIXED_09` = 1 to use fixed 0x09/0x06 (vehicle BLF) instead of rolling counter
- `VELAR_96_SEND_CHECKSUM` = 1 to compute CRC8 in byte 0 over bytes 1-7 (0x96). Set to 0 if BMS rejects.

**Logger / DBC notes:**  
- **ID 0x8** can appear in the DBC as **TCM_SP_PMZ_ParkLockDataTarget** (TCM/EDPM → BCM, “Idle, challenge query” & Enable status). If the logger shows a **CAN Error (Stuff Error)** on ID 0x8, possible causes: (1) we and another node (e.g. vehicle) both send 0x8 and conflict; (2) payload or timing causes a bit-stuff violation.  
- **0x008 disabled by default:** We **no longer send ID 0x008** by default (`VELAR_SEND_FRAME_0x008` = 0 in `LAND-ROVER-VELAR-PHEV-BATTERY.h`) to avoid that conflict; the vehicle can send 0x8 alone. Re-enable by setting `VELAR_SEND_FRAME_0x008` to 1 if you need to try gateway presence again. **Test with 0x008 disabled:** No TX1 0x8 in log; 0x8A byte 6 still 0 (PrechargeAllowed false), 0x98 status unchanged—contactors still did not close. So disabling 0x8 did not resolve it; next step remains contactor-enable hardwire or full vehicle capture.  
- **ID 0x180 vs 0x18D:** We send **0x18D** (GWM_PMZ_V_HYBRID). The logger may show **0x180** as a separate ID; 0x180 (384) ≠ 0x18D (397). If the vehicle sends 0x180 with specific content for contactor logic, we are not currently emulating it. Check a full-vehicle capture for 0x180 when contactors are closed.

---

## Vehicle vs emulator comparison (CANalyzer)

**Layout:** Left plot = real vehicle (key in Run, contactors closed). Right plot = battery emulator.

### Contactor behavior

| Scenario | Contactor behavior |
|----------|--------------------|
| **Vehicle (left)** | Contactors close and **stay closed** for several seconds (e.g. 43.5–44.5 s, 45.5–46.5 s). |
| **Emulator (right)** | Contactors close for ~1 s, then open and remain open. HV appears briefly when closed. |

**Goal:** Make the emulator match the vehicle so contactors stay closed.

---

### Full signal comparison table

Each row is a signal from the CANalyzer trace.

| Signal | Vehicle (left) | Emulator (right) | Match? | Action |
|--------|----------------|------------------|--------|--------|
| **HvBattContDemand_CS [Chksum]** | 100 (drops to 0, steps back to 100) | 200 (drops to 0, gradual rise to 200) | **NO** | Emulator sends wrong checksum. Target 100 or vehicle algorithm. |
| **HvBattContactorStatus** | Sustained Closed | Brief Closed, then Open | **NO** | Result of other mismatches. |
| **HvBattContactorRequest** | (assumed Closed) | (we send Closed) | ? | Confirm vehicle value. |
| **HVL not checked** | OFF after ~43.5 s | OFF after ~35.5 s | OK | Both reach OFF. |
| **HVL not OK** | OFF | OFF | OK | Same. |
| **InverterHVLStatus** | (OK) | We send 0xA4 = 2 (OK) | ? | Likely OK. |
| **ChargerHVLStatus** | (vehicle sends) | Not emulated | **NO** | Add Charger HVIL if DBC/capture available. |
| **HvBattPrechargeAllowed** | (likely 1 when closed) | 0 in our logs | **NO** | BMS may not grant precharge. |
| **HybridMode** | (vehicle value) | 0x18D minimal payload | ? | Check vehicle 0x18D content. |
| **PMChargingFunctionReq** | Steady, then pulsed | Pulsed from start, longer | **NO** | Different pattern; may need charging-function frame. |
| **PNChargingReq** | (vehicle pattern) | (emulator pattern) | **NO** | Different – needs vehicle capture. |
| **PNFuelRefil** | (vehicle pattern) | (emulator pattern) | **NO** | Different – needs vehicle capture. |
| **Plugged in, not charging** | Steady, then pulsed | Pulsed earlier, longer | **NO** | Chattering pattern differs. |
| **Require Charging Function** | Steady, then pulsed | Pulsed earlier, longer | **NO** | Same as above. |
| **Partial NOT networking required** | Inactive (low) | Active 37.5–40 s | **NO** | BMS sees missing node with emulator. |
| **PwrSupWakeUpAllowed / PSS wake-up** | 2 pulses | 1 pulse | **NO** | Vehicle sends 2 wake-up pulses; emulator 1. |
| **DMVoltageOCLint [V]** | (analog) | (analog) | ? | Compare if relevant. |
| **Red trace (status -1/2/3)** | Mostly -1, brief 2 | -1, 2, 3, more transitions | **NO** | More state changes with emulator. |

---

### Highest-priority mismatches

1. **PnSupWakeupP5Allowed** – Emulator: "external power supply"; Vehicle: "HV Battery". **Critical** – use HV Battery value.
2. **HVBattContDemandTCS** (checksum) – Vehicle 100, emulator 200. Need correct algorithm.
3. **HVBattContDemandTCount** (counter) – We use rolling 0–15; vehicle may use different pattern.
4. **Partial NOT networking required** – Active with emulator; indicates missing frame or wrong content.
5. **ChargerHVILStatus** – Not emulated. **HVILMSStatus** – Not emulated (3rd HVIL).
6. **PnChargingFunctionReq / Require Charging Function** – We send **too much**; vehicle sends brief pulse then "Do Not Require". Need to reduce or match vehicle pattern.
7. **PNFuelRefill** – Different between vehicle and emulator.
8. **PSS wake-up** – Vehicle: 2 pulses; emulator: 1.

---

### Complete list of different items (vehicle vs emulator)

All signals/frames we’ve identified as different or missing. Needed: CAN ID, byte, start bit, length, algorithm.

| # | Signal / item | Type | Vehicle | Emulator | DBC info needed |
|---|----------------|------|---------|----------|-----------------|
| 1 | **HVBattContDemandTCS** | checksum | 100 (step back) | 200 (gradual rise) | Frame, byte, bits, algorithm |
| 2 | **HVBattContDemandTCount** | counter | (vehicle pattern) | Rolling 0–15 in byte 7 | Frame, byte, bits, increment rule |
| 3 | **HvBattContactorStatus** | status | Sustained Closed | Brief Closed, then Open | (result of others) |
| 4 | **ChargerHVILStatus** | HVIL | Vehicle sends, longer "ok" | Not emulated | CAN ID, payload |
| 5 | **InverterHVILStatus** | HVIL | Longer "ok" duration | 0xA4; may toggle too fast | Timing, payload |
| 6 | **HVILMSStatus** | HVIL | Vehicle sends | Not emulated (3rd HVIL) | CAN ID, payload |
| 7 | **HvBattPrechargeAllowed** | permission | Allowed when closed | Not allowed / brief | (BMS output) |
| 8 | **PnChargingFunctionReq / Require Charging Function** | request | Brief pulse, then "Do Not Require" | **We send too much** – stays active longer | CAN ID, payload; send less / match vehicle pattern |
| 9 | **PNFuelRefill** | request | Vehicle pattern | 0/Off (may differ) | CAN ID, payload |
| 10 | **PnSupWakeupP5Allowed** | wake-up | "energy by **HV Battery**" | "energy by **external supply**" | **CRITICAL:** Use HV Battery value |
| 11 | **Partial NOT networking required** | fault | Inactive | Active | (missing frame / wrong content) |
| 12 | **PwrSupWakeUpAllowed / PSS wake-up** | wake-up | 2 pulses | 1 pulse | CAN ID, payload, timing |
| 13 | **HybridMode** (0x18D) | mode | Vehicle payload | Minimal `01 00...` | Byte layout, valid values |
| 14 | **HVBattContactorServReq** | service | 0/1 transitions | (check if we send) | CAN ID, payload |
| 15 | **HVBattContactorServReq2** | service | 0/1 transitions | (check if we send) | CAN ID, payload |
| 16 | **VSCVBattInletRegDat** | analog | Osc 0–200, vehicle pattern | Different pattern | CAN ID if needed |
| 17 | **Red trace (status -1/2/3)** | status | Mostly -1, brief 2 | -1, 2, 3, more | (derived) |

**Contactor-demand frame (likely 0xA2 or 0x18B):**  
Need DBC for **HVBattContDemandTCount** and **HVBattContDemandTCS** – which frame, which byte(s), which nibbles, and the checksum formula (e.g. CRC8, XOR, lookup table).

---

### Require Charging Function – we send too much

Vehicle: brief pulse of "Require Charging Function", then "Do Not Require".  
Emulator: "Require Charging Function" stays active **longer** or pulses **more often**.

**Action:** Find which CAN frame carries PnChargingFunctionReq / PMChargingFunctionReq. Either (a) don’t send it when contactors are closed, or (b) send it with a brief pulse then 0/off to match the vehicle. Range Rover list mentions EPIC_PMZ_B (0x009), PCM_PMZ_C (0x030, 0x304, etc.) – may be in one of those.

---

### PnSupWakeupP5Allowed – critical difference (vehicle vs emulator log)

| Source | Value | Meaning |
|--------|-------|---------|
| **Vehicle** (contactors closed) | "energy supplied by **HV Battery**" | Correct – BMS accepts |
| **Emulator** | "energy supplied by **external power supply**" | **Wrong** – BMS may reject / open contactors |

We must send the value for "HV Battery" instead of "external power supply". DBC/capture required for CAN ID and payload.

---

### Next steps

- **Vehicle CAN capture:** Log 0x18B, 0xA2, 0xA4, 0x008, 0x18D, 0x224, Charger HVIL, **HVILMSStatus**, **PnSupWakeupP5Allowed**, PNChargingReq, PNFuelRefill.
- **DBC:** For contactor demand: HVBattContDemandTCount, HVBattContDemandTCS. Also **PnSupWakeupP5Allowed** (HV Battery vs external supply value), ChargerHVILStatus, HVILMSStatus, PNChargingReq, PNFuelRefill.

---

### Things we send that the vehicle may NOT do

We may be sending extra or different traffic that triggers the BMS to reject or chatter.

| What we do | Vehicle likely does | Risk |
|------------|---------------------|------|
| **0xA4 at 10 ms** | Inverter sends 0xA4 at **20 ms** (per DBC) | Sending too fast; BMS may reject |
| **0x18B / 0xA2 at 30 ms** | Likely **50 ms** (from original capture) | Too fast; may confuse BMS |
| **0x18D at 40 ms** | Range Rover doc: **60 ms** | Too fast |
| **0x224 at 60 ms** | Doc: **90 ms** | Too fast |
| **0x18B byte 0 = 0x07** | Capture may have used **0x03** (no bit 2) | Extra bit could be wrong state |
| **HVBattContDemandTCS** = 200 | Vehicle = **100** | Wrong checksum algorithm; BMS rejects |
| **HVBattContDemandTCount** | We use rolling 0–15 in byte 7 | Vehicle may use different counter rule or byte |
| **0x18B byte 7 = 0** | Vehicle may put checksum here | Missing or wrong checksum |
| **0x008** | GWM sends it; on bench we emulate | Payload may differ from vehicle |
| **PSS wake-up: 1 pulse** | Vehicle: **2 pulses** | Wrong sequence |
| **PnSupWakeupP5Allowed** | "HV Battery" | We send "external power supply" | **Wrong value – use HV Battery** |
| **PnChargingFunctionReq** | Brief pulse, then off | We send **too much** / stay active longer | Reduce; match vehicle timing |
| **PNFuelRefill** | Vehicle pattern | We send different / don't emulate | May need correct frame and value |
| **HVILMSStatus** | Vehicle sends | Not emulated | 3rd HVIL – may need to add |

**Quick experiments (no capture):**

1. ~~Revert timing: 0xA4 → 20 ms, 0x18B/0xA2 → 50 ms, 0x18D → 60 ms, 0x224 → 90 ms.~~ **Applied.**
2. ~~Try 0x18B byte 0 = **0x07**.~~ **Applied** (was 0x03). Revert to 0x03 if contactors behave oddly.
3. Try fixed byte 7 values (e.g. 0x09 for 0xA2 when closed) instead of rolling counter.

---

## Contactor still chattering – what to try next

After 0xA4 fix, tighter timing, rolling counters: contactors still chatter. Options:

| # | Action | Notes |
|---|--------|-------|
| 1 | **Vehicle CAN capture** | Highest priority. Log from real Velar (key Run, contactors closed). Compare 0x18B, 0xA2, 0xA4, 0x008, 0x18D, 0x224 payloads and timing. Look for HvBattContDemand_CS algorithm. |
| 2 | **Revert to 50 ms timing** | Tighter (30 ms) might be wrong. Some BMS reject or misinterpret if messages come too fast. |
| 3 | **Connect a load** | With inverter or resistive load, precharge/voltage behavior may improve and reduce chattering. |
| 4 | **DBC: HvBattContDemand_CS** | If you have DBC, find which frame/byte carries the checksum and how it’s calculated. Implement in emulator. |
| 5 | **Charger HVIL** | Vehicle may send Charger HVIL (separate from Inverter 0xA4). Find ID and payload from capture or DBC. |
| 6 | **Add 0x180** | Doc mentions 0x180; vehicle may send it. Needs capture to get payload. |

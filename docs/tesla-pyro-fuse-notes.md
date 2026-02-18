# Tesla Pyro Fuse – Why It Can Blow (T2CAN / Stationary)

The **pyro fuse** (pyrotechnic disconnect) is inside the Tesla pack. The **BMS/HVP fires it**; the Battery-Emulator does not. Our firmware only *reports* what the BMS says (e.g. `pyroFuseBlown`, `passivePyroDeploy`).

## How the code sees it

- **Reported by BMS** in HVP alert frame **0x3AA** (on some firmware; on newer packs this frame is muxed and the parser is commented out in `TESLA-BATTERY.cpp`).
- **Events we derive:**
  - **INTERNAL_OPEN_FAULT** when `battery_hvil_status == 3`: *"Someone disconnected a high voltage cable while battery was in use"* – HVIL opened while pack was live.
  - **EVENT_BATTERY_FUSE** when pack voltage is between **0.5 V and 5.0 V**: *"pyrofuse most likely blown"* (voltage collapse after pyro fires).

So when the pyro blows, we only see the *result* (e.g. voltage collapse, or BMS reporting pyro blown). The *decision* to fire is inside the pack.

## Likely causes (what can make the BMS fire the pyro)

1. **HVIL opened while under load / contactors closed**  
   - Unplugging a high-voltage connector, opening the service disconnect, or breaking the HVIL loop while the pack is live.  
   - BMS can treat this as **INTERNAL_OPEN_FAULT** and may fire the pyro for safety.

2. **Crash signal**  
   - BMS receives or infers a crash (e.g. from vehicle CAN, crash sensor, or missing “car present” messages).  
   - In stationary use: wrong or missing emulation of gateway/VC front/drive inverter might be interpreted badly; unclear if that alone fires pyro or only blocks contactors.

3. **Overcurrent**  
   - BMS_a022_SW_Over_Current or HVP overcurrent.  
   - Inverter or load pulling more than the BMS allows can trigger faults; in severe cases the BMS can disconnect; pyro is possible for very severe/protection events.

4. **Isolation fault**  
   - BMS_a035_SW_Isolation, BMS_a034_SW_Passive_Isolation.  
   - Leakage to chassis can trigger safety disconnect; in some strategies that can involve pyro.

5. **Arc at contactors**  
   - PackPosCtrArcFault, packNegCtrArcFault.  
   - Arcing when opening/closing under load can be a pyro trigger.

6. **Passive pyro deploy**  
   - `battery_passivePyroDeploy`: e.g. crash sensor or other “passive” safety path telling the BMS to deploy pyro.

## What to check on your T2CAN Tesla setup

- **HVIL**  
  - Never break the HVIL loop or unplug HV connectors while the pack is awake and contactors could be closed.  
  - Ensure HVIL is continuous when you intend the pack to be “on” and that no connector is half-disconnected.

- **Current path**  
  - Ensure the inverter/load doesn’t exceed what the BMS allows (no sustained or huge spikes that could be seen as overcurrent).

- **CAN emulation**  
  - Confirm we’re sending the expected vehicle/gateway/VC messages so the BMS doesn’t set “gateway MIA” or similar; fix any MIA/faults that might lead to a fault path that could escalate (even if pyro is only in extreme cases).

- **Isolation**  
  - No unintended paths from HV+ or HV− to chassis; keep HV wiring and connectors clean and dry.

- **Logs / events**  
  - Before the incident, check serial/web for:  
    `INTERNAL_OPEN_FAULT`, `EVENT_BATTERY_FUSE`, `BMS_a022_SW_Over_Current`, `BMS_a035_SW_Isolation`, `BMS_a036_SW_HvpHvilFault`, and any HVP alert (0x3AA if your pack sends it and we parse it).  
  - That narrows whether it was HVIL, overcurrent, isolation, or something else.

## Summary

The pyro is **fired by the Tesla BMS**, not by the emulator. The most common “we did something wrong” cause in a stationary setup is **breaking HVIL or an HV connection while the pack was live**. Avoid that first; then check overcurrent, isolation, and CAN/emulation so the BMS doesn’t see fault conditions that could lead to a safety disconnect or pyro.

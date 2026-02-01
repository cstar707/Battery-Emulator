# Tesla Model 3/Y — DC Fast Charge Contactor CAN Research

**See also:** [tesla-battery-emulator-can-and-contactors.md](tesla-battery-emulator-can-and-contactors.md) — overview of BE ↔ Tesla CAN, full TX/RX message list, main contactor engagement, and FC contactor summary.

---

## Summary

The **vehicle** tells the pack to open/close the DC fast charge (FC) contactors by sending **0x232 BMS_contactorRequest** on the Vehicle bus. The pack (BMS/HVP) receives this and then drives the FC contactors and reports state on **0x20A HVP_contactorState**.

**Battery Emulator does not currently send 0x232** for Model 3/Y. To support “DC charge path on/off” from BE, we would add TX of 0x232 and set **BMS_fcContactorRequest** (and related signals) accordingly.

---

## CAN frame: 0x232 BMS_contactorRequest

- **Source:** Vehicle (e.g. gateway, VCFRONT, or charge port controller)
- **Receiver:** Pack (BMS/HVP)
- **Bus:** VehicleBus (from [Model3CAN.dbc](https://github.com/joshwardell/model3dbc))
- **DLC:** 8 bytes

### Relevant signals (from Model3CAN.dbc)

| Signal                       | Start bit | Length | Value range | Description (DBC/community) |
|-----------------------------|-----------|--------|-------------|-----------------------------|
| **BMS_fcContactorRequest**  | 0         | 3      | 0–5         | FC (fast charge) contactor request. Pack uses this to close/open FC contactors. |
| **BMS_packContactorRequest**| 3         | 3      | 0–5         | Pack (main) contactor request. |
| **BMS_gpoHasCompleted**     | 6         | 1      | 0–1         | GPO completion. |
| **BMS_ensShouldBeActiveForDrive** | 7 | 1  | 0–1         | ENS active for drive. |
| **BMS_pcsPwmDisable**       | 8         | 1      | 0–1         | PCS PWM disable. |
| **BMS_internalHvilSenseV**  | 16        | 16     | 0–65.534 V  | Internal HVIL sense voltage. |
| **BMS_fcLinkOkToEnergizeRequest** | 32 | 2 | 0–2       | FC link OK to energize request. |

The value encoding for **BMS_fcContactorRequest** (0–5) is not fully documented in the DBC; it likely follows a pattern similar to contactor state enums (e.g. open, closing, closed, opening). Testing or further reverse engineering would be needed to map “request DC charge path on” vs “off” to specific values.

---

## Related frames already in BE

| CAN ID  | Name                 | Direction in BE | Role |
|---------|----------------------|-----------------|------|
| **0x20A** | HVP_contactorState | RX only         | Pack reports contactor state (pack + FC). BE only receives. |
| **0x212** | BMS_status         | RX only         | Pack reports BMS state, including BMS_hvState (e.g. UP_FOR_DC_CHARGE). |
| **0x333** | UI_chargeRequest   | TX              | Vehicle charge UI (e.g. charge enable, termination %). BE sends. |
| **0x334** | UI request         | TX              | Vehicle UI/powertrain control. BE sends. |
| **0x25D** | CP_status          | Defined, not TX | Charge port status (e.g. CCS). Comment in BE: “not necessary for standalone pack operation so not used.” |
| **0x232** | BMS_contactorRequest | **Not in BE** | Vehicle → pack contactor request (pack + **FC**). This is the missing “DC charge path on/off” command path. |

---

## Charge/port context (vehicle → pack)

- **0x25D CP_status** — Charge port status (cable present, door, latch, CP_type, etc.). Vehicle sends; pack may use it for charge/DC context.
- **0x333 UI_chargeRequest** — Includes `UI_chargeEnableRequest`, `UI_chargeTerminationPct`, etc. BE already sends this.
- **0x3A1 VCFRONT_vehicleStatus** — Includes `VCFRONT_bmsHvChargeEnable`, `VCFRONT_preconditionRequest`. BE already sends 0x3A1.

So the **direct** command for FC contactors is **0x232 BMS_fcContactorRequest**. Other frames (0x25D, 0x333, 0x3A1) provide context (charge enabled, port state, HV charge enable) but the explicit contactor request is 0x232.

---

## Implementation outline (for BE)

1. **Add 0x232 TX (Model 3/Y Tesla battery)**  
   - Define a CAN frame for **BMS_contactorRequest** (ID 0x232, DLC 8).
   - Map bytes so that:
     - **BMS_fcContactorRequest** = bits 0–2 (byte 0).
     - **BMS_packContactorRequest** = bits 3–5 (byte 0).
     - **BMS_fcLinkOkToEnergizeRequest** = bits 32–33 (byte 4).
     - Other signals (e.g. BMS_internalHvilSenseV, BMS_pcsPwmDisable) as needed for compatibility.

2. **Drive from a “DC charge contactor” concept**  
   - Add a setting or internal state: “DC charge path requested” (on/off).
   - When “on”: set **BMS_fcContactorRequest** to the value that means “request FC contactors closed” (likely 2–4 depending on enum; needs bench/log verification).
   - When “off”: set **BMS_fcContactorRequest** to “open” (likely 0 or 1).

3. **Optional**  
   - Send **0x25D CP_status** when DC charge is “on” (e.g. cable present, DC capable) so pack has full context.
   - Ensure **0x333** / **0x3A1** are consistent with “charging” when FC is requested (e.g. charge enable, HV charge enable).

4. **Validation**  
   - Log 0x232 and 0x20A on a real pack during DC charge to confirm value meaning and timing.
   - Check for **BMS_a086_SW_FC_Contactor_Mismatch** if FC request and 0x20A state disagree; align BE’s 0x232 with pack expectations to avoid this.

---

## References

- [joshwardell/model3dbc](https://github.com/joshwardell/model3dbc) — Model3CAN.dbc (Model 3/Y CAN IDs and signals).
- BMS_contactorRequest: `BO_ 562 ID232 BMS_contactorRequest: 8 VehicleBus` with `SG_ BMS_fcContactorRequest : 0|3@1+ (1,0) [0|5]`.
- HVP_contactorState (0x20A): already parsed in BE; contains FC state reported by pack.
- Community: BMS_a086_SW_FC_Contactor_Mismatch occurs when commanded FC state (from vehicle) and actual FC state (from pack) disagree.

---

## Sources on the internet

- **Tessie – BMS_a086_SW_FC_Contactor_Mismatch**  
  [stats.tessie.com/alerts/BMS_a086_SW_FC_Contactor_Mismatch](https://stats.tessie.com/alerts/BMS_a086_SW_FC_Contactor_Mismatch)  
  - BMS compares **BMS_fcContactorRequest** (commanded) with **HVP_fcContactorSetState** (actual).  
  - Fault when: “fast charge contactors were closed” then “are now open/opening” while “BMS_fcContactorRequest has remained closed”.  
  - Clear when: “HVP_fcContactorSetState is correct for the current BMS_fcContactorRequest”.  
  - So **BMS_fcContactorRequest** has at least a “closed” semantic; exact numeric value (0–5) not given.

- **Model 3 CAN bus IDs and data (Google Sheets)**  
  [docs.google.com/spreadsheets/d/1ijvNE4lU9Xoruvcg5AhUNLKr7xYyHcxa8YSkTxAERUw](https://docs.google.com/spreadsheets/d/1ijvNE4lU9Xoruvcg5AhUNLKr7xYyHcxa8YSkTxAERUw)  
  - Community CAN ID reference; may list 0x232 and related IDs.

- **Tesla third‑party CAN interface (Model 3/Y)**  
  Service bulletin CD-20-17-001: X181 (J1962) under left instrument panel, 500 kbps, Pin 6/14 CAN H/L.  
  - Describes physical access only; no 0x232 or contactor value encoding.

- **Open Inverter forum – Model 3 High Voltage Controller**  
  [openinverter.org/forum/viewtopic.php?t=1650](https://openinverter.org/forum/viewtopic.php?t=1650)  
  - Discussion of Model 3 HV/BMS; may contain CAN or contactor details (site may use bot protection).

- **DBC files**  
  - [joshwardell/model3dbc](https://github.com/joshwardell/model3dbc) — Model3CAN.dbc (0x232 and signal layout).  
  - [onyx-m2/onyx-m2-dbc](https://github.com/onyx-m2/onyx-m2-dbc) — tesla_model3.dbc variant.  

No public source found on the internet that documents the exact **BMS_fcContactorRequest** value enum (which of 0–5 = open vs closed vs closing etc.); that would need capture from a real car during DC charge or bench testing.

---

## Related docs

- **[tesla-battery-emulator-can-and-contactors.md](tesla-battery-emulator-can-and-contactors.md)** — Master document: BE ↔ Tesla CAN overview, full TX/RX message list, main contactor engagement, and FC contactor summary.

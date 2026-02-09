# Velar: Contactor not closing – HVBattContactorDemandT

The BMS will not close contactors until it sees **HVBattContactorRequest** / **HVBattContactorDemandT** from the vehicle. Your diagnostic screen showed this signal as "-" (not present), so the emulator was not sending the demand.

## What we changed

- **0x18B (BCCM_PMZ_A)** payload: byte 0 set from `0x01` to **`0x03`**.
  - Bit 0 = 1: module alive (unchanged).
  - Bit 1 = 1: contactor request/demand (added).
- If your DBC uses a different **byte or bit** for the contactor demand, this guess may be wrong and you’ll need to set the correct one.

## If contactors still don’t close

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

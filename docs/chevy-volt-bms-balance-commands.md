# Chevy Volt (1st Gen) BMS balance commands

This document summarizes the **BECM–BICM internal CAN** balance/bleed protocol for **1st Gen Chevy Volt** packs (e.g. **2015**; Gen 1 = 2011–2015). It is based on reverse-engineering by the community; the primary source is the DIY Electric Car forum thread below. **Gen 2 Volt (2016–2019) may use different IDs and behaviour**—bench logs show different traffic (e.g. 0x200 00... and 0x2C7 on GMLAN). This doc applies to **Gen 1 (2011–2015)** including **2015**.

**Source:** [1st Gen Chevy Volt BMS Balance Commands Found](https://www.diyelectriccar.com/threads/1st-gen-chevy-volt-bms-balance-commands-found.200023/) (DIY Electric Car Forums).

---

## Overview

- The **BECM** (master) talks to **BICMs** (slaves) on an **internal CAN bus** (not the vehicle HS-CAN/OBD-II). In normal operation every **200 ms** the BECM sends three frames: **0x200**, **0x300**, **0x310**.
- **0x200** with data **0x020000**: prompts all connected BICMs to send back voltage and temperature data for their modules.
- **0x300** (8 bytes) and **0x310** (5 bytes): **queue which cells to bleed**. When a cell position is set in 0x300 or 0x310, the **next** 0x200 with 0x020000 causes the BICM to **bleed that cell for ~130 ms** (~100 mA through internal resistors).
- This is a **manual** method: you decide which cells to bleed and send the commands; the BECM does not initiate balancing on its own from the vehicle side. You can run this with **only the BICMs on the bus** (no BECM), sending 0x200, 0x300, 0x310 from your own controller.

---

## Protocol summary

| Frame | Length | Role |
|-------|--------|------|
| 0x200 | —     | Data **0x02 0x00 0x00** (0x020000): trigger BICMs to report voltages/temps and to perform any queued bleed. |
| 0x300 | 8 bytes | Bit map of cells to **queue for bleeding** (see mapping below). |
| 0x310 | 5 bytes | Additional cell positions for bleed queue. |

- Send **0x300** and **0x310** with the desired cell bits set, then send **0x200#020000** every **200 ms**. Each 0x200 cycle triggers one ~130 ms bleed for the queued cells.
- Queuing rules (from thread): you can queue **every other cell**; **adjacent cells** can be queued to some limit (greater than 2, less than 12 per module). **Queuing all cells at once does not work** (no balancing occurs).

---

## Cell-to-command mapping (1st Gen)

The mapping ties **voltage-reporting CAN IDs** (e.g. 0x460, 0x470, 0x461…) to **bytes/bits in 0x300 and 0x310**. Example from the thread:

- To bleed the cell at the **most positive end** of the pack: send  
  **0x300#01.00.00.00.00.00.00.00** then **0x200#02.00.00** every 200 ms until the cell is at target voltage.
- 0x300 byte 0: bits for first group of cells (e.g. 0x01 = first cell, 0x02 = second, 0x04, 0x08…).
- Full mapping: see the **Balance mapping** attachment (Excel/CSV) in the forum thread, and the **VoltBMSV2** code (below) which derives 0x300/0x310 from the same reporting IDs.

**Note:** In the thread, a typo in the original Excel for frames 0x461/0x471 (reversed byte values) was later corrected; use the corrected mapping or the open-source code.

---

## References

- **Forum thread:** [1st Gen Chevy Volt BMS Balance Commands Found](https://www.diyelectriccar.com/threads/1st-gen-chevy-volt-bms-balance-commands-found.200023/) — balance protocol, scope captures, mapping table, and discussion (Gen 1; Gen 2 may differ).
- **Tom’s VoltBMSV2 (SimpBMS / Ampera):** [AmperaBattery/VoltBMSV2](https://github.com/tomdebree/AmperaBattery/tree/master/VoltBMSV2) — example implementation of 0x300/0x310 construction from cell voltages and reporting IDs.

---

## Gen 2 note

On **Gen 2** packs, bench traffic has been reported as 0x200 with all zeros and 0x2C7 on GMLAN; the balance command set may differ. If you have a Gen 2 BECM–BICM capture or a Gen 2 balance mapping, it would be worth documenting separately.

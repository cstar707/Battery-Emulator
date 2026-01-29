# Battery-Emulator Focus

## Project Link
- https://github.com/dalathegreat/Battery-Emulator

## Summary
Battery-Emulator is a translation layer that enables EV battery packs to work with home solar inverters, providing contactor control and protocol adaptation.

## Key Questions to Answer
- Is Sol-Ark 12k supported or compatible?
- Which Tesla Model Y pack variants are supported?
- Required hardware board and wiring for Tesla pack CAN
- Safety requirements: HV contactors, precharge, and fusing

## Supported Inverters (from project wiki)
Fully supported (examples):
- Fronius Gen24 Plus (Primo/Symo)
- Sungrow SH series (RT and RS families)
- GoodWe ET/BT, EH/BH, EHB
- SolaX X1/X3 Hybrid (may require dual CAN board)
- Deye SUN-(5-20)K-SG01HP3-EU-AM2 and SUN-(29.9-50)K-SG01HP3-EU-BM3
- FoxESS H1/AC1 and H3/AC3 (H3 may need separate CAN channel)
- Sofar 5K...20KTL-3PH
- Solis RHI-3P(5-10)K-HVES-5G

Testing/experimental:
- Growatt
- SMA Sunny Boy Storage 2.5/3.7/5.0/6.0
- Kostal Plenticore

Note: Sol-Ark 12k is not listed on the wiki; treat it as unknown until verified.

## Tesla Model 3/Y Pack Support (high level)
The project documentation and community resources indicate support for Tesla Model 3/Y packs when paired with compatible inverters and proper CAN interface hardware.

## Hardware Interface Options (examples)
- LilyGo T-CAN485 board (cost-effective option)
- Stark CMR Module (professional option)

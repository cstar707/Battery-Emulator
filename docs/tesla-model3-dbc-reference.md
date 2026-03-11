# Tesla Model 3 CAN Database (DBC) Reference

This document provides a reference for the Tesla Model 3/Y CAN database (DBC) used to decode vehicle bus messages. The DBC defines message IDs, signal layouts, scaling factors, and units.

---

## 1. Overview

| Property | Value |
|----------|-------|
| **Format** | DBC (Vector CANdb) |
| **Target** | Tesla Model 3 / Model Y |
| **Bus** | VehicleBus (primary), ChassisBus, PartyBus |
| **Modified** | JW2021Aug29 (per DBC metadata) |

The DBC describes messages transmitted on the vehicle CAN bus. The battery emulator firmware receives these messages from the Tesla pack (BMS, PCS) and decodes them.

---

## 2. DBC Format Quick Reference

| Element | Syntax | Example |
|---------|--------|---------|
| Message | `BO_ <id> <name>: <dlc> <bus>` | `BO_ 692 PCS_dcdcRailStatus: 5 VehicleBus` |
| Signal | `SG_ <name> : <start>\|<length>@<byte_order><signed> (<scale>,<offset>) [<min>\|<max>] "<unit>" <receiver>` | `SG_ PCS_dcdcLvBusVolt : 0\|10@1+ (0.0390625,0) [0\|39.96] "V" Receiver` |
| Byte order | `1` = little-endian, `0` = big-endian | |
| Signed | `+` = unsigned, `-` = signed | |
| Physical value | `raw × scale + offset` | |

---

## 3. Battery & BMS Messages

### 3.1 PCS_dcdcRailStatus (0x2B4 / 692)

| Signal | Bits | Scale | Offset | Unit | Range | Description |
|--------|------|-------|--------|------|-------|-------------|
| PCS_dcdcLvBusVolt | 0 | 10 | 0.0390625 | 0 | V | 0–39.96 | 12V rail voltage |
| PCS_dcdcHvBusVolt | 10 | 12 | 0.146484 | 0 | V | 0–599.85 | HV bus voltage |
| PCS_dcdcLvOutputCurrent | 24 | 12 | 0.1 | 0 | A | 0–400 | 12V output current |

**Used by emulator:** Yes — `TESLA-BATTERY.cpp` → `datalayer_extended.tesla.*` → MQTT / web UI.

---

### 3.2 BMS_energyStatus (0x352 / 850)

| Signal | Bits | Scale | Offset | Unit | Range | Description |
|--------|------|-------|--------|------|-------|-------------|
| BMS_nominalFullPackEnergy | 0 | 11 | 0.1 | 0 | kWh | 0–204.6 | Nominal full pack capacity |
| BMS_nominalEnergyRemaining | 11 | 11 | 0.1 | 0 | kWh | 0–204.6 | Nominal energy remaining |
| BMS_idealEnergyRemaining | 33 | 11 | 0.1 | 0 | kWh | 0–204.6 | Ideal energy remaining |
| BMS_energyToChargeComplete | 44 | 11 | 0.1 | 0 | kWh | 0–204.6 | Energy to full charge |
| BMS_expectedEnergyRemaining | 22 | 11 | 0.1 | 0 | kWh | 0–204.6 | Expected energy remaining |
| BMS_energyBuffer | 55 | 8 | 0.1 | 0 | kWh | 0–25.4 | Energy buffer |
| BMS_fullChargeComplete | 63 | 1 | 1 | 0 | — | 0–1 | Charge complete flag |

---

### 3.3 BMS_SOC (0x292 / 658)

| Signal | Bits | Scale | Offset | Unit | Range | Description |
|--------|------|-------|--------|------|-------|-------------|
| SOCmin | 0 | 10 | 0.1 | 0 | % | 0–102.3 | Min SOC |
| SOCUI | 10 | 10 | 0.1 | 0 | % | 0–102.3 | UI SOC |
| SOCmax | 20 | 10 | 0.1 | 0 | % | 0–102.3 | Max SOC |
| SOCave | 30 | 10 | 0.1 | 0 | % | 0–102.3 | Average SOC |
| BattBeginningOfLifeEnergy | 40 | 10 | 0.1 | 0 | kWh | 0–102.3 | Original pack energy |
| BMS_battTempPct | 50 | 8 | 0.4 | 0 | % | 0–100 | Battery temp % |

---

### 3.4 BMS_powerAvailable (0x252 / 594)

| Signal | Bits | Scale | Offset | Unit | Range | Description |
|--------|------|-------|--------|------|-------|-------------|
| BMS_maxRegenPower | 0 | 16 | 0.01 | 0 | kW | 0–655.35 | Max regen power |
| BMS_maxDischargePower | 16 | 16 | 0.013 | 0 | kW | 0–655.35 | Max discharge power |
| BMS_maxStationaryHeatPower | 32 | 10 | 0.01 | 0 | kW | 0–10.23 | Max waste heat |
| BMS_hvacPowerBudget | 50 | 10 | 0.02 | 0 | kW | 0–20.46 | HVAC power budget |
| BMS_powerDissipation | — | — | 0.02 | 0 | kW | — | Power dissipation |
| BMS_notEnoughPowerForHeatPump | 42 | 1 | 1 | 0 | — | 0–1 | Heat pump limit flag |
| BMS_powerLimitsState | 48 | 1 | 1 | 0 | — | 0–1 | Power calc state |

---

### 3.5 BMSthermal (0x312 / 786)

| Signal | Bits | Scale | Offset | Unit | Range | Description |
|--------|------|-------|--------|------|-------|-------------|
| BMSdissipation | 0 | 10 | 0.02 | 0 | kW | 0–20 | Battery dissipation |
| BMSflowRequest | 10 | 7 | 0.3 | 0 | LPM | 0–30 | Coolant flow request |
| BMSinletActiveCoolTarget | 17 | 9 | 0.25 | -25 | °C | -25–100 | Active cool target |
| BMSinletPassiveTarget | 26 | 9 | 0.25 | -25 | °C | -25–100 | Passive target |
| BMSinletActiveHeatTarget | 35 | 9 | 0.25 | -25 | °C | -25–100 | Active heat target |
| BMSminPackTemperature | 44 | 9 | 0.25 | -25 | °C | -25–100 | Min pack temp |
| BMSmaxPackTemperature | 53 | 9 | 0.25 | -25 | °C | -25–100 | Max pack temp |
| BMSnoFlowRequest | 63 | 1 | 1 | 0 | — | 0–1 | No flow flag |
| BMSpcsNoFlowRequest | 62 | 1 | 1 | 0 | — | 0–1 | PCS no flow flag |

---

### 3.6 BMS_status (0x212 / 530)

| Signal | Bits | Scale | Offset | Unit | Range | Description |
|--------|------|-------|--------|------|-------|-------------|
| BMS_hvacPowerRequest | 0 | 1 | 1 | 0 | — | 0–1 | HVAC power request |
| BMS_notEnoughPowerForDrive | 1 | 1 | 1 | 0 | — | 0–1 | Drive power limit |
| BMS_notEnoughPowerForSupport | 2 | 1 | 1 | 0 | — | 0–1 | Support power limit |
| BMS_preconditionAllowed | 3 | 1 | 1 | 0 | — | 0–1 | Precondition allowed |
| BMS_updateAllowed | 4 | 1 | 1 | 0 | — | 0–1 | Update allowed |
| BMS_activeHeatingWorthwhile | 5 | 1 | 1 | 0 | — | 0–1 | Heating worthwhile |
| BMS_cpMiaOnHvs | 6 | 1 | 1 | 0 | — | 0–1 | CP MIA on HVS |
| BMS_pcsPwmEnabled | 7 | 1 | 1 | 0 | — | 0–1 | PCS PWM enabled |
| BMS_contactorState | 8 | 3 | 1 | 0 | — | 0–6 | Contactor state |
| BMS_uiChargeStatus | 11 | 3 | 1 | 0 | — | 0–5 | UI charge status |
| BMS_ecuLogUploadRequest | 14 | 2 | 1 | 0 | — | 0–3 | ECU log upload |
| BMS_hvState | 16 | 3 | 1 | 0 | — | 0–6 | HV state |
| BMS_isolationResistance | 19 | 10 | 10 | 0 | kΩ | 0–10000 | Isolation resistance |
| BMS_chargeRequest | 29 | 1 | 1 | 0 | — | 0–1 | Charge request |
| BMS_keepWarmRequest | 30 | 1 | 1 | 0 | — | 0–1 | Keep warm |
| BMS_state | 32 | 4 | 1 | 0 | — | 0–10 | BMS state |
| BMS_diLimpRequest | 36 | 1 | 1 | 0 | — | 0–1 | DI limp request |
| BMS_okToShipByAir | 37 | 1 | 1 | 0 | — | 0–1 | Ship by air OK |
| BMS_okToShipByLand | 38 | 1 | 1 | 0 | — | 0–1 | Ship by land OK |
| BMS_chgPowerAvailable | 40 | 11 | 0.125 | 0 | kW | 0–255.75 | Charge power available |
| BMS_chargeRetryCount | 51 | 3 | 1 | 0 | — | 0–7 | Charge retry count |
| BMS_smStateRequest | 56 | 4 | 1 | 0 | — | 0–9 | SM state request |

---

### 3.7 BMSVAlimits (0x2D2 / 722)

| Signal | Bits | Scale | Offset | Unit | Range | Description |
|--------|------|-------|--------|------|-------|-------------|
| MinVoltage | 0 | 16 | 0.01 | 0 | V | 0–430 | BMS min voltage |
| MaxVoltage | 16 | 16 | 0.01 | 0 | V | 0–430 | BMS max voltage |
| MaxChargeCurrent | 32 | 14 | 0.1 | 0 | A | 0–1638.2 | Max charge current |
| MaxDischargeCurrent | 48 | 14 | 0.128 | 0 | A | 0–2096.9 | Max discharge current |

---

### 3.8 BattBrickMinMax (0x332 / 818)

| Signal | Bits | Scale | Offset | Unit | Range | Description |
|--------|------|-------|--------|------|-------|-------------|
| BattBrickVoltageMax | 2 | 12 | 0.002 | 0 | V | — | Max brick voltage |
| BattBrickVoltageMin | 16 | 12 | 0.002 | 0 | V | — | Min brick voltage |
| BattBrickTempMax | 16 | 8 | 0.5 | -40 | °C | — | Max brick temp |
| BattBrickTempMin | 24 | 8 | 0.5 | -40 | °C | — | Min brick temp |

---

### 3.9 BMS_packConfig (0x392 / 914)

| Signal | Bits | Scale | Offset | Unit | Range | Description |
|--------|------|-------|--------|------|-------|-------------|
| BMS_packMass | 16 | 8 | 1 | 300 | kg | 342–469 | Pack mass |
| BMS_platformMaxBusVoltage | 24 | 10 | 0.1 | 375 | V | — | Platform max bus voltage |
| BMS_moduleType | 8 | 3 | 1 | 0 | — | 0–4 | Module type |

---

## 4. PCS / DCDC Messages

### 4.1 PCSDCDCstatus (0x224 / 548)

| Signal | Bits | Scale | Offset | Unit | Range | Description |
|--------|------|-------|--------|------|-------|-------------|
| DCDCPrechargeStatus | 0 | 2 | 1 | 0 | — | 0–2 | Precharge status |
| DCDC12VSupportStatus | 2 | 2 | 1 | 0 | — | 0–2 | 12V support status |
| DCDCHvBusDischargeStatus | 4 | 2 | 1 | 0 | — | 0–2 | HV bus discharge |
| DCDCstate | 6 | 4 | 1 | 0 | — | 0–10 | DCDC state |
| DCDCSubState | 10 | 5 | 1 | 0 | — | 0–17 | DCDC sub-state |
| DCDCoutputCurrent | 16 | 12 | 0.1 | 0 | A | 0–409.5 | Output current |
| DCDCOutputIsLimited | 28 | 1 | 1 | 0 | — | 0–1 | Output limited |
| DCDCoutputCurrentMax | 29 | 12 | 0.1 | 0 | A | 0–409.5 | Max output current |
| DCDCFaulted | 15 | 1 | 1 | 0 | — | 0–1 | Faulted |
| DCDCPrechargeRtyCnt | 41 | 3 | 1 | 0 | — | 0–7 | Precharge retry |
| DCDCPrechargeRestartCnt | 56 | 3 | 1 | 0 | — | 0–7 | Precharge restart |
| DCDC12VSupportRtyCnt | 44 | 4 | 1 | 0 | — | 0–15 | 12V support retry |
| DCDCDischargeRtyCnt | 48 | 4 | 1 | 0 | — | 0–15 | Discharge retry |
| DCDCPwmEnableLine | 52 | 1 | 1 | 0 | — | 0–1 | PWM enable |
| DCDCSupportingFixedLvTarget | 53 | 1 | 1 | 0 | — | 0–1 | Fixed LV target |
| PCS_ecuLogUploadRequest | 54 | 2 | 1 | 0 | — | 0–3 | ECU log upload |
| DCDCInitialPrechargeSubState | 59 | 5 | 1 | 0 | — | 0–31 | Init precharge |

**DCDCstate values:** 0=Idle, 1=12vChg, 2=PrechargeStart, 3=Precharge, 4=HVactive, 5=Shutdown, 6=Error

---

### 4.2 PCS_thermalStatus (0x2A4 / 676)

PCS thermal status with DCDC temp, ambient temp, and charger phase temps.

---

## 5. HVP / Contactor Messages

### 5.1 HVP_contactorState (0x20A / 522)

| Signal | Bits | Scale | Offset | Unit | Description |
|--------|------|-------|--------|------|-------------|
| HVP_packContNegativeState | 0 | 3 | 1 | 0 | Pack negative contactor |
| HVP_packContPositiveState | 3 | 3 | 1 | 0 | Pack positive contactor |
| HVP_packContactorSetState | 8 | 4 | 1 | 0 | Pack contactor set state |
| HVP_fcContNegativeState | 12 | 3 | 1 | 0 | FC negative contactor |
| HVP_fcContPositiveState | 16 | 3 | 1 | 0 | FC positive contactor |
| HVP_fcContactorSetState | 19 | 4 | 1 | 0 | FC contactor set state |
| HVP_fcContNegativeAuxOpen | 7 | 1 | 1 | 0 | FC neg aux open |
| HVP_fcContPositiveAuxOpen | 6 | 1 | 1 | 0 | FC pos aux open |
| HVP_fcCtrsRequestStatus | 24 | 2 | 1 | 0 | FC request status |
| HVP_fcCtrsOpenRequested | 28 | 1 | 1 | 0 | FC open requested |
| HVP_fcCtrsOpenNowRequested | 27 | 1 | 1 | 0 | FC open now |
| HVP_fcCtrsResetRequestRequired | 26 | 1 | 1 | 0 | FC reset required |
| HVP_fcCtrsClosingAllowed | 29 | 1 | 1 | 0 | FC closing allowed |
| HVP_packCtrsRequestStatus | 30 | 2 | 1 | 0 | Pack request status |
| HVP_packCtrsOpenRequested | 34 | 1 | 1 | 0 | Pack open requested |
| HVP_packCtrsOpenNowRequested | 33 | 1 | 1 | 0 | Pack open now |
| HVP_packCtrsResetRequestRequired | 32 | 1 | 1 | 0 | Pack reset required |
| HVP_packCtrsClosingAllowed | 35 | 1 | 1 | 0 | Pack closing allowed |
| HVP_dcLinkAllowedToEnergize | 36 | 1 | 1 | 0 | DC link energize |
| HVP_pyroTestInProgress | 37 | 1 | 1 | 0 | Pyro test |
| HVP_hvilStatus | 40 | 4 | 1 | 0 | HVIL status |
| HVP_fcLinkAllowedToEnergize | 44 | 2 | 1 | 0 | FC link energize |

---

## 6. HV Battery & Charging

### 6.1 HVBattAmpVolt (0x132 / 306)

| Signal | Bits | Scale | Offset | Unit | Description |
|--------|------|-------|--------|------|-------------|
| BattVoltage | 0 | 16 | 0.01 | 0 | V | HV battery voltage |
| SmoothBattCurrent | 16 | 16 | -0.1 | 0 | A | Smoothed battery current |
| RawBattCurrent | 32 | 16 | -0.05 | 822 | A | Raw battery current |
| ChargeHoursRemaining | 48 | 12 | 1 | 0 | min | Charge time remaining |

---

### 6.2 TotalChargeDischarge (0x3D2 / 978)

| Signal | Bits | Scale | Offset | Unit | Description |
|--------|------|-------|--------|------|-------------|
| TotalDischargeKWh3D2 | 0 | 32 | 0.001 | 0 | kWh | Lifetime discharge |
| TotalChargeKWh3D2 | 32 | 32 | 0.001 | 0 | kWh | Lifetime charge |

---

### 6.3 BMS_kwhCounter (0x3D2 / 978)

Pack-level total charge and discharge (see 6.2).

### 6.4 BMS_kwhCountersMultiplexed (0x3F2 / 1010)

| Mux | Signal | Bits | Scale | Unit | Description |
|-----|--------|------|-------|------|-------------|
| 0 | BMS_acChargerKwhTotal | 8 | 32 | 0.001 | kWh | AC charger total |
| 1 | BMS_dcChargerKwhTotal | 8 | 32 | 0.001 | kWh | DC charger total |
| 2 | BMS_kwhRegenChargeTotal | 8 | 32 | 0.001 | kWh | Regen charge total |
| 3 | BMS_kwhDriveDischargeTotal | 8 | 32 | 0.001 | kWh | Drive discharge total |

**Used by emulator:** Yes — `TESLA-BATTERY.cpp` → `datalayer_extended.tesla.*` → MQTT `tesla_ac_charger_total_kwh`, `tesla_dc_charger_total_kwh`, `tesla_regen_charge_total_kwh`, `tesla_drive_discharge_total_kwh`.

---

### 6.4 ChargeLineStatus (0x264 / 612)

| Signal | Bits | Scale | Offset | Unit | Description |
|--------|------|-------|--------|------|-------------|
| ChargeLineVoltage264 | 0 | 14 | 0.0333 | 0 | V | Charger line voltage |
| ChargeLineCurrent264 | 14 | 9 | 0.1 | 0 | A | Charger line current |
| ChargeLinePower264 | 24 | 8 | 0.1 | 0 | kW | Charger line power |
| ChargeLineCurrentLimit264 | 32 | 10 | 0.1 | 0 | A | Current limit |

---

## 7. 12V Battery (VCFRONT)

### 7.1 _12vBattStatus (0x261 / 609)

| Signal | Bits | Scale | Offset | Unit | Description |
|--------|------|-------|--------|------|-------------|
| v12vBattVoltage261 | 32 | 12 | 0.00544368 | 0 | V | 12V battery voltage |
| v12vBattCurrent261 | 16 | 16 | -0.005 | 0 | A | 12V battery current |
| v12vBattTemp261 | 16 | 16 | -0.01 | 0 | °C | 12V battery temp |
| v12vBattAH261 | 2 | 14 | -0.005 | 0 | Ah | 12V capacity |

---

## 8. Vehicle & Other Messages

The DBC also defines messages for:

- **UI** — Display, trip planning, speed, odometer, SOC
- **Drive** — Torque, gear, pedal, brake, steering
- **HVAC** — Cabin temp, blower, evaporator, heat pump
- **Chassis** — Door status, latch, liftgate, wipers
- **Safety** — ESP, ABS, TPMS, EPB
- **DAS** — Autopilot, ACC, lane detection
- **Charging** — CP status, EVSE, pilot, proximity

---

## 9. Emulator Usage

| Message | CAN ID | Used by firmware | MQTT topic |
|---------|--------|------------------|-------------|
| PCS_dcdcRailStatus | 0x2B4 | ✓ | tesla_extended |
| BMS_energyStatus | 0x352 | ✓ | tesla_extended |
| BMS_SOC | 0x292 | ✓ | tesla_extended |
| BMS_powerAvailable | 0x252 | ✓ | tesla_extended |
| BMSthermal | 0x312 | ✓ | tesla_extended |
| BMS_status | 0x212 | ✓ | tesla_extended |
| BMSVAlimits | 0x2D2 | ✓ | tesla_extended |
| BattBrickMinMax | 0x332 | ✓ | tesla_extended |
| PCSDCDCstatus | 0x224 | ✓ | tesla_extended |
| HVP_contactorState | 0x20A | ✓ | tesla_extended |
| BMS_packConfig | 0x392 | ✓ | tesla_extended |
| BMS_kwhCounter | 0x3D2 | ✓ | tesla_extended |
| HVBattAmpVolt | 0x132 | ✓ | (generic datalayer) |
| TotalChargeDischarge | 0x3D2 | ✓ | tesla_extended |

---

## 10. References

- **12V DC rail spec:** `docs/tesla-12v-dc-rail-can-spec.md`
- **Firmware parser:** `Software/src/battery/TESLA-BATTERY.cpp`
- **Datalayer:** `Software/src/datalayer/datalayer_extended.h`
- **MQTT publish:** `Software/src/devboard/mqtt/mqtt.cpp` → `publish_tesla_extended()`

---

*Document generated from Tesla Model 3/Y DBC. DBC format version and metadata: `Modified "JW2021Aug29"`.*

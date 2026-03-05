#include "LAND-ROVER-VELAR-PHEV-BATTERY.h"
#include <cstring>  //For unit test
#include "../communication/can/comm_can.h"
#include "../datalayer/datalayer.h"
#include "../devboard/utils/events.h"
#include "../devboard/utils/common_functions.h"

#if VELAR_96_SEND_CHECKSUM
static uint8_t velar_crc8_96(CAN_frame* f) {
  // CRC8 over bytes 1-7, result for byte 0. JLR STJLR 18.036 style (SAE J1850 poly 0x1D).
  uint8_t crc = 0xFF;
  for (uint8_t j = 1; j < 8; j++) {
    crc = crc8_table_SAE_J1850_ZER0[crc ^ f->data.u8[j]];
  }
  return crc ^ 0xFF;
}
#endif

/* TODO
*/

void LandRoverVelarPhevBattery::update_values() {

  datalayer.battery.status.real_soc = HVBattSOCAverage;

  datalayer.battery.status.soh_pptt = HVBattStateofHealth * 10;

  datalayer.battery.status.voltage_dV = HVBattVoltageExt * 10;

  //datalayer.battery.status.current_dA;

  //datalayer.battery.status.max_charge_power_W;

  //datalayer.battery.status.max_discharge_power_W;

  datalayer.battery.status.remaining_capacity_Wh = static_cast<uint32_t>(
      (static_cast<double>(datalayer.battery.status.real_soc) / 10000) * datalayer.battery.info.total_capacity_Wh);

  datalayer.battery.status.cell_max_voltage_mV = HVBattCellVoltageMax;

  datalayer.battery.status.cell_min_voltage_mV = HVBattCellVoltageMin;

  datalayer.battery.status.temperature_min_dC = HVBattCellTempColdest * 10;

  datalayer.battery.status.temperature_max_dC = HVBattCellTempHottest * 10;

  if (HVBattHVILStatus) {
    set_event(EVENT_HVIL_FAILURE, 0);
  } else {
    clear_event(EVENT_HVIL_FAILURE);
  }

  if (HVBattAuxiliaryFuse) {
    set_event(EVENT_BATTERY_FUSE, 0);
  } else {
    clear_event(EVENT_BATTERY_FUSE);
  }

  if (HVBattAuxiliaryFuse) {
    set_event(EVENT_BATTERY_FUSE, 1);
  } else {
    clear_event(EVENT_BATTERY_FUSE);
  }

  if (HVBattAuxiliaryFuse) {
    set_event(EVENT_BATTERY_FUSE, 2);
  } else {
    clear_event(EVENT_BATTERY_FUSE);
  }

  if (HVBattStatusCritical == 2) {
    set_event(EVENT_THERMAL_RUNAWAY, 0);
  }
}

void LandRoverVelarPhevBattery::handle_incoming_can_frame(CAN_frame rx_frame) {
  switch (rx_frame.ID) {
    case 0x080:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      break;
    case 0x088:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      //HVBattChgExtGpCS
      //HVBattChgExtGpCounter
      //HVBattChgVoltageLimit
      //HVBattChgPowerLimitExt
      //HVBattChgContPwrLmt
      break;
    case 0x08A:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      //HVBattTotalCapacity
      //HVBattChgCurrentLimit
      //HVBattFastChgCounter
      HVBattPrechargeAllowed = (rx_frame.data.u8[6] & 0x10) >> 4;  // byte 6 bit 4, same as Range Rover 0x080
      //HVBattEndOfCharge
      //HVBattDerateWarning
      //HVBattCCCVChargeMode
      break;
    case 0x08C:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      //HVBattDchExtGpCS
      //HVBattDchExtGpCounter
      //HVBattDchVoltageLimit
      //HVBattDchContPwrLmt
      //HVBattDchPowerLimitExt
      break;
    case 0x08E:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      //HVBattEstEgyDischLoss
      //HVBattDchCurrentLimit
      //HVBattLossDchRouteTrac
      //HVBattLossDchRouteTot
      break;
    case 0x090:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      //HVBattVoltageOC
      //HVBattCurrentWarning
      //HVBattMILRequest
      HVBattTractionFuseF = (rx_frame.data.u8[2] & 0x40) >> 6;
      HVBattTractionFuseR = (rx_frame.data.u8[2] & 0x80) >> 7;
      break;
    case 0x098:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      //HVBattStatusGpCS
      //HVBattStatusGpCounter
      //HVBattOCMonitorStatus
      HVBattContactorStatus = (rx_frame.data.u8[0] & 0x80) >> 7;  // byte 0 bit 7, same as Range Rover 0x080
      HVBattHVILStatus = (rx_frame.data.u8[1] & 0x80) >> 7;
      //HVBattWeldCheckStatus
      //HVBattStatusCAT7NowBPO
      //HVBattStatusCAT6DlyBPO
      //HVBattStatusCAT5BPOChg
      //HVBattStatusCAT4Derate
      //HVBattStatusCAT3
      //HVBattIsolationStatus
      //HVBattCAT6Count
      HVBattAuxiliaryFuse = (rx_frame.data.u8[3] & 0x80) >> 7;
      HVBattStateofHealth = (((rx_frame.data.u8[4] & 0x03) << 8) | rx_frame.data.u8[5]);
      HVBattStatusCritical = (rx_frame.data.u8[5] & 0x0C) >> 2;
      //HVBattIsolationStatus2
      //HVBattClntPumpDiagStat
      //HVBattValveCtrlStat
      //HVIsolationTestStatus
      //HVBattIsoRes
      break;
    case 0x0C8:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      //HVBattPwrExtGpCS
      //HVBattPwrExtGpCounter
      //HVBattVoltageBusTF
      //HVBattVoltageBusTR
      //HVBattCurrentTR
      break;
    case 0x0CA:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      //HVBattPwrGpCS
      //HVBattVoltageBus
      HVBattVoltageExt = (((rx_frame.data.u8[3] & 0x03) << 8) | rx_frame.data.u8[4]);
      //HVBattPwrGpCounter
      //HVBattCurrentExt
      break;
    case 0x0EA:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      //HVBattStatExtGpCS
      //HVBattStatExtGpCounter
      //HVBattOCMonitorStatusTR
      //HVBattOCMonitorStatusTF
      //HVBattWeldCheckStatusT
      //HVBattWeldCheckStatusC
      //HVBattContactorStatusT
      break;
    case 0x18B:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      break;
    case 0x146:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      //HVBattEstimatedLossChg
      //HVBattEstimatedLossDch
      //HVBattEstLossDchTgtSoC
      //HVBattEstLossTracDch
      break;
    case 0x148:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      //HVBattThermalMgtStatus
      //HVBattThermalOvercheck
      //HVBattTempWarning
      //HVBattAvTempAtEvent
      //HVBattTempUpLimit
      HVBattCellTempHottest = (rx_frame.data.u8[5] / 2) - 40;
      HVBattCellTempColdest = (rx_frame.data.u8[6] / 2) - 40;
      //HVBattCellTempAverage
      break;
    case 0x14C:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      //HVBattCellVoltUpLimit
      HVBattCellVoltageMin = (((rx_frame.data.u8[4] & 0x1F) << 8) | rx_frame.data.u8[5]);
      HVBattCellVoltageMax = (((rx_frame.data.u8[6] & 0x1F) << 8) | rx_frame.data.u8[7]);
      //HVBattCellVoltWarning
      break;
    case 0x14E:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      //HVBattWarmupRateChg
      //HVBattWarmupRateDch
      //HVBattWarmupRateRtTot
      //HVBattWakeUpDchReq
      //HVBattWarmupRateRtTrac
      //HVBattWakeUpThermalReq
      //HVBattWakeUpTopUpReq
      break;
    case 0x171:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      //HVBattHeaterCtrlStat
      //HVBattCoolingRequestExt
      //HVBattFanDutyRequest
      //HVBattInletCoolantTemp
      //HVBattHeatPowerGenChg
      //HVBattHeatPowerGenDch
      //HVBattHeatPwrGenRtTot
      //HVBattCoolantLevel
      //HVBattHeatPwrGenRtTrac
      //HVBattHeatingRequest
      break;
    case 0x204:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      //HVBattChgExtGp2CS
      //HVBattChgExtGp2AC
      //HVBattChgContPwrLmtExt
      break;
    case 0x207:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      //HVBattAvgSOCOAtEvent
      //HVBattSOCLowestCell
      //HVBattSOCHighestCell
      HVBattSOCAverage = (((rx_frame.data.u8[6] & 0x3F) << 8) | rx_frame.data.u8[7]);
      break;
    case 0x27A:  //Cellvoltages
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      // Extract module ID (1-8) and sequence counter from first byte
      module_id = rx_frame.data.u8[0] >> 1;  // Bits 7-1: 1-8

      // Extract voltage group from second byte (1-4 for groups of 3 cells)
      voltage_group = rx_frame.data.u8[1] >> 2;  // Bits 7-2: 1-4

      // Calculate the starting index for these 3 cells
      // Each module has 12 cells (4 groups × 3 cells)
      // Module 1: cells 0-11, Module 2: cells 12-23, etc.
      // Voltage group 1: first 3 cells, group 2: next 3, etc.
      base_index = ((module_id - 1) * 12) + ((voltage_group - 1) * 3);

      // Decode the 3 cell voltages from the message
      // Format appears to be: high 4 bits in byte 2/4/6, low 8 bits in following byte
      datalayer.battery.status.cell_voltages_mV[base_index] = ((rx_frame.data.u8[2] & 0x0F) << 8) | rx_frame.data.u8[3];
      datalayer.battery.status.cell_voltages_mV[base_index + 1] =
          ((rx_frame.data.u8[4] & 0x0F) << 8) | rx_frame.data.u8[5];
      datalayer.battery.status.cell_voltages_mV[base_index + 2] =
          ((rx_frame.data.u8[6] & 0x0F) << 8) | rx_frame.data.u8[7];
      break;
    case 0x27E:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      //celltemperatures same as above
      break;
    case 0x310:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      //HVBattEnergyAvailable
      //HVBattEnergyUsableBulk
      //HVBattEnergyUsableMax
      //HVBattEnergyUsableMin
      break;
    case 0x318:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      //ID of min/max cells for temps and voltages
      break;
    default:
      break;
  }
}

void LandRoverVelarPhevBattery::transmit_can(unsigned long currentMillis) {
  // 10ms tick: GWM (0x008) + DCDC/Charger HVIL (0x96). Vehicle timing: 0x008 10ms, 0x96 per DBC.
  if (currentMillis - previousMillis10ms >= INTERVAL_10_MS) {
    previousMillis10ms = currentMillis;
#if VELAR_SEND_FRAME_0x008
    VELAR_0x008_GWM.data.u8[7] = velar_counter_008 & 0x0F;
    velar_counter_008++;
    transmit_can_frame(&VELAR_0x008_GWM);
#endif
    // 0x96 BCCM_PMZ_DCDCOperatingMode: ChargerHVILStatus=0 (OK) at bit 33. DCDCOpModeGpCounter at bits 11-14.
    VELAR_0x96_BCCM_DCDC.data.u8[1] = (VELAR_0x96_BCCM_DCDC.data.u8[1] & 0xC3) | ((velar_counter_96 & 0x0F) << 3);
    velar_counter_96++;
#if VELAR_96_SEND_CHECKSUM
    VELAR_0x96_BCCM_DCDC.data.u8[0] = velar_crc8_96(&VELAR_0x96_BCCM_DCDC);
#endif
    transmit_can_frame(&VELAR_0x96_BCCM_DCDC);
  }

  // Inverter HVIL (0xA4) 20ms – per DBC. Sending at 10ms may cause BMS rejection.
  if (currentMillis - previousMillis20ms >= INTERVAL_20_MS) {
    previousMillis20ms = currentMillis;
    transmit_can_frame(&VELAR_0xA4_InverterHVIL);
  }

  // BCCM keep-alive (0x18B) + PCM (0xA2) 50ms – contactor demand. Vehicle uses 50ms.
  // State machine:
  //   IDLE     – no close request, or within boot delay        → alive-only / open demand
  //   WAKEUP   – close requested, contactors not yet confirmed → wake-up pattern (0x24 0x09)
  //   CLOSED   – BMS confirms contactors closed               → drive/hold pattern (0x20 0x01)
  // Transition WAKEUP→CLOSED is driven by HVBattContactorStatus feedback, not a fixed timer.
  // This prevents chatter caused by the BMS re-opening on a mid-cycle byte change.
  if (currentMillis - previousMillis50ms >= INTERVAL_50_MS) {
    previousMillis50ms = currentMillis;
    bool effective_close = userRequestContactorClose && (currentMillis >= VELAR_CONTACTOR_DELAY_MS);

    // Record the moment we first assert close (for timeout fallback)
    if (effective_close && !velar_was_sending_close) {
      velar_close_started_ms = currentMillis;
    }
    velar_was_sending_close = effective_close;

    // Stay in wake-up pattern until BMS confirms closed, or fall through after timeout
    bool in_wakeup = effective_close && !HVBattContactorStatus &&
                     (currentMillis - velar_close_started_ms < VELAR_WAKEUP_PHASE_MS);

    if (effective_close) {
      VELAR_18B.data.u8[0] = VELAR_18B_BYTE0_CLOSED;
      VELAR_18B.data.u8[1] = 0x01;  // precharge request
      VELAR_0xA2_PCM_HVBatt.data.u8[0] = 0x21;  // vehicle closed
      VELAR_0xA2_PCM_HVBatt.data.u8[1] = 0x03;
      VELAR_0xA2_PCM_HVBatt.data.u8[5] = 0x00;  // HybridMode=0 (Standby)
      if (in_wakeup) {
        VELAR_0xA2_PCM_HVBatt.data.u8[6] = 0x24;  // wake-up pattern (Key Cycles)
        VELAR_0xA2_PCM_HVBatt.data.u8[7] = 0x09;
      } else {
        VELAR_0xA2_PCM_HVBatt.data.u8[6] = 0x20;  // drive/hold pattern (ShortDrive)
        VELAR_0xA2_PCM_HVBatt.data.u8[7] = 0x01;
      }
    } else {
      VELAR_18B.data.u8[0] = 0x01;  // alive only, no contactor demand
      VELAR_18B.data.u8[1] = 0x00;  // no precharge
      VELAR_0xA2_PCM_HVBatt.data.u8[0] = 0x72;  // vehicle open
      VELAR_0xA2_PCM_HVBatt.data.u8[1] = 0x04;
      VELAR_0xA2_PCM_HVBatt.data.u8[5] = 0x00;
      VELAR_0xA2_PCM_HVBatt.data.u8[6] = 0x04;  // Key Cycles open
      VELAR_0xA2_PCM_HVBatt.data.u8[7] = 0x06;
    }
    velar_counter_a2++;
    transmit_can_frame(&VELAR_18B);
    transmit_can_frame(&VELAR_0xA2_PCM_HVBatt);
  }

  // GWM_PMZ_V_HYBRID (0x18d) 60ms – hybrid vehicle state. Vehicle timing.
  if (currentMillis - previousMillis60ms >= INTERVAL_60_MS) {
    previousMillis60ms = currentMillis;
    VELAR_0x18d_GWM.data.u8[7] = velar_counter_18d & 0x0F;
    velar_counter_18d++;
    transmit_can_frame(&VELAR_0x18d_GWM);
  }

  // BCCMB_PMZ_A (0x224) 90ms – second BCCM-style message. Vehicle timing.
  if (currentMillis - previousMillis90ms >= INTERVAL_90_MS) {
    previousMillis90ms = currentMillis;
    VELAR_0x224_BCCMB.data.u8[7] = velar_counter_224 & 0x0F;
    velar_counter_224++;
    transmit_can_frame(&VELAR_0x224_BCCMB);
  }
}

void LandRoverVelarPhevBattery::setup(void) {  // Performs one time setup at startup
  strncpy(datalayer.system.info.battery_protocol, Name, 63);
  datalayer.system.info.battery_protocol[63] = '\0';
  datalayer.system.status.battery_allows_contactor_closing = true;
  datalayer.battery.info.number_of_cells = 72;
  datalayer.battery.info.total_capacity_Wh = 17000;
  datalayer.battery.info.max_design_voltage_dV = MAX_PACK_VOLTAGE_DV;
  datalayer.battery.info.min_design_voltage_dV = MIN_PACK_VOLTAGE_DV;
  datalayer.battery.info.max_cell_voltage_mV = MAX_CELL_VOLTAGE_MV;
  datalayer.battery.info.min_cell_voltage_mV = MIN_CELL_VOLTAGE_MV;
}

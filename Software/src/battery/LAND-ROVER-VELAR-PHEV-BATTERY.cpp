#include "LAND-ROVER-VELAR-PHEV-BATTERY.h"
#include <cstring>
#include "../communication/can/comm_can.h"
#include "../datalayer/datalayer.h"
#include "../devboard/utils/events.h"
#include "../devboard/utils/common_functions.h"

// CRC8 STJLR 18.036 style (SAE J1850 poly 0x1D) over bytes 1-7, result in byte 0.
static uint8_t velar_crc8(CAN_frame* f) {
  uint8_t crc = 0xFF;
  for (uint8_t j = 1; j < 8; j++) {
    crc = crc8_table_SAE_J1850_ZER0[crc ^ f->data.u8[j]];
  }
  return crc ^ 0xFF;
}

void LandRoverVelarPhevBattery::update_values() {
  datalayer.battery.status.real_soc = HVBattSOCAverage;
  datalayer.battery.status.soh_pptt = HVBattStateofHealth * 10;
  datalayer.battery.status.voltage_dV = HVBattVoltageExt * 10;
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

  if (HVBattTractionFuseF) {
    set_event(EVENT_BATTERY_FUSE, 1);
  } else {
    clear_event(EVENT_BATTERY_FUSE);
  }

  if (HVBattTractionFuseR) {
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
    case 0x088:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      break;
    case 0x08A:
      // BECM_PMZ_HVBattCharge_B: HVBattPrechargeAllowed at bit 52 (byte 6 bit 4).
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      HVBattPrechargeAllowed = (rx_frame.data.u8[6] & 0x10) >> 4;
      break;
    case 0x08C:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      break;
    case 0x08E:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      break;
    case 0x090:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      HVBattTractionFuseF = (rx_frame.data.u8[2] & 0x40) >> 6;
      HVBattTractionFuseR = (rx_frame.data.u8[2] & 0x80) >> 7;
      break;
    case 0x098:
      // BECM_PMZ_HVBattStatus_A. Per VELAR_PMZ_HSCAN.dbc:
      //   HVBattContactorStatus: start=14 (byte1 bit6) 0=Open 1=Closed
      //   HVBattHVILStatus:      start=15 (byte1 bit7) 0=OK   1=Not OK
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      HVBattContactorStatus = (rx_frame.data.u8[1] & 0x40) >> 6;
      HVBattHVILStatus = (rx_frame.data.u8[1] & 0x80) >> 7;
      HVBattAuxiliaryFuse = (rx_frame.data.u8[3] & 0x80) >> 7;
      HVBattStateofHealth = (((rx_frame.data.u8[4] & 0x03) << 8) | rx_frame.data.u8[5]);
      HVBattStatusCritical = (rx_frame.data.u8[5] & 0x0C) >> 2;
      break;
    case 0x0C8:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      break;
    case 0x0CA:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      HVBattVoltageExt = (((rx_frame.data.u8[3] & 0x03) << 8) | rx_frame.data.u8[4]);
      break;
    case 0x0EA:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      break;
    case 0x146:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      break;
    case 0x148:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      HVBattCellTempHottest = (rx_frame.data.u8[5] / 2) - 40;
      HVBattCellTempColdest = (rx_frame.data.u8[6] / 2) - 40;
      break;
    case 0x14C:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      HVBattCellVoltageMin = (((rx_frame.data.u8[4] & 0x1F) << 8) | rx_frame.data.u8[5]);
      HVBattCellVoltageMax = (((rx_frame.data.u8[6] & 0x1F) << 8) | rx_frame.data.u8[7]);
      break;
    case 0x14E:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      break;
    case 0x171:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      break;
    case 0x204:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      break;
    case 0x207:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      HVBattSOCAverage = (((rx_frame.data.u8[6] & 0x3F) << 8) | rx_frame.data.u8[7]);
      break;
    case 0x27A: {
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      module_id = rx_frame.data.u8[0] >> 1;
      voltage_group = rx_frame.data.u8[1] >> 2;
      if (module_id < 1 || module_id > 8 || voltage_group < 1 || voltage_group > 4) break;
      base_index = ((module_id - 1) * 12) + ((voltage_group - 1) * 3);
      datalayer.battery.status.cell_voltages_mV[base_index] = ((rx_frame.data.u8[2] & 0x0F) << 8) | rx_frame.data.u8[3];
      datalayer.battery.status.cell_voltages_mV[base_index + 1] =
          ((rx_frame.data.u8[4] & 0x0F) << 8) | rx_frame.data.u8[5];
      datalayer.battery.status.cell_voltages_mV[base_index + 2] =
          ((rx_frame.data.u8[6] & 0x0F) << 8) | rx_frame.data.u8[7];
      break;
    }
    case 0x27E:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      break;
    case 0x310:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      break;
    case 0x318:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      break;
    default:
      break;
  }
}

void LandRoverVelarPhevBattery::transmit_can(unsigned long currentMillis) {
  // 0x96 BCCM_PMZ_DCDCOperatingMode 10ms.
  // ChargerHVILStatus=0 (OK) at bits 33-34 (byte4 bits1-2). DCDCOpModeGpCounter at bits11-14 (byte1 bits3-6).
  // Byte0 = STJLR 18.036 CRC8 over bytes1-7.
  if (currentMillis - previousMillis10ms >= INTERVAL_10_MS) {
    previousMillis10ms = currentMillis;
    // Match ShortDrive: byte1 = 0x10 | (counter & 0x0F)
    VELAR_0x96_BCCM_DCDC.data.u8[1] = 0x10 | (velar_counter_96 & 0x0F);
    velar_counter_96++;
    VELAR_0x96_BCCM_DCDC.data.u8[0] = velar_crc8(&VELAR_0x96_BCCM_DCDC);
    transmit_can_frame(&VELAR_0x96_BCCM_DCDC);
  }

  // 0xA4 Inverter_PMZ_InverterHV 20ms. InverterHVILStatus=2 (OK) at byte4=0x20.
  if (currentMillis - previousMillis20ms >= INTERVAL_20_MS) {
    previousMillis20ms = currentMillis;
    transmit_can_frame(&VELAR_0xA4_InverterHVIL);
  }

  // 0xA2 PCM_PMZ_HVBatt 50ms. Match ShortDrive: bytes 6-7 = 20 01 (close) / 00 00 (open).
  if (currentMillis - previousMillis50ms >= INTERVAL_50_MS) {
    previousMillis50ms = currentMillis;

    bool effective_close = userRequestContactorClose && (currentMillis >= VELAR_CONTACTOR_DELAY_MS);
    velar_counter_a2++;
    uint8_t count = velar_counter_a2 & 0x0F;

    // byte1: match ShortDrive - counter in lower nibble
    uint8_t byte1 = count;

    if (effective_close) {
      // HybridMode=2 (Hybrid Battery Only) byte5 bits3-6 = 0x10
      // HVBattContactorRequest=1 byte6 bit5 = 0x20
      // PwrSupWakeUpAllowed=1 (HV Battery) byte7 bits4-5 = 0x10
      VELAR_0xA2_PCM_HVBatt.data.u8[1] = byte1;
      VELAR_0xA2_PCM_HVBatt.data.u8[5] = 0x10;
      VELAR_0xA2_PCM_HVBatt.data.u8[6] = 0x20;
      VELAR_0xA2_PCM_HVBatt.data.u8[7] = 0x10;
    } else {
      // HybridMode=0 (Standby), no contactor request, no wakeup
      VELAR_0xA2_PCM_HVBatt.data.u8[1] = byte1;
      VELAR_0xA2_PCM_HVBatt.data.u8[5] = 0x00;
      VELAR_0xA2_PCM_HVBatt.data.u8[6] = 0x00;
      VELAR_0xA2_PCM_HVBatt.data.u8[7] = 0x00;
    }
    VELAR_0xA2_PCM_HVBatt.data.u8[0] = velar_crc8(&VELAR_0xA2_PCM_HVBatt);
    transmit_can_frame(&VELAR_0xA2_PCM_HVBatt);
  }
}

void LandRoverVelarPhevBattery::setup(void) {
  strncpy(datalayer.system.info.battery_protocol, Name, 63);
  datalayer.system.info.battery_protocol[63] = '\0';
  datalayer.system.status.battery_allows_contactor_closing = true;
  datalayer.battery.info.number_of_cells = 72;
  datalayer.battery.info.total_capacity_Wh = 17000;
  datalayer.battery.info.max_design_voltage_dV = MAX_PACK_VOLTAGE_DV;
  datalayer.battery.info.min_design_voltage_dV = MIN_PACK_VOLTAGE_DV;
  datalayer.battery.info.max_cell_voltage_mV = MAX_CELL_VOLTAGE_MV;
  datalayer.battery.info.min_cell_voltage_mV = MIN_CELL_VOLTAGE_MV;
  datalayer.battery.info.max_cell_voltage_deviation_mV = MAX_CELL_DEVIATION_MV;
}

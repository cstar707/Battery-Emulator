#include "LAND-ROVER-VELAR-PHEV-BATTERY.h"
#include <cstring>
#include "../communication/can/comm_can.h"
#include "../datalayer/datalayer.h"
#include "../devboard/utils/common_functions.h"
#include "../devboard/utils/events.h"

namespace {

static uint8_t velar_crc8(CAN_frame* f) {
  uint8_t crc = 0xFF;
  for (uint8_t j = 1; j < 8; j++) {
    crc = crc8_table_SAE_J1850_ZER0[crc ^ f->data.u8[j]];
  }
  return crc ^ 0xFF;
}

static CAN_frame make_frame(uint16_t id,
                            uint8_t b0,
                            uint8_t b1,
                            uint8_t b2,
                            uint8_t b3,
                            uint8_t b4,
                            uint8_t b5,
                            uint8_t b6,
                            uint8_t b7) {
  CAN_frame frame = {};
  frame.FD = false;
  frame.ext_ID = false;
  frame.DLC = 8;
  frame.ID = id;
  frame.data.u8[0] = b0;
  frame.data.u8[1] = b1;
  frame.data.u8[2] = b2;
  frame.data.u8[3] = b3;
  frame.data.u8[4] = b4;
  frame.data.u8[5] = b5;
  frame.data.u8[6] = b6;
  frame.data.u8[7] = b7;
  return frame;
}

static CAN_frame VELAR_0x013 = make_frame(0x013, 0x31, 0xF1, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00);
static CAN_frame VELAR_0x041 = make_frame(0x041, 0x49, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
static CAN_frame VELAR_0x00D = make_frame(0x00D, 0x24, 0x03, 0x03, 0xE8, 0x03, 0xE8, 0x00, 0x00);
static CAN_frame VELAR_0x0AA = make_frame(0x0AA, 0x1E, 0x0A, 0x01, 0x50, 0x3F, 0xFE, 0x3F, 0xFF);
static CAN_frame VELAR_0x2DD = make_frame(0x2DD, 0xF4, 0x02, 0xF2, 0xC0, 0x0F, 0xFC, 0x00, 0x00);
static CAN_frame VELAR_0x072 = make_frame(0x072, 0x00, 0x00, 0x00, 0x00, 0x05, 0x4D, 0x01, 0x52);
static CAN_frame VELAR_0x050 = make_frame(0x050, 0x5E, 0x01, 0x1D, 0x80, 0x00, 0x80, 0x3C, 0x50);
static CAN_frame VELAR_0x067 = make_frame(0x067, 0xF2, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

static CAN_frame VELAR_0x029 = make_frame(0x029, 0x15, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
static CAN_frame VELAR_0x036 = make_frame(0x036, 0x89, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
static CAN_frame VELAR_0x07A = make_frame(0x07A, 0xD8, 0x0B, 0x01, 0xFD, 0x00, 0x00, 0x00, 0x00);
static CAN_frame VELAR_0x084 = make_frame(0x084, 0xD8, 0x58, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00);
static CAN_frame VELAR_0x15B = make_frame(0x15B, 0x00, 0x00, 0x00, 0x01, 0x00, 0x78, 0x00, 0x96);
static CAN_frame VELAR_0x0A8 = make_frame(0x0A8, 0xF6, 0x01, 0x01, 0x50, 0x00, 0x00, 0x00, 0x00);
static CAN_frame VELAR_0x0BF = make_frame(0x0BF, 0x53, 0x0D, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00);
static CAN_frame VELAR_0x0D9 = make_frame(0x0D9, 0x00, 0x00, 0x19, 0xA6, 0x01, 0x01, 0x00, 0x00);
static CAN_frame VELAR_0x0DB = make_frame(0x0DB, 0x00, 0x6B, 0x00, 0x00, 0x01, 0xA6, 0x00, 0x00);
static CAN_frame VELAR_0x0DD = make_frame(0x0DD, 0x00, 0x00, 0x00, 0x00, 0xFE, 0x24, 0x03, 0x1F);
static CAN_frame VELAR_0x0E8 = make_frame(0x0E8, 0x00, 0x00, 0x03, 0x9B, 0x00, 0x14, 0x02, 0x00);
static CAN_frame VELAR_0x0F7 = make_frame(0x0F7, 0x01, 0xA6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
static CAN_frame VELAR_0x0FB = make_frame(0x0FB, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
static CAN_frame VELAR_0x011 = make_frame(0x011, 0x7F, 0xCB, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00);
static CAN_frame VELAR_0x104 = make_frame(0x104, 0x00, 0x00, 0x00, 0x04, 0x00, 0x1A, 0x00, 0x00);
static CAN_frame VELAR_0x143 = make_frame(0x143, 0xFE, 0x0D, 0x01, 0x4F, 0x00, 0x40, 0x00, 0x00);
static CAN_frame VELAR_0x159 = make_frame(0x159, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x08, 0x36);
static CAN_frame VELAR_0x162 = make_frame(0x162, 0x00, 0x1C, 0x28, 0x00, 0x00, 0x00, 0x0A, 0x0A);
static CAN_frame VELAR_0x164 = make_frame(0x164, 0x00, 0x00, 0x00, 0x00, 0x32, 0x0A, 0x02, 0x00);
static CAN_frame VELAR_0x166 = make_frame(0x166, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
static CAN_frame VELAR_0x177 = make_frame(0x177, 0x00, 0x3B, 0x00, 0x00, 0x68, 0x00, 0x50, 0x28);
static CAN_frame VELAR_0x183 = make_frame(0x183, 0x0A, 0x0A, 0x00, 0x5A, 0x40, 0x00, 0x00, 0x8C);
static CAN_frame VELAR_0x1A1 = make_frame(0x1A1, 0xEF, 0x11, 0x36, 0x00, 0x02, 0x82, 0x02, 0x00);
static CAN_frame VELAR_0x1B1 = make_frame(0x1B1, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00);
static CAN_frame VELAR_0x1B2 = make_frame(0x1B2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
static CAN_frame VELAR_0x200 = make_frame(0x200, 0x00, 0x00, 0x00, 0x00, 0x73, 0x40, 0xFE, 0x24);
static CAN_frame VELAR_0x206 = make_frame(0x206, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xA6);
static CAN_frame VELAR_0x217 = make_frame(0x217, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
static CAN_frame VELAR_0x230 = make_frame(0x230, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
static CAN_frame VELAR_0x2B4 = make_frame(0x2B4, 0x00, 0x00, 0x00, 0x0F, 0x1F, 0x00, 0x00, 0x30);
static CAN_frame VELAR_0x2C1 = make_frame(0x2C1, 0x00, 0x00, 0x28, 0x01, 0x01, 0xC2, 0x41, 0xC2);
static CAN_frame VELAR_0x2E2 = make_frame(0x2E2, 0x01, 0x31, 0x31, 0x56, 0x1F, 0xEE, 0x00, 0x85);
static CAN_frame VELAR_0x2E3 = make_frame(0x2E3, 0x02, 0x76, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
static CAN_frame VELAR_0x2E4 = make_frame(0x2E4, 0x02, 0x47, 0x00, 0x00, 0x00, 0x00, 0x01, 0x8D);
static CAN_frame VELAR_0x0310 = make_frame(0x310, 0x00, 0x36, 0x00, 0x00, 0x01, 0x4D, 0x00, 0x30);
static CAN_frame VELAR_0x0318 = make_frame(0x318, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x22, 0x02, 0x08);
static CAN_frame VELAR_0x0400 = make_frame(0x400, 0x72, 0x24, 0x12, 0x49, 0x89, 0x09, 0x09, 0x09);
static CAN_frame VELAR_0x0405 = make_frame(0x405, 0x10, 0x00, 0x00, 0x00, 0x00, 0x53, 0x41, 0x44);
static CAN_frame VELAR_0x0501 = make_frame(0x501, 0x1E, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00);
static CAN_frame VELAR_0x051E = make_frame(0x51E, 0x1F, 0x02, 0x02, 0x3F, 0x00, 0x00, 0x00, 0x00);
static CAN_frame VELAR_0x051F = make_frame(0x51F, 0x01, 0x12, 0x02, 0x3F, 0x00, 0x00, 0x00, 0x00);
static CAN_frame VELAR_0x0583 = make_frame(0x583, 0x83, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
static CAN_frame VELAR_0x0595 = make_frame(0x595, 0x95, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
static CAN_frame VELAR_0x059E = make_frame(0x59E, 0x9E, 0x10, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
static CAN_frame VELAR_0x0EC = make_frame(0x0EC, 0x00, 0x00, 0x01, 0x37, 0x01, 0xED, 0x87, 0x87);
static CAN_frame VELAR_0x0EF = make_frame(0x0EF, 0x00, 0x00, 0x0E, 0x1E, 0xF3, 0x6B, 0xC3, 0x6A);

}  // namespace

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
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      HVBattPrechargeAllowed = (rx_frame.data.u8[6] & 0x10) >> 4;
      break;
    case 0x08C:
    case 0x08E:
    case 0x0C8:
    case 0x0EA:
    case 0x146:
    case 0x14E:
    case 0x171:
    case 0x204:
    case 0x27E:
    case 0x310:
    case 0x318:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      break;
    case 0x090:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      HVBattTractionFuseF = (rx_frame.data.u8[2] & 0x40) >> 6;
      HVBattTractionFuseR = (rx_frame.data.u8[2] & 0x80) >> 7;
      break;
    case 0x098:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      HVBattContactorStatus = (rx_frame.data.u8[1] & 0x40) >> 6;
      HVBattHVILStatus = (rx_frame.data.u8[1] & 0x80) >> 7;
      HVBattAuxiliaryFuse = (rx_frame.data.u8[3] & 0x80) >> 7;
      HVBattStateofHealth = (((rx_frame.data.u8[4] & 0x03) << 8) | rx_frame.data.u8[5]);
      HVBattStatusCritical = (rx_frame.data.u8[5] & 0x0C) >> 2;
      break;
    case 0x0CA:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      HVBattVoltageExt = (((rx_frame.data.u8[3] & 0x03) << 8) | rx_frame.data.u8[4]);
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
    case 0x207:
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      HVBattSOCAverage = (((rx_frame.data.u8[6] & 0x3F) << 8) | rx_frame.data.u8[7]);
      break;
    case 0x27A: {
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;
      module_id = rx_frame.data.u8[0] >> 1;
      voltage_group = rx_frame.data.u8[1] >> 2;
      if (module_id < 1 || module_id > 8 || voltage_group < 1 || voltage_group > 4) {
        break;
      }
      base_index = ((module_id - 1) * 12) + ((voltage_group - 1) * 3);
      datalayer.battery.status.cell_voltages_mV[base_index] = ((rx_frame.data.u8[2] & 0x0F) << 8) | rx_frame.data.u8[3];
      datalayer.battery.status.cell_voltages_mV[base_index + 1] =
          ((rx_frame.data.u8[4] & 0x0F) << 8) | rx_frame.data.u8[5];
      datalayer.battery.status.cell_voltages_mV[base_index + 2] =
          ((rx_frame.data.u8[6] & 0x0F) << 8) | rx_frame.data.u8[7];
      break;
    }
    default:
      break;
  }
}

void LandRoverVelarPhevBattery::transmit_can(unsigned long currentMillis) {
  if (userRequestContactorClose && !lastUserRequestContactorClose) {
    closeRequestStartedMs = currentMillis;
  }
  lastUserRequestContactorClose = userRequestContactorClose;

  const bool effective_close =
      userRequestContactorClose &&
      (HVBattContactorStatus || ((currentMillis - closeRequestStartedMs) >= VELAR_CONTACTOR_DELAY_MS));

  if (currentMillis - previousMillis10ms >= INTERVAL_10_MS) {
    previousMillis10ms = currentMillis;

    velar_counter_a2 = (velar_counter_a2 + 1) & 0x0F;
    VELAR_0xA2_PCM_HVBatt.data.u8[1] = velar_counter_a2;
    if (effective_close) {
      VELAR_0xA2_PCM_HVBatt.data.u8[5] = 0x10;
      VELAR_0xA2_PCM_HVBatt.data.u8[6] = 0x20;
      VELAR_0xA2_PCM_HVBatt.data.u8[7] = 0x01;
    } else {
      VELAR_0xA2_PCM_HVBatt.data.u8[5] = 0x00;
      VELAR_0xA2_PCM_HVBatt.data.u8[6] = 0x00;
      VELAR_0xA2_PCM_HVBatt.data.u8[7] = 0x00;
    }
    VELAR_0xA2_PCM_HVBatt.data.u8[0] = velar_crc8(&VELAR_0xA2_PCM_HVBatt);
    transmit_can_frame(&VELAR_0xA2_PCM_HVBatt);

    transmit_can_frame(&VELAR_0x013);
    transmit_can_frame(&VELAR_0x041);
    transmit_can_frame(&VELAR_0x00D);
    transmit_can_frame(&VELAR_0x0AA);
    transmit_can_frame(&VELAR_0x2DD);
  }

  if (currentMillis - previousMillis20ms >= INTERVAL_20_MS) {
    previousMillis20ms = currentMillis;

    VELAR_0x96_BCCM_DCDC.data.u8[1] = 0x10 | (velar_counter_96 & 0x0F);
    velar_counter_96 = (velar_counter_96 + 1) & 0x0F;
    VELAR_0x96_BCCM_DCDC.data.u8[0] = velar_crc8(&VELAR_0x96_BCCM_DCDC);
    transmit_can_frame(&VELAR_0x96_BCCM_DCDC);
    transmit_can_frame(&VELAR_0xA4_InverterHVIL);

    transmit_can_frame(&VELAR_0x072);
    transmit_can_frame(&VELAR_0x050);
    transmit_can_frame(&VELAR_0x067);
    transmit_can_frame(&VELAR_0x029);
    transmit_can_frame(&VELAR_0x036);
    transmit_can_frame(&VELAR_0x07A);
    transmit_can_frame(&VELAR_0x084);
    transmit_can_frame(&VELAR_0x15B);
    transmit_can_frame(&VELAR_0x0A8);
    transmit_can_frame(&VELAR_0x0BF);
    transmit_can_frame(&VELAR_0x0D9);
    transmit_can_frame(&VELAR_0x0DB);
  }

  if (currentMillis - previousMillis50ms >= INTERVAL_50_MS) {
    previousMillis50ms = currentMillis;
    transmit_can_frame(&VELAR_0x0DD);
    transmit_can_frame(&VELAR_0x0E8);
    transmit_can_frame(&VELAR_0x0F7);
    transmit_can_frame(&VELAR_0x0FB);
  }

  if (currentMillis - previousMillis100ms >= INTERVAL_100_MS) {
    previousMillis100ms = currentMillis;
    transmit_can_frame(&VELAR_0x011);
    transmit_can_frame(&VELAR_0x104);
    transmit_can_frame(&VELAR_0x143);
    transmit_can_frame(&VELAR_0x159);
    transmit_can_frame(&VELAR_0x162);
    transmit_can_frame(&VELAR_0x164);
    transmit_can_frame(&VELAR_0x166);
    transmit_can_frame(&VELAR_0x177);
    transmit_can_frame(&VELAR_0x183);
    transmit_can_frame(&VELAR_0x1A1);
    transmit_can_frame(&VELAR_0x1B1);
    transmit_can_frame(&VELAR_0x1B2);
    transmit_can_frame(&VELAR_0x200);
    transmit_can_frame(&VELAR_0x206);
    transmit_can_frame(&VELAR_0x217);
    transmit_can_frame(&VELAR_0x230);
    transmit_can_frame(&VELAR_0x2B4);
    transmit_can_frame(&VELAR_0x2C1);
    transmit_can_frame(&VELAR_0x2E2);
    transmit_can_frame(&VELAR_0x2E3);
    transmit_can_frame(&VELAR_0x2E4);
    transmit_can_frame(&VELAR_0x0310);
    transmit_can_frame(&VELAR_0x0318);
    transmit_can_frame(&VELAR_0x0400);
    transmit_can_frame(&VELAR_0x0405);
    transmit_can_frame(&VELAR_0x0501);
    transmit_can_frame(&VELAR_0x051E);
    transmit_can_frame(&VELAR_0x051F);
    transmit_can_frame(&VELAR_0x0583);
    transmit_can_frame(&VELAR_0x0595);
    transmit_can_frame(&VELAR_0x059E);
    transmit_can_frame(&VELAR_0x0EC);
    transmit_can_frame(&VELAR_0x0EF);
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

#ifndef LAND_ROVER_VELAR_PHEV_BATTERY_H
#define LAND_ROVER_VELAR_PHEV_BATTERY_H

#include "CanBattery.h"

// Set to 0 to stop sending ID 0x008 (avoids conflict / Stuff Error if vehicle also sends 0x8; may allow contactors). Set to 1 to send 0x008 again.
#define VELAR_SEND_FRAME_0x008 0

class LandRoverVelarPhevBattery : public CanBattery {
 public:
  virtual void setup(void);
  virtual void handle_incoming_can_frame(CAN_frame rx_frame);
  virtual void update_values();
  virtual void transmit_can(unsigned long currentMillis);
  static constexpr const char* Name = "Range Rover Velar 17kWh PHEV battery (L560)";

 private:
  /* Change the following to suit your battery */
  static const int MAX_PACK_VOLTAGE_DV = 4710;
  static const int MIN_PACK_VOLTAGE_DV = 3000;
  static const int MAX_CELL_VOLTAGE_MV = 4250;  //Battery is put into emergency stop if one cell goes over this value
  static const int MIN_CELL_VOLTAGE_MV = 2700;  //Battery is put into emergency stop if one cell goes below this value
  static const int MAX_CELL_DEVIATION_MV = 150;

  unsigned long previousMillis10ms = 0;
  unsigned long previousMillis20ms = 0;
  unsigned long previousMillis50ms = 0;
  unsigned long previousMillis60ms = 0;
  unsigned long previousMillis90ms = 0;

  // Rolling counters for vehicle frames (BMS may expect changing values). Increment before each send.
  uint8_t velar_counter_008 = 0;
  uint8_t velar_counter_18d = 0;
  uint8_t velar_counter_224 = 0;

  uint16_t HVBattStateofHealth = 1000;
  uint16_t HVBattSOCAverage = 5000;
  uint16_t HVBattVoltageExt = 370;
  uint16_t HVBattCellVoltageMin = 3700;
  uint16_t HVBattCellVoltageMax = 3700;
  int16_t HVBattCellTempHottest = 0;
  int16_t HVBattCellTempColdest = 0;
  uint8_t HVBattStatusCritical = 1;  //1=OK, 2 = FAULT
  uint8_t voltage_group = 0;
  uint8_t module_id = 0;
  uint8_t base_index = 0;
  bool HVBattHVILStatus = false;         // 0=OK, 1=Not OK (from BMS)
  bool HVBattContactorStatus = false;    // 0=open, 1=closed (from BMS 0x98)
  bool HVBattPrechargeAllowed = false;  // from BMS 0x08A byte6 bit4; BMS may only set after precharge request
  bool HVBattAuxiliaryFuse = false;      // 0=OK, 1=Not OK
  bool HVBattTractionFuseF = false;    // 0=OK, 1=Not OK
  bool HVBattTractionFuseR = false;    // 0=OK, 1=Not OK

  // BCCM_PMZ_A (0x18B) 50ms. Byte 0: bit0=alive, bit1=contactor demand; trying bit2=1 (0x07) in case BMS needs it. Byte 1: precharge request.
  CAN_frame VELAR_18B = {.FD = false,
                         .ext_ID = false,
                         .DLC = 8,
                         .ID = 0x18B,
                         .data = {0x07, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};  // 0x07 = alive+contactor+bit2; revert to 0x03 if no help

  // Inverter HVIL status (0xA4, 20ms cyclic). EPIC normally sends this; emulator sends it when no inverter.
  // Keeps HVIL error cleared so BMS/vehicle logic sees "inverter HVIL OK".
  CAN_frame VELAR_0xA4_InverterHVIL = {.FD = false,
                                        .ext_ID = false,
                                        .DLC = 8,
                                        .ID = 0xA4,
                                        .data = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

  // Vehicle presence frames (may be required for BMS to allow contactor close). Same IDs as Range Rover PHEV.
  // Payloads are minimal defaults; tune from vehicle capture if contactors still refuse to close.
  CAN_frame VELAR_0x008_GWM = {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x008,
                              .data = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};   // GWM_PMZ_A 10ms
  CAN_frame VELAR_0x18d_GWM = {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x18d,
                               .data = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};  // GWM_PMZ_V_HYBRID 60ms
  CAN_frame VELAR_0x224_BCCMB = {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x224,
                                  .data = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};  // BCCMB_PMZ_A 90ms
};

#endif

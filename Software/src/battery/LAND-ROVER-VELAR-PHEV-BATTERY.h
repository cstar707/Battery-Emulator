#ifndef LAND_ROVER_VELAR_PHEV_BATTERY_H
#define LAND_ROVER_VELAR_PHEV_BATTERY_H

#include "CanBattery.h"

// 0x96 checksum: compute CRC8 in byte 0 over bytes 1-7 (STJLR 18.036 style).
#define VELAR_96_SEND_CHECKSUM 1

// 0xA2 checksum: same STJLR 18.036 CRC8 in byte 0 over bytes 1-7.
#define VELAR_A2_SEND_CHECKSUM 1

// Delay before sending contactor demand (let BMS init after power-on).
#define VELAR_CONTACTOR_DELAY_MS 1000

class LandRoverVelarPhevBattery : public CanBattery {
 public:
  virtual void setup(void);
  virtual void handle_incoming_can_frame(CAN_frame rx_frame);
  virtual void update_values();
  virtual void transmit_can(unsigned long currentMillis);
  bool supports_contactor_close() { return true; }
  void request_open_contactors() { userRequestContactorClose = false; }
  void request_close_contactors() { userRequestContactorClose = true; }
  static constexpr const char* Name = "Range Rover Velar 17kWh PHEV battery (L560)";

 private:
  static const int MAX_PACK_VOLTAGE_DV = 4710;
  static const int MIN_PACK_VOLTAGE_DV = 3000;
  static const int MAX_CELL_VOLTAGE_MV = 4250;
  static const int MIN_CELL_VOLTAGE_MV = 2700;
  static const int MAX_CELL_DEVIATION_MV = 150;

  unsigned long previousMillis10ms = 0;
  unsigned long previousMillis20ms = 0;
  unsigned long previousMillis50ms = 0;
  unsigned long previousMillis100ms = 0;
  unsigned long previousMillis200ms = 0;
  unsigned long previousMillis500ms = 0;
  unsigned long closeRequestStartedMs = 0;

  uint8_t velar_counter_a2 = 0;
  uint8_t velar_counter_96 = 0;

  uint16_t HVBattStateofHealth = 1000;
  uint16_t HVBattSOCAverage = 5000;
  uint16_t HVBattVoltageExt = 370;
  uint16_t HVBattCellVoltageMin = 3700;
  uint16_t HVBattCellVoltageMax = 3700;
  int16_t HVBattCellTempHottest = 0;
  int16_t HVBattCellTempColdest = 0;
  uint8_t HVBattStatusCritical = 1;
  uint8_t voltage_group = 0;
  uint8_t module_id = 0;
  uint8_t base_index = 0;
  bool HVBattHVILStatus = false;
  bool HVBattContactorStatus = false;
  bool HVBattPrechargeAllowed = false;
  bool HVBattAuxiliaryFuse = false;
  bool HVBattTractionFuseF = false;
  bool HVBattTractionFuseR = false;
  bool userRequestContactorClose = false;
  bool lastUserRequestContactorClose = false;

  // PCM_PMZ_HVBatt (0xA2). Per VELAR_PMZ_HSCAN.dbc:
  //   byte0 = HVBattContDemandTCS checksum (STJLR 18.036 CRC8 over bytes 1-7)
  //   byte1 bits 3-6 = HVBattContDemandTCount rolling counter 0-15
  //   byte6 bit5 (0x20) = HVBattContactorRequest: 0=Open, 1=Closed
  //   byte6 bit4 (0x10) = HVBattBusTestRequest
  //   byte7 = rolling counter (lower nibble, matches vehicle capture)
  // Set dynamically in transmit_can() based on userRequestContactorClose.
  CAN_frame VELAR_0xA2_PCM_HVBatt = {.FD = false,
                                      .ext_ID = false,
                                      .DLC = 8,
                                      .ID = 0xA2,
                                      .data = {0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

  // BCCM_PMZ_DCDCOperatingMode (0x96). Match the fixed build logs: bytes 2-7 = 01 4F 02 00 00 00.
  CAN_frame VELAR_0x96_BCCM_DCDC = {.FD = false,
                                     .ext_ID = false,
                                     .DLC = 8,
                                     .ID = 0x96,
                                     .data = {0x00, 0x00, 0x01, 0x4F, 0x02, 0x00, 0x00, 0x00}};

  // Inverter_PMZ_InverterHV (0xA4, 20ms). Match ShortDrive: 00 00 00 00 27 FE 10 00.
  CAN_frame VELAR_0xA4_InverterHVIL = {.FD = false,
                                        .ext_ID = false,
                                        .DLC = 8,
                                        .ID = 0xA4,
                                        .data = {0x00, 0x00, 0x00, 0x00, 0x27, 0xFE, 0x10, 0x00}};
};

#endif

#ifndef LAND_ROVER_VELAR_PHEV_BATTERY_H
#define LAND_ROVER_VELAR_PHEV_BATTERY_H

#include "CanBattery.h"

// Set to 0 to stop sending ID 0x008 (avoids conflict if vehicle also sends 0x8). Set to 1 to send.
#define VELAR_SEND_FRAME_0x008 1

// Tuning: 0x03 = alive+contactor only, 0x07 = +bit2 (HV enable). Try 0x07 if contactors chatter.
#define VELAR_18B_BYTE0_CLOSED 0x07

// 0xA2 byte 7 when closed: 0 = rolling counter|0x10, 1 = fixed 0x09 (matches vehicle BLF).
#define VELAR_A2_BYTE7_FIXED_09 1

// 0x96 checksum: 1 = compute CRC8 in byte 0 over bytes 1-7 (STJLR 18.036 style).
#define VELAR_96_SEND_CHECKSUM 1

// Phased approach: delay contactor demand after boot, then wake-up pattern, then drive pattern.
#define VELAR_CONTACTOR_DELAY_MS 5000   // No 0x18B/0xA2 close for first 5s (let BMS init)
#define VELAR_WAKEUP_PHASE_MS 12000     // Use 24 09 (wake-up) for 12s after close starts, then 20 01 (drive)

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
  uint8_t velar_counter_a2 = 0;
  uint8_t velar_counter_96 = 0;

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
  bool userRequestContactorClose = false;  // User must request close via web UI; PCM/BCCM demand only sent when true
  unsigned long velar_close_started_ms = 0;   // When we first started sending close (for phased 0xA2)
  bool velar_was_sending_close = false;       // Previous effective_close state

  // BCCM_PMZ_A (0x18B) 50ms. Byte 0: bit0=alive, bit1=contactor demand; bit2=1 (0x07) when close. Byte 1: precharge request.
  // Set dynamically in transmit_can() based on userRequestContactorClose.
  CAN_frame VELAR_18B = {.FD = false,
                         .ext_ID = false,
                         .DLC = 8,
                         .ID = 0x18B,
                         .data = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};  // default: alive only, no demand

  // PCM_PMZ_HVBatt (0xA2, 20ms DBC). DBC signals: PNChargingFunctionReq(56)=0, PwrSupWakeUpAllowed(60:2)=1,
  // PNFuelRefill(48)=0, HVBattContactorRequest(53), HybridMode(43:4)=0. Byte 7 low nibble = rolling counter.
  CAN_frame VELAR_0xA2_PCM_HVBatt = {.FD = false,
                                      .ext_ID = false,
                                      .DLC = 8,
                                      .ID = 0xA2,
                                      .data = {0x72, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};  // default: open

  // BCCM_PMZ_DCDCOperatingMode (0x96/150, 20ms). ChargerHVILStatus at bit 33 = HVIL OK (0). BCCM normally sends; we emulate when no charger.
  CAN_frame VELAR_0x96_BCCM_DCDC = {.FD = false,
                                    .ext_ID = false,
                                    .DLC = 8,
                                    .ID = 0x96,
                                    .data = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

  // Inverter HVIL status (0xA4, 20ms cyclic). EPIC normally sends this; emulator sends it when no inverter.
  // DBC: InverterHVILStatus at start bit 36 (byte 4 bits 5:4), 2 bits, Motorola, values 0–3.
  // Value 2 = OK/Closed (safe). We send 2 so BMS allows contactors.
  CAN_frame VELAR_0xA4_InverterHVIL = {.FD = false,
                                        .ext_ID = false,
                                        .DLC = 8,
                                        .ID = 0xA4,
                                        .data = {0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00}};  // byte 4 = 0x20 → InverterHVILStatus=2 (OK)

  // Vehicle presence frames (may be required for BMS to allow contactor close). Same IDs as Range Rover PHEV.
  // Payloads are minimal defaults; tune from vehicle capture if contactors still refuse to close.
  CAN_frame VELAR_0x008_GWM = {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x008,
                              .data = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};   // GWM_PMZ_A 10ms
  // GWM_PMZ_V_HYBRID. Byte 0: 0x01 may = "Require Charging Function"; try 0x00 for "Do Not Require" (test).
  CAN_frame VELAR_0x18d_GWM = {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x18d,
                               .data = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};  // byte 0=0: reduce Require Charging Function
  CAN_frame VELAR_0x224_BCCMB = {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x224,
                                  .data = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};  // BCCMB_PMZ_A 90ms
};

#endif

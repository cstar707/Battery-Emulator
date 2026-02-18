#ifndef RUIXU_BATTERY_H
#define RUIXU_BATTERY_H

#include "../datalayer/datalayer.h"
#include "CanBattery.h"

/**
 * Ruixu (e.g. Lithi2-16) battery over CAN bus.
 * Ruixu supports closed-loop communication with inverters via CAN and RS485.
 * CAN message IDs and byte layout must be filled in from Ruixu BMS protocol
 * documentation (e.g. Operation and Maintenance Manual or Ruixu support).
 */
class RuixuBattery : public CanBattery {
 public:
  RuixuBattery()
      : CanBattery(CAN_Speed::CAN_SPEED_500KBPS) {
    datalayer_battery = &datalayer.battery;
  }

  virtual void setup(void) override;
  virtual void handle_incoming_can_frame(CAN_frame rx_frame) override;
  virtual void update_values() override;
  virtual void transmit_can(unsigned long currentMillis) override;

  static constexpr const char* Name = "Ruixu (CAN)";

 private:
  DATALAYER_BATTERY_TYPE* datalayer_battery;

  uint16_t voltage_dV = 0;
  int16_t current_dA = 0;
  uint16_t soc_pptt = 5000;   // 50.00%
  uint16_t soh_pptt = 10000;  // 100.00%
  uint16_t cell_max_mV = 3300;
  uint16_t cell_min_mV = 3300;
  int16_t temperature_max_dC = 250;  // 25.0 °C
  int16_t temperature_min_dC = 250;
  int16_t max_charge_current_dA = 0;
  int16_t max_discharge_current_dA = 0;
};

#endif

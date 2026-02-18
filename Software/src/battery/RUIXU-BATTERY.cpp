/**
 * Ruixu battery over CAN bus (e.g. Lithi2-16).
 * Fill in CAN IDs and byte mapping from Ruixu BMS protocol documentation.
 * Ruixu supports CAN and RS485; this driver is for CAN only.
 */

#include "RUIXU-BATTERY.h"
#include <cstring>
#include "../battery/BATTERIES.h"
#include "../communication/can/comm_can.h"
#include "../datalayer/datalayer.h"
#include "../devboard/utils/events.h"

void RuixuBattery::setup(void) {
  strncpy(datalayer.system.info.battery_protocol, Name, 63);
  datalayer.system.info.battery_protocol[63] = '\0';
  // CAN speed set in constructor (500 kbps typical for stationary BMS)
}

void RuixuBattery::handle_incoming_can_frame(CAN_frame rx_frame) {
  // TODO: Add Ruixu CAN message IDs from official protocol document.
  // Ruixu Lithi2-16 / BMS may use standard BMS CAN IDs (e.g. Victron-style 0x351, 0x355, 0x356)
  // or a proprietary layout. Update the switch below once you have the spec.
  (void)rx_frame;

  switch (rx_frame.ID) {
    // Placeholder: replace with actual Ruixu CAN IDs when documented.
    // Example (do not assume these are correct for Ruixu):
    // case 0x351:  // Limits / voltage
    //   datalayer_battery->status.CAN_battery_still_alive = CAN_STILL_ALIVE;
    //   break;
    // case 0x355:  // SOC / SOH
    //   soc_pptt = ...;
    //   break;
    // case 0x356:  // Voltage, current, temperature
    //   voltage_dV = ...;
    //   current_dA = ...;
    //   break;
    default:
      break;
  }
}

void RuixuBattery::update_values() {
  datalayer_battery->status.real_soc = soc_pptt;
  datalayer_battery->status.reported_soc = soc_pptt;
  datalayer_battery->status.soh_pptt = soh_pptt;
  datalayer_battery->status.voltage_dV = voltage_dV;
  datalayer_battery->status.current_dA = current_dA;
  datalayer_battery->status.reported_current_dA = current_dA;
  datalayer_battery->status.cell_max_voltage_mV = cell_max_mV;
  datalayer_battery->status.cell_min_voltage_mV = cell_min_mV;
  datalayer_battery->status.temperature_max_dC = temperature_max_dC;
  datalayer_battery->status.temperature_min_dC = temperature_min_dC;

  datalayer_battery->status.remaining_capacity_Wh = static_cast<uint32_t>(
      (static_cast<double>(soc_pptt) / 10000.0) * datalayer_battery->info.total_capacity_Wh);
  datalayer_battery->status.reported_remaining_capacity_Wh = datalayer_battery->status.remaining_capacity_Wh;

  if (voltage_dV > 0) {
    datalayer_battery->status.max_charge_power_W =
        (uint32_t)((max_charge_current_dA / 10) * (voltage_dV / 10));
    datalayer_battery->status.max_discharge_power_W =
        (uint32_t)((max_discharge_current_dA / 10) * (voltage_dV / 10));
  }
}

void RuixuBattery::transmit_can(unsigned long currentMillis) {
  (void)currentMillis;
  // Ruixu BMS may be passive (battery sends, we only receive). If the protocol
  // requires requests or keep-alive from the inverter/emulator side, add
  // the appropriate CAN frames here once the protocol is documented.
}

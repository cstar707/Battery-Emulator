#ifndef MQTT_DISPLAY_BRIDGE_H
#define MQTT_DISPLAY_BRIDGE_H

#include <stdint.h>

// Solar/inverter data used by the display-only solar tab
struct SolarData {
  // Solark inverter data
  float solark_pv_power_W = 0.0f;
  float solark_load_power_W = 0.0f;
  float solark_grid_power_W = 0.0f;
  float solark_battery_power_W = 0.0f;
  float solark_battery_voltage_V = 0.0f;
  float solark_battery_temp_C = 0.0f;
  float solark_total_battery_current_A = 0.0f;
  float solark_battery_soc_pct = 0.0f;
  float solark_day_pv_energy_kWh = 0.0f;
  unsigned long solark_last_update_ms = 0;
  
  // Solis inverter-side data (battery-side truth comes from BE/info via datalayer)
  float solis_pv_power_W = 0.0f;
  float solis_load_power_W = 0.0f;
  float solis_grid_power_W = 0.0f;
  float solis_battery_power_W = 0.0f;
  float solis_battery_soc_pct = 0.0f;
  float solis_day_pv_energy_kWh = 0.0f;
  unsigned long solis_last_update_ms = 0;
  
  // Envoy summary data (pre-computed by server)
  float envoy_total_live_W = 0.0f;
  float envoy_total_today_kWh = 0.0f;
  float envoy_house_today_kWh = 0.0f;
  float envoy_shed_today_kWh = 0.0f;
  float envoy_trailer_today_kWh = 0.0f;
  unsigned long envoy_last_update_ms = 0;

  // Overall-system summary data following the grid-status total semantics
  float root_day_pv_energy_kWh = 0.0f;
  float total_day_pv_energy_kWh = 0.0f;
  unsigned long total_day_pv_last_update_ms = 0;

  // Solar bottom control-card state restored from the approved 3007 contract
  bool solis_mode_self_use = false;
  bool solis_mode_feed_in_priority = false;
  unsigned long solis_mode_last_update_ms = 0;

  bool tabuchi_export_enabled = false;
  bool shed_micros_enabled = false;
  bool iq8_micros_enabled = false;
  bool curtail_auto_enabled = false;
  bool curtail_auto_active = false;
  bool curtail_solis_active = false;
  float curtail_soc_pct = 0.0f;
  float curtail_threshold_pct = 0.0f;
  float curtail_restore_pct = 0.0f;
  unsigned long curtailment_last_update_ms = 0;
  
  // Legacy compatibility fields (mapped to Solark data)
  float pv_power_W = 0.0f;
  float load_power_W = 0.0f;
  float grid_power_W = 0.0f;     // positive = import, negative = export
  float battery_power_W = 0.0f;  // positive = charging, negative = discharging
  float battery_soc_pct = 0.0f;
  float day_pv_energy_kWh = 0.0f;
  float day_batt_charge_kWh = 0.0f;
  float day_batt_discharge_kWh = 0.0f;
  unsigned long last_update_ms = 0;  // millis() of last received update, 0 = never
};

// MQTT message log entry (repurposed from CAN log viewer)
struct MqttLogEntry {
  char topic[64];
  char payload[128];
  unsigned long timestamp_ms;
};

#define MQTT_LOG_SIZE 50

#ifdef HW_WAVESHARE7B_DISPLAY_ONLY

namespace mqtt_display_bridge {

// Called from mqtt.cpp on MQTT_EVENT_DATA for display-only build
void on_mqtt_message(const char* topic, int topic_len, const char* data, int data_len);

// Called from Software.cpp subscribe() to register the topics we care about
void subscribe_topics(void* mqtt_client_handle);

// Called every 1s from the minimal core loop to age out the MQTT-alive counter
void tick_alive_counter();

// Solar data accessor — used by display.cpp Solar tab
const SolarData& get_solar_data();

// Approved 3007 action helpers for the live Solar control cards
bool set_solis_mode_self_use();
bool set_solis_mode_feed_in_priority();
bool set_tabuchi_export_enabled(bool enabled);
bool set_shed_micros_enabled(bool enabled);
bool set_iq8_micros_enabled(bool enabled);

// MQTT message log accessor — used by webserver /canlog repurposed endpoint
const MqttLogEntry* get_mqtt_log();
int get_mqtt_log_count();

}  // namespace mqtt_display_bridge

#endif  // HW_WAVESHARE7B_DISPLAY_ONLY
#endif  // MQTT_DISPLAY_BRIDGE_H

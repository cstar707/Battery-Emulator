#ifndef MQTT_DISPLAY_BRIDGE_H
#define MQTT_DISPLAY_BRIDGE_H

#include <stdint.h>

// Solar/inverter data received from ESPHome solar/solark/sensors/#
struct SolarData {
  // Solark inverter data
  float solark_pv_power_W = 0.0f;
  float solark_load_power_W = 0.0f;
  float solark_grid_power_W = 0.0f;
  float solark_battery_power_W = 0.0f;
  float solark_battery_soc_pct = 0.0f;
  float solark_day_pv_energy_kWh = 0.0f;
  unsigned long solark_last_update_ms = 0;
  
  // Solis inverter data
  float solis_pv_power_W = 0.0f;
  float solis_load_power_W = 0.0f;
  float solis_grid_power_W = 0.0f;
  float solis_battery_power_W = 0.0f;
  float solis_battery_soc_pct = 0.0f;
  float solis_day_pv_energy_kWh = 0.0f;
  unsigned long solis_last_update_ms = 0;
  
  // Envoy device data
  float envoy1_active_power_W = 0.0f;
  unsigned long envoy1_last_update_ms = 0;
  float envoy2_active_power_W = 0.0f;
  unsigned long envoy2_last_update_ms = 0;
  
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

// MQTT message log accessor — used by webserver /canlog repurposed endpoint
const MqttLogEntry* get_mqtt_log();
int get_mqtt_log_count();

}  // namespace mqtt_display_bridge

#endif  // HW_WAVESHARE7B_DISPLAY_ONLY
#endif  // MQTT_DISPLAY_BRIDGE_H

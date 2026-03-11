#ifdef HW_WAVESHARE7B_DISPLAY_ONLY

#include "mqtt_display_bridge.h"
#include <Arduino.h>
#include <string.h>
#include "../../datalayer/datalayer.h"
#include "../../devboard/utils/types.h"
#include "../../lib/bblanchon-ArduinoJson/ArduinoJson.h"
#include "mqtt_client.h"

// Subscribed topics
#define TOPIC_BE_INFO "BE/info"
#define TOPIC_BE_SPEC_DATA "BE/spec_data"
#define TOPIC_BE_EVENTS "BE/events"
#define TOPIC_SOLARK_PREFIX "solar/solark/sensors/"
#define TOPIC_SOLIS_PREFIX "solar/solis/sensors/"
#define TOPIC_ENVOY1 "envoy/1/active_power"
#define TOPIC_ENVOY2 "envoy/2/active_power"

namespace mqtt_display_bridge {

static SolarData solar_data;
static TeslaSummaryData tesla_summary;
static MqttLogEntry mqtt_log[MQTT_LOG_SIZE];
static int mqtt_log_head = 0;
static int mqtt_log_count = 0;

// ── Helpers ──────────────────────────────────────────────────────────────────

static void log_message(const char* topic, const char* payload) {
  MqttLogEntry& entry = mqtt_log[mqtt_log_head];
  strncpy(entry.topic, topic, sizeof(entry.topic) - 1);
  entry.topic[sizeof(entry.topic) - 1] = '\0';
  strncpy(entry.payload, payload, sizeof(entry.payload) - 1);
  entry.payload[sizeof(entry.payload) - 1] = '\0';
  entry.timestamp_ms = millis();
  mqtt_log_head = (mqtt_log_head + 1) % MQTT_LOG_SIZE;
  if (mqtt_log_count < MQTT_LOG_SIZE)
    mqtt_log_count++;
}

static bms_status_enum parse_bms_status(const char* s) {
  if (s == nullptr)
    return ACTIVE;
  if (strncmp(s, "FAULT", 5) == 0)
    return FAULT;
  if (strncmp(s, "STANDBY", 7) == 0)
    return STANDBY;
  return ACTIVE;
}

static const char* parse_tesla_contactor_state(uint8_t code) {
  switch (code) {
    case 1:
      return "OPEN";
    case 2:
      return "OPENING";
    case 3:
      return "CLOSING";
    case 4:
      return "CLOSED";
    case 5:
      return "WELDED";
    case 6:
      return "BLOCKED";
    default:
      return "UNKNOWN";
  }
}

// ── BE/info handler ───────────────────────────────────────────────────────────

static void handle_be_info(const char* data, int data_len) {
  static JsonDocument doc;
  char buf[2048];
  int copy_len = (data_len < (int)(sizeof(buf) - 1)) ? data_len : (int)(sizeof(buf) - 1);
  memcpy(buf, data, copy_len);
  buf[copy_len] = '\0';

  DeserializationError err = deserializeJson(doc, buf);
  if (err)
    return;

  // SOC — published as float percent (e.g. 85.5), stored as pptt (8550)
  if (doc["SOC"].is<float>()) {
    datalayer.battery.status.reported_soc = (uint16_t)(doc["SOC"].as<float>() * 100.0f);
  }
  if (doc["SOC_real"].is<float>()) {
    datalayer.battery.status.real_soc = (uint16_t)(doc["SOC_real"].as<float>() * 100.0f);
  }

  // State of health — published as float percent
  if (doc["state_of_health"].is<float>()) {
    datalayer.battery.status.soh_pptt = (uint16_t)(doc["state_of_health"].as<float>() * 100.0f);
  }

  // Temperatures — published as float °C, stored as d°C
  if (doc["temperature_min"].is<float>()) {
    datalayer.battery.status.temperature_min_dC = (int16_t)(doc["temperature_min"].as<float>() * 10.0f);
  }
  if (doc["temperature_max"].is<float>()) {
    datalayer.battery.status.temperature_max_dC = (int16_t)(doc["temperature_max"].as<float>() * 10.0f);
  }

  // Power / current / voltage
  if (doc["stat_batt_power"].is<float>()) {
    datalayer.battery.status.active_power_W = (int32_t)doc["stat_batt_power"].as<float>();
  }
  if (doc["battery_current"].is<float>()) {
    datalayer.battery.status.current_dA = (int16_t)(doc["battery_current"].as<float>() * 10.0f);
  }
  if (doc["battery_voltage"].is<float>()) {
    datalayer.battery.status.voltage_dV = (uint16_t)(doc["battery_voltage"].as<float>() * 10.0f);
  }

  // Cell voltages summary — published as float V, stored as mV
  if (doc["cell_max_voltage"].is<float>()) {
    datalayer.battery.status.cell_max_voltage_mV = (uint16_t)(doc["cell_max_voltage"].as<float>() * 1000.0f);
  }
  if (doc["cell_min_voltage"].is<float>()) {
    datalayer.battery.status.cell_min_voltage_mV = (uint16_t)(doc["cell_min_voltage"].as<float>() * 1000.0f);
  }

  // Capacity — published as float Wh
  if (doc["total_capacity"].is<float>()) {
    datalayer.battery.info.total_capacity_Wh = (uint32_t)doc["total_capacity"].as<float>();
  }
  if (doc["remaining_capacity"].is<float>()) {
    datalayer.battery.status.reported_remaining_capacity_Wh = (uint32_t)doc["remaining_capacity"].as<float>();
  }
  if (doc["remaining_capacity_real"].is<float>()) {
    datalayer.battery.status.remaining_capacity_Wh = (uint32_t)doc["remaining_capacity_real"].as<float>();
  }

  // Charge/discharge power limits
  if (doc["max_discharge_power"].is<float>()) {
    datalayer.battery.status.max_discharge_power_W = (uint32_t)doc["max_discharge_power"].as<float>();
  }
  if (doc["max_charge_power"].is<float>()) {
    datalayer.battery.status.max_charge_power_W = (uint32_t)doc["max_charge_power"].as<float>();
  }

  // BMS status string → enum
  if (doc["bms_status"].is<const char*>()) {
    datalayer.battery.status.bms_status = parse_bms_status(doc["bms_status"].as<const char*>());
  }

  // Tesla-specific summary fields published on BE/info for display-only rendering
  tesla_summary.has_contactor_state = false;
  tesla_summary.contactor_state[0] = '\0';
  tesla_summary.contactor_state_code = 0;
  bool has_contactor_state_code = false;
  if (doc["contactor_state_code"].is<int>()) {
    tesla_summary.contactor_state_code = (uint8_t)doc["contactor_state_code"].as<int>();
    has_contactor_state_code = true;
  }
  if (doc["contactor_state"].is<const char*>()) {
    strncpy(tesla_summary.contactor_state, doc["contactor_state"].as<const char*>(), sizeof(tesla_summary.contactor_state) - 1);
    tesla_summary.contactor_state[sizeof(tesla_summary.contactor_state) - 1] = '\0';
    tesla_summary.has_contactor_state = true;
  } else if (has_contactor_state_code) {
    strncpy(tesla_summary.contactor_state, parse_tesla_contactor_state(tesla_summary.contactor_state_code),
            sizeof(tesla_summary.contactor_state) - 1);
    tesla_summary.contactor_state[sizeof(tesla_summary.contactor_state) - 1] = '\0';
    tesla_summary.has_contactor_state = true;
  }

  tesla_summary.has_battery_12v_voltage = false;
  tesla_summary.battery_12v_voltage_V = 0.0f;
  if (doc["battery_12v_voltage"].is<float>()) {
    tesla_summary.battery_12v_voltage_V = doc["battery_12v_voltage"].as<float>();
    tesla_summary.has_battery_12v_voltage = true;
  }

  tesla_summary.has_battery_12v_current = false;
  tesla_summary.battery_12v_current_A = 0.0f;
  if (doc["battery_12v_current"].is<float>()) {
    tesla_summary.battery_12v_current_A = doc["battery_12v_current"].as<float>();
    tesla_summary.has_battery_12v_current = true;
  }

  // Mark battery data as fresh — same mechanism as CAN-alive counter
  datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;

  doc.clear();
}

// ── BE/spec_data handler ──────────────────────────────────────────────────────

static void handle_be_spec_data(const char* data, int data_len) {
  static JsonDocument doc;
  char buf[4096];
  int copy_len = (data_len < (int)(sizeof(buf) - 1)) ? data_len : (int)(sizeof(buf) - 1);
  memcpy(buf, data, copy_len);
  buf[copy_len] = '\0';

  DeserializationError err = deserializeJson(doc, buf);
  if (err)
    return;

  JsonArray arr = doc["cell_voltages"].as<JsonArray>();
  if (arr.isNull())
    return;

  uint8_t count = 0;
  for (JsonVariant v : arr) {
    if (count >= MAX_AMOUNT_CELLS)
      break;
    // Published as float V, stored as mV
    datalayer.battery.status.cell_voltages_mV[count] = (uint16_t)(v.as<float>() * 1000.0f);
    count++;
  }
  datalayer.battery.info.number_of_cells = count;

  doc.clear();
}

// ── BE/events handler ─────────────────────────────────────────────────────────

static void handle_be_events(const char* data, int data_len) {
  // Events are logged to the MQTT log for the /canlog web page.
  // The display Alerts tab uses its own threshold-based alert logic fed from datalayer,
  // so no extra work needed here — the datalayer values updated by handle_be_info() drive it.
  (void)data;
  (void)data_len;
}

// ── solar/solark/sensors/<suffix> handler ────────────────────────────────────

static void handle_solark_sensor(const char* suffix, const char* payload, int payload_len) {
  char val_buf[32];
  int copy_len = (payload_len < (int)(sizeof(val_buf) - 1)) ? payload_len : (int)(sizeof(val_buf) - 1);
  memcpy(val_buf, payload, copy_len);
  val_buf[copy_len] = '\0';

  float val = atof(val_buf);

  if (strcmp(suffix, "solar_power") == 0) {
    solar_data.solark_pv_power_W = val;
    solar_data.pv_power_W = val;  // Legacy compatibility
  } else if (strcmp(suffix, "total_solar_power") == 0) {
    solar_data.solark_pv_power_W = val;
    solar_data.pv_power_W = val;  // prefer total if available
  } else if (strcmp(suffix, "load_power") == 0) {
    solar_data.solark_load_power_W = val;
    solar_data.load_power_W = val;
  } else if (strcmp(suffix, "grid_power") == 0) {
    solar_data.solark_grid_power_W = val;
    solar_data.grid_power_W = val;
  } else if (strcmp(suffix, "battery_power") == 0 || strcmp(suffix, "total_battery_power") == 0) {
    solar_data.solark_battery_power_W = val;
    solar_data.battery_power_W = val;
  } else if (strcmp(suffix, "battery_soc") == 0) {
    solar_data.solark_battery_soc_pct = val;
    solar_data.battery_soc_pct = val;
  } else if (strcmp(suffix, "day_pv_energy") == 0) {
    solar_data.solark_day_pv_energy_kWh = val;
    solar_data.day_pv_energy_kWh = val;
  } else if (strcmp(suffix, "day_battery_charge") == 0) {
    solar_data.day_batt_charge_kWh = val;
  } else if (strcmp(suffix, "day_battery_discharge") == 0) {
    solar_data.day_batt_discharge_kWh = val;
  }

  solar_data.solark_last_update_ms = millis();
  solar_data.last_update_ms = millis();
}

// ── solar/solis/sensors/<suffix> handler ──────────────────────────────────────

static void handle_solis_sensor(const char* suffix, const char* payload, int payload_len) {
  char val_buf[32];
  int copy_len = (payload_len < (int)(sizeof(val_buf) - 1)) ? payload_len : (int)(sizeof(val_buf) - 1);
  memcpy(val_buf, payload, copy_len);
  val_buf[copy_len] = '\0';

  float val = atof(val_buf);

  if (strcmp(suffix, "solar_power") == 0) {
    solar_data.solis_pv_power_W = val;
  } else if (strcmp(suffix, "total_solar_power") == 0) {
    solar_data.solis_pv_power_W = val;
  } else if (strcmp(suffix, "load_power") == 0) {
    solar_data.solis_load_power_W = val;
  } else if (strcmp(suffix, "grid_power") == 0) {
    solar_data.solis_grid_power_W = val;
  } else if (strcmp(suffix, "battery_power") == 0 || strcmp(suffix, "total_battery_power") == 0) {
    solar_data.solis_battery_power_W = val;
  } else if (strcmp(suffix, "battery_soc") == 0) {
    solar_data.solis_battery_soc_pct = val;
  } else if (strcmp(suffix, "day_pv_energy") == 0) {
    solar_data.solis_day_pv_energy_kWh = val;
  }

  solar_data.solis_last_update_ms = millis();
}

// ── envoy/<id>/active_power handler ─────────────────────────────────────────

static void handle_envoy_power(const char* topic, const char* payload, int payload_len) {
  char val_buf[32];
  int copy_len = (payload_len < (int)(sizeof(val_buf) - 1)) ? payload_len : (int)(sizeof(val_buf) - 1);
  memcpy(val_buf, payload, copy_len);
  val_buf[copy_len] = '\0';

  float val = atof(val_buf);

  if (strcmp(topic, TOPIC_ENVOY1) == 0) {
    solar_data.envoy1_active_power_W = val;
    solar_data.envoy1_last_update_ms = millis();
  } else if (strcmp(topic, TOPIC_ENVOY2) == 0) {
    solar_data.envoy2_active_power_W = val;
    solar_data.envoy2_last_update_ms = millis();
  }
}

// ── Public API ────────────────────────────────────────────────────────────────

void on_mqtt_message(const char* topic_raw, int topic_len, const char* data, int data_len) {
  char topic[128];
  int copy_len = (topic_len < (int)(sizeof(topic) - 1)) ? topic_len : (int)(sizeof(topic) - 1);
  memcpy(topic, topic_raw, copy_len);
  topic[copy_len] = '\0';

  // Log every incoming message for the /canlog web viewer
  char payload_preview[128];
  int prev_len = (data_len < (int)(sizeof(payload_preview) - 1)) ? data_len : (int)(sizeof(payload_preview) - 1);
  memcpy(payload_preview, data, prev_len);
  payload_preview[prev_len] = '\0';
  log_message(topic, payload_preview);

  if (strcmp(topic, TOPIC_BE_INFO) == 0) {
    handle_be_info(data, data_len);
  } else if (strcmp(topic, TOPIC_BE_SPEC_DATA) == 0) {
    handle_be_spec_data(data, data_len);
  } else if (strcmp(topic, TOPIC_BE_EVENTS) == 0) {
    handle_be_events(data, data_len);
  } else if (strncmp(topic, TOPIC_SOLARK_PREFIX, strlen(TOPIC_SOLARK_PREFIX)) == 0) {
    const char* suffix = topic + strlen(TOPIC_SOLARK_PREFIX);
    handle_solark_sensor(suffix, data, data_len);
  } else if (strncmp(topic, TOPIC_SOLIS_PREFIX, strlen(TOPIC_SOLIS_PREFIX)) == 0) {
    const char* suffix = topic + strlen(TOPIC_SOLIS_PREFIX);
    handle_solis_sensor(suffix, data, data_len);
  } else if (strcmp(topic, TOPIC_ENVOY1) == 0 || strcmp(topic, TOPIC_ENVOY2) == 0) {
    handle_envoy_power(topic, data, data_len);
  }
}

void subscribe_topics(void* client_handle) {
  esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t)client_handle;
  esp_mqtt_client_subscribe(client, TOPIC_BE_INFO, 0);
  esp_mqtt_client_subscribe(client, TOPIC_BE_SPEC_DATA, 0);
  esp_mqtt_client_subscribe(client, TOPIC_BE_EVENTS, 0);
  esp_mqtt_client_subscribe(client, "solar/solark/sensors/#", 0);
  esp_mqtt_client_subscribe(client, "solar/solis/sensors/#", 0);
  esp_mqtt_client_subscribe(client, TOPIC_ENVOY1, 0);
  esp_mqtt_client_subscribe(client, TOPIC_ENVOY2, 0);
}

void tick_alive_counter() {
  // Mirrors the CAN-alive decrement that normally happens in battery->update_values().
  // Goes to 0 if no BE/info MQTT messages arrive — display then shows "No data" state.
  if (datalayer.battery.status.CAN_battery_still_alive > 0) {
    datalayer.battery.status.CAN_battery_still_alive--;
  }
}

const SolarData& get_solar_data() {
  return solar_data;
}

const TeslaSummaryData& get_tesla_summary() {
  return tesla_summary;
}

const MqttLogEntry* get_mqtt_log() {
  return mqtt_log;
}

int get_mqtt_log_count() {
  return mqtt_log_count;
}

}  // namespace mqtt_display_bridge

#endif  // HW_WAVESHARE7B_DISPLAY_ONLY

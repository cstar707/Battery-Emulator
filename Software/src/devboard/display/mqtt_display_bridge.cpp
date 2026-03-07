#ifdef HW_WAVESHARE7B_DISPLAY_ONLY

#include "mqtt_display_bridge.h"
#include <Arduino.h>
#include <string.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include "../../datalayer/datalayer.h"
#include "../../devboard/utils/types.h"
#include "../../lib/bblanchon-ArduinoJson/ArduinoJson.h"
#include "mqtt_client.h"
#include "esp_log.h"

// Subscribed topics
#define TOPIC_BE_INFO "BE/info"
#define TOPIC_BE_SPEC_DATA "BE/spec_data"
#define TOPIC_BE_EVENTS "BE/events"
#define TOPIC_SOLARK_PREFIX "solar/solark/sensors/"
#define TOPIC_SOLIS_PREFIX "solis/sensors/"

// Envoy HTTP polling
#define ENVOY_API_URL "http://10.10.53.92:3008/api/envoy/data"
#define ENVOY_POLL_INTERVAL_MS 60000UL

// House inverter serial numbers (envoy1)
static const char* HOUSE_SERIALS[] = { "121138031474", "121138032402", "121138031483", nullptr };

namespace mqtt_display_bridge {

static SolarData solar_data;
static MqttLogEntry mqtt_log[MQTT_LOG_SIZE];
static int mqtt_log_head = 0;
static int mqtt_log_count = 0;
static unsigned long envoy_last_poll_ms = 0;

// ── Envoy HTTP poll ───────────────────────────────────────────────────────────

static bool is_house_serial(const char* serial) {
  for (int i = 0; HOUSE_SERIALS[i] != nullptr; i++) {
    if (strcmp(serial, HOUSE_SERIALS[i]) == 0) return true;
  }
  return false;
}

static void fetch_envoy_http() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(ENVOY_API_URL);
  http.setTimeout(8000);
  int code = http.GET();
  if (code == 200) {
    String body = http.getString();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (!err && doc["ok"].as<bool>()) {
      float total_live = 0, total_today = 0;
      float house_today = 0, shed_today = 0, trailer_today = 0;
      for (JsonObject inv : doc["inverters"].as<JsonArray>()) {
        const char* serial   = inv["serial"]   | "";
        const char* envoy_id = inv["envoy_id"] | "";
        float watts = inv["watts"]     | 0.0f;
        float daily = inv["daily_kwh"] | 0.0f;
        total_live  += watts;
        total_today += daily;
        if (strcmp(envoy_id, "envoy1") == 0) {
          if (is_house_serial(serial)) house_today += daily;
          else                         shed_today  += daily;
        } else {
          trailer_today += daily;
        }
      }
      solar_data.envoy_total_live_W      = total_live;
      solar_data.envoy_total_today_kWh   = total_today;
      solar_data.envoy_house_today_kWh   = house_today;
      solar_data.envoy_shed_today_kWh    = shed_today;
      solar_data.envoy_trailer_today_kWh = trailer_today;
      solar_data.envoy_last_update_ms    = millis();
      Serial.printf("[ENVOY] live=%.0fW today=%.2f house=%.2f shed=%.2f trailer=%.2f kWh\n",
        total_live, total_today, house_today, shed_today, trailer_today);
    }
  } else {
    Serial.printf("[ENVOY] HTTP GET failed: %d\n", code);
  }
  http.end();
}

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

  if (strcmp(suffix, "pv_power_W") == 0 || strcmp(suffix, "solar_power") == 0 || strcmp(suffix, "total_solar_power") == 0) {
    solar_data.solis_pv_power_W = val;
  } else if (strcmp(suffix, "load_power_W") == 0 || strcmp(suffix, "load_power") == 0) {
    solar_data.solis_load_power_W = val;
  } else if (strcmp(suffix, "grid_power_W") == 0 || strcmp(suffix, "grid_power") == 0) {
    solar_data.solis_grid_power_W = val;
  } else if (strcmp(suffix, "battery_power_W") == 0 || strcmp(suffix, "battery_power") == 0 || strcmp(suffix, "total_battery_power") == 0) {
    solar_data.solis_battery_power_W = val;
  } else if (strcmp(suffix, "battery_soc_pct") == 0 || strcmp(suffix, "battery_soc") == 0) {
    solar_data.solis_battery_soc_pct = val;
  } else if (strcmp(suffix, "energy_today_pv_kWh") == 0 || strcmp(suffix, "day_pv_energy") == 0) {
    solar_data.solis_day_pv_energy_kWh = val;
  }

  solar_data.solis_last_update_ms = millis();
}

// ── envoy/summary/* handler (simple numeric values from server) ─────────────

static void handle_envoy_summary(const char* topic, const char* payload, int payload_len) {
  char val_buf[32];
  int copy_len = (payload_len < (int)(sizeof(val_buf) - 1)) ? payload_len : (int)(sizeof(val_buf) - 1);
  memcpy(val_buf, payload, copy_len);
  val_buf[copy_len] = '\0';
  float val = atof(val_buf);

  const char* suffix = topic + strlen("envoy/summary/");
  if (strcmp(suffix, "total_live") == 0) {
    solar_data.envoy_total_live_W = val;
  } else if (strcmp(suffix, "total_today") == 0) {
    solar_data.envoy_total_today_kWh = val;
  } else if (strcmp(suffix, "house_today") == 0) {
    solar_data.envoy_house_today_kWh = val;
  } else if (strcmp(suffix, "shed_today") == 0) {
    solar_data.envoy_shed_today_kWh = val;
  } else if (strcmp(suffix, "trailer_today") == 0) {
    solar_data.envoy_trailer_today_kWh = val;
  }
  solar_data.envoy_last_update_ms = millis();
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
  } else if (strncmp(topic, "envoy/summary/", 14) == 0) {
    handle_envoy_summary(topic, data, data_len);
  }
}

void subscribe_topics(void* client_handle) {
  esp_log_level_set("mqtt_client", ESP_LOG_WARN);
  esp_log_level_set("transport", ESP_LOG_WARN);
  esp_log_level_set("transport_base", ESP_LOG_WARN);
  esp_log_level_set("transport_ssl", ESP_LOG_WARN);
  esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t)client_handle;
  esp_mqtt_client_subscribe(client, TOPIC_BE_INFO, 0);
  esp_mqtt_client_subscribe(client, TOPIC_BE_SPEC_DATA, 0);
  esp_mqtt_client_subscribe(client, TOPIC_BE_EVENTS, 0);
  esp_mqtt_client_subscribe(client, "solar/solark/sensors/solar_power", 0);
  esp_mqtt_client_subscribe(client, "solar/solark/sensors/load_power", 0);
  esp_mqtt_client_subscribe(client, "solar/solark/sensors/grid_power", 0);
  esp_mqtt_client_subscribe(client, "solar/solark/sensors/battery_power", 0);
  esp_mqtt_client_subscribe(client, "solar/solark/sensors/battery_soc", 0);
  esp_mqtt_client_subscribe(client, "solar/solark/sensors/day_pv_energy", 0);
  esp_mqtt_client_subscribe(client, "solis/sensors/pv_power_W", 0);
  esp_mqtt_client_subscribe(client, "solis/sensors/load_power_W", 0);
  esp_mqtt_client_subscribe(client, "solis/sensors/grid_power_W", 0);
  esp_mqtt_client_subscribe(client, "solis/sensors/battery_power_W", 0);
  esp_mqtt_client_subscribe(client, "solis/sensors/battery_soc_pct", 0);
  esp_mqtt_client_subscribe(client, "solis/sensors/energy_today_pv_kWh", 0);
  Serial.println("[MQTT-BRIDGE] Subscribed to 15 topics; envoy via HTTP poll");
}

void tick_alive_counter() {
  if (datalayer.battery.status.CAN_battery_still_alive > 0) {
    datalayer.battery.status.CAN_battery_still_alive--;
  }
  // Poll envoy HTTP API every 60s (staggered: first poll at 10s after boot)
  unsigned long now = millis();
  if (envoy_last_poll_ms == 0) {
    if (now >= 10000UL) {
      envoy_last_poll_ms = now;
      fetch_envoy_http();
    }
  } else if ((now - envoy_last_poll_ms) >= ENVOY_POLL_INTERVAL_MS) {
    envoy_last_poll_ms = now;
    fetch_envoy_http();
  }
}

const SolarData& get_solar_data() {
  return solar_data;
}

const MqttLogEntry* get_mqtt_log() {
  return mqtt_log;
}

int get_mqtt_log_count() {
  return mqtt_log_count;
}

}  // namespace mqtt_display_bridge

#endif  // HW_WAVESHARE7B_DISPLAY_ONLY

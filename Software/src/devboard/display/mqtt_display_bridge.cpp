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
#define TOPIC_SOLIS_PREFIX "solar/solis/sensors/"

// Remote HTTP polling
#define ROOT_SENSORS_API_URL "http://10.10.53.92:3008/api/sensors/latest"
#define ROOT_SENSORS_POLL_INTERVAL_MS 5000UL
#define ENVOY_API_URL "http://10.10.53.92:3008/api/envoy/data"
#define ENVOY_POLL_INTERVAL_MS 60000UL
#define STORAGE_BITS_API_URL "http://10.10.53.92:3007/api/storage_bits"
#define STORAGE_BITS_POLL_INTERVAL_MS 5000UL
#define CURTAILMENT_API_URL "http://10.10.53.92:3007/api/curtailment"
#define CURTAILMENT_POLL_INTERVAL_MS 5000UL
#define SOLIS_CONTROL_API_URL "http://10.10.53.92:3007/api/control"
#define CURTAILMENT_TABUCHI_API_URL "http://10.10.53.92:3007/api/curtailment/tabuchi"
#define CURTAILMENT_MSERIES_API_URL "http://10.10.53.92:3007/api/curtailment/mseries"
#define CURTAILMENT_IQ8_API_URL "http://10.10.53.92:3007/api/curtailment/iq8"
#define CONTROL_STATUS_HTTP_TIMEOUT_MS 2000
#define REMOTE_STATUS_HTTP_TIMEOUT_MS 8000
#define ACTION_HTTP_TIMEOUT_MS 2000

// House inverter serial numbers (envoy1)
static const char* HOUSE_SERIALS[] = { "121138031474", "121138032402", "121138031483", nullptr };
static const char* SHED_FROM_ENVOY2_SERIAL = "542442025779";

namespace mqtt_display_bridge {

enum class PendingSolarAction : uint8_t {
  None = 0,
  SolisSelfUse,
  SolisFeedInPriority,
  TabuchiOn,
  TabuchiOff,
  ShedOn,
  ShedOff,
  Iq8On,
  Iq8Off,
};

static SolarData solar_data;
static TeslaSummaryData tesla_summary;
static MqttLogEntry mqtt_log[MQTT_LOG_SIZE];
static int mqtt_log_head = 0;
static int mqtt_log_count = 0;
static unsigned long root_sensors_last_poll_ms = 0;
static unsigned long envoy_last_poll_ms = 0;
static unsigned long storage_bits_last_poll_ms = 0;
static unsigned long curtailment_last_poll_ms = 0;
static volatile PendingSolarAction pending_solar_action = PendingSolarAction::None;
static constexpr float GRID_STATUS_TUBUCHI_DAY_PV_KWH = 15.0f;

static void recompute_total_day_pv_energy() {
  solar_data.total_day_pv_energy_kWh =
    solar_data.solis_day_pv_energy_kWh +
    solar_data.root_day_pv_energy_kWh +
    GRID_STATUS_TUBUCHI_DAY_PV_KWH +
    solar_data.envoy_total_today_kWh;
  solar_data.total_day_pv_last_update_ms = millis();
}

// ── Envoy HTTP poll ───────────────────────────────────────────────────────────

static bool is_house_serial(const char* serial) {
  for (int i = 0; HOUSE_SERIALS[i] != nullptr; i++) {
    if (strcmp(serial, HOUSE_SERIALS[i]) == 0) return true;
  }
  return false;
}

static bool is_shed_from_envoy2_serial(const char* serial) {
  return strcmp(serial, SHED_FROM_ENVOY2_SERIAL) == 0;
}

static bool read_sensor_value(JsonVariantConst values, const char* key, float* out) {
  JsonVariantConst sensor = values[key];
  if (sensor.isNull()) return false;
  JsonVariantConst value = sensor["value"];
  if (!value.isNull()) {
    *out = value.as<float>();
    return true;
  }
  if (sensor.is<float>() || sensor.is<int>()) {
    *out = sensor.as<float>();
    return true;
  }
  return false;
}

static bool read_bool_value(JsonVariantConst object, const char* key, bool* out) {
  JsonVariantConst value = object[key];
  if (value.isNull()) return false;
  *out = value.as<bool>();
  return true;
}

static bool read_float_value(JsonVariantConst object, const char* key, float* out) {
  JsonVariantConst value = object[key];
  if (value.isNull()) return false;
  *out = value.as<float>();
  return true;
}

static bool read_switch_enabled(JsonVariantConst switches, const char* key, bool* out) {
  JsonVariantConst state = switches[key]["state"];
  if (state.isNull()) return false;
  const char* state_str = state.as<const char*>();
  if (state_str == nullptr) return false;
  *out = strcmp(state_str, "on") == 0;
  return true;
}

static bool post_remote_json(const char* url, const char* payload) {
  if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http;
  http.begin(url);
  http.setTimeout(ACTION_HTTP_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(String(payload));
  bool ok = (code >= 200 && code < 300);
  if (!ok) {
    Serial.printf("[3007] HTTP POST failed: %s -> %d\n", url, code);
  }
  http.end();
  return ok;
}

static bool post_remote_empty(const char* url) {
  if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http;
  http.begin(url);
  http.setTimeout(ACTION_HTTP_TIMEOUT_MS);
  int code = http.POST(String(""));
  bool ok = (code >= 200 && code < 300);
  if (!ok) {
    Serial.printf("[3007] HTTP POST failed: %s -> %d\n", url, code);
  }
  http.end();
  return ok;
}

static void fetch_storage_bits_http() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(STORAGE_BITS_API_URL);
  http.setTimeout(CONTROL_STATUS_HTTP_TIMEOUT_MS);
  int code = http.GET();
  if (code == 200) {
    String body = http.getString();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (!err) {
      bool updated = false;
      bool value = false;
      if (read_bool_value(doc.as<JsonVariantConst>(), "self_use", &value)) {
        solar_data.solis_mode_self_use = value;
        updated = true;
      }
      if (read_bool_value(doc.as<JsonVariantConst>(), "feed_in_priority", &value)) {
        solar_data.solis_mode_feed_in_priority = value;
        updated = true;
      }
      if (updated) {
        solar_data.solis_mode_last_update_ms = millis();
      }
    }
  }
  http.end();
}

static void fetch_curtailment_http() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(CURTAILMENT_API_URL);
  http.setTimeout(CONTROL_STATUS_HTTP_TIMEOUT_MS);
  int code = http.GET();
  if (code == 200) {
    String body = http.getString();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (!err) {
      JsonVariantConst root = doc.as<JsonVariantConst>();
      if (!root["auth_ok"].isNull() && !root["auth_ok"].as<bool>()) {
        Serial.println("[3007] curtailment GET auth not ok");
      } else {
        bool updated = false;
        JsonVariantConst auto_state = root["auto"];
        JsonVariantConst switches = root["switches"];
        bool bool_value = false;
        float float_value = 0.0f;

        if (read_bool_value(auto_state, "enabled", &bool_value)) {
          solar_data.curtail_auto_enabled = bool_value;
          updated = true;
        }
        if (read_bool_value(auto_state, "curtail_active", &bool_value)) {
          solar_data.curtail_auto_active = bool_value;
          updated = true;
        }
        if (read_bool_value(auto_state, "solis_active", &bool_value)) {
          solar_data.curtail_solis_active = bool_value;
          updated = true;
        }
        if (read_float_value(auto_state, "soc_pct", &float_value)) {
          solar_data.curtail_soc_pct = float_value;
          updated = true;
        }
        if (read_float_value(auto_state, "threshold_pct", &float_value)) {
          solar_data.curtail_threshold_pct = float_value;
          updated = true;
        }
        if (read_float_value(auto_state, "restore_pct", &float_value)) {
          solar_data.curtail_restore_pct = float_value;
          updated = true;
        }
        if (read_switch_enabled(switches, "tabuchi", &bool_value)) {
          solar_data.tabuchi_export_enabled = bool_value;
          updated = true;
        }
        if (read_switch_enabled(switches, "mseries", &bool_value)) {
          solar_data.shed_micros_enabled = bool_value;
          updated = true;
        }
        if (read_switch_enabled(switches, "iq8", &bool_value)) {
          solar_data.iq8_micros_enabled = bool_value;
          updated = true;
        }
        if (updated) {
          solar_data.curtailment_last_update_ms = millis();
        }
      }
    }
  } else {
    Serial.printf("[3007] curtailment GET failed: %d\n", code);
  }
  http.end();
}

static void fetch_root_sensors_http() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(ROOT_SENSORS_API_URL);
  http.setTimeout(REMOTE_STATUS_HTTP_TIMEOUT_MS);
  int code = http.GET();
  if (code == 200) {
    String body = http.getString();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (!err && doc["ok"].as<bool>()) {
      JsonVariantConst values = doc["values"];
      bool updated = false;
      bool total_day_updated = false;
      float val = 0.0f;
      if (read_sensor_value(values, "tesla_pv", &val) || read_sensor_value(values, "solis_pv", &val) ||
          read_sensor_value(values, "s6_pv", &val) || read_sensor_value(values, "pv_power", &val) ||
          read_sensor_value(values, "pv", &val)) {
        solar_data.solis_pv_power_W = val;
        updated = true;
      }
      if (read_sensor_value(values, "tesla_load", &val)) {
        solar_data.solis_load_power_W = val;
        updated = true;
      }
      if (read_sensor_value(values, "tesla_grid", &val)) {
        solar_data.solis_grid_power_W = val;
        updated = true;
      }
      if (read_sensor_value(values, "tesla_today_pv", &val)) {
        solar_data.solis_day_pv_energy_kWh = val;
        total_day_updated = true;
      }
      if (read_sensor_value(values, "day_pv_energy", &val)) {
        solar_data.root_day_pv_energy_kWh = val;
        total_day_updated = true;
      }
      if (total_day_updated) {
        recompute_total_day_pv_energy();
      }
      if (updated) {
        solar_data.solis_last_update_ms = millis();
        solar_data.solis1_pv_power_W = solar_data.solis_pv_power_W;
        solar_data.solis1_load_power_W = solar_data.solis_load_power_W;
        solar_data.solis1_grid_power_W = solar_data.solis_grid_power_W;
        solar_data.solis1_battery_power_W = solar_data.solis_battery_power_W;
        solar_data.solis1_battery_soc_pct = solar_data.solis_battery_soc_pct;
        solar_data.solis1_day_pv_energy_kWh = solar_data.solis_day_pv_energy_kWh;
        solar_data.solis1_last_update_ms = solar_data.solis_last_update_ms;
        Serial.printf("[ROOT-SENSORS] tesla pv=%.0fW load=%.0fW grid=%.0fW\n",
          solar_data.solis_pv_power_W, solar_data.solis_load_power_W, solar_data.solis_grid_power_W);
      }
    }
  } else {
    Serial.printf("[ROOT-SENSORS] HTTP GET failed: %d\n", code);
  }
  http.end();
}

static void fetch_envoy_http() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(ENVOY_API_URL);
  http.setTimeout(REMOTE_STATUS_HTTP_TIMEOUT_MS);
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
        } else if (is_shed_from_envoy2_serial(serial)) {
          shed_today += daily;
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
      recompute_total_day_pv_energy();
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

static bool queue_solar_action(PendingSolarAction action) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (pending_solar_action != PendingSolarAction::None) return false;
  pending_solar_action = action;
  return true;
}

static bool process_pending_solar_action() {
  if (pending_solar_action == PendingSolarAction::None) {
    return false;
  }

  bool ok = false;
  bool needs_storage_refresh = false;
  bool needs_curtailment_refresh = false;

  switch (pending_solar_action) {
    case PendingSolarAction::SolisSelfUse:
      ok = post_remote_json(SOLIS_CONTROL_API_URL, "{\"register\":\"storage\",\"bit_index\":0,\"on\":true}");
      needs_storage_refresh = ok;
      break;
    case PendingSolarAction::SolisFeedInPriority:
      ok = post_remote_json(SOLIS_CONTROL_API_URL, "{\"register\":\"storage\",\"bit_index\":6,\"on\":true}");
      needs_storage_refresh = ok;
      break;
    case PendingSolarAction::TabuchiOn:
      ok = post_remote_empty(CURTAILMENT_TABUCHI_API_URL "/on");
      needs_curtailment_refresh = ok;
      break;
    case PendingSolarAction::TabuchiOff:
      ok = post_remote_empty(CURTAILMENT_TABUCHI_API_URL "/off");
      needs_curtailment_refresh = ok;
      break;
    case PendingSolarAction::ShedOn:
      ok = post_remote_empty(CURTAILMENT_MSERIES_API_URL "/on");
      needs_curtailment_refresh = ok;
      break;
    case PendingSolarAction::ShedOff:
      ok = post_remote_empty(CURTAILMENT_MSERIES_API_URL "/off");
      needs_curtailment_refresh = ok;
      break;
    case PendingSolarAction::Iq8On:
      ok = post_remote_empty(CURTAILMENT_IQ8_API_URL "/on");
      needs_curtailment_refresh = ok;
      break;
    case PendingSolarAction::Iq8Off:
      ok = post_remote_empty(CURTAILMENT_IQ8_API_URL "/off");
      needs_curtailment_refresh = ok;
      break;
    case PendingSolarAction::None:
    default:
      break;
  }

  pending_solar_action = PendingSolarAction::None;

  if (needs_storage_refresh) {
    storage_bits_last_poll_ms = 0;
  }
  if (needs_curtailment_refresh) {
    curtailment_last_poll_ms = 0;
  }

  return true;
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
  JsonVariantConst contactor_state_code = doc["contactor_state_code"];
  if (!contactor_state_code.isNull()) {
    tesla_summary.contactor_state_code = (uint8_t)contactor_state_code.as<int>();
  }
  JsonVariantConst contactor_state = doc["contactor_state"];
  if (contactor_state.is<const char*>()) {
    strncpy(tesla_summary.contactor_state, contactor_state.as<const char*>(), sizeof(tesla_summary.contactor_state) - 1);
    tesla_summary.contactor_state[sizeof(tesla_summary.contactor_state) - 1] = '\0';
    tesla_summary.has_contactor_state = true;
  } else if (!contactor_state_code.isNull()) {
    strncpy(tesla_summary.contactor_state, parse_tesla_contactor_state(tesla_summary.contactor_state_code),
            sizeof(tesla_summary.contactor_state) - 1);
    tesla_summary.contactor_state[sizeof(tesla_summary.contactor_state) - 1] = '\0';
    tesla_summary.has_contactor_state = true;
  }
  // Dual Solis: inverter 1 contactor from BE/info (single BE for now). Inverter 2 stays false until BE/X.
  solar_data.solis1_contactor_closed = (tesla_summary.contactor_state_code == 4);  // 4 = CLOSED

  tesla_summary.has_battery_12v_voltage = false;
  tesla_summary.battery_12v_voltage_V = 0.0f;
  JsonVariantConst battery_12v_voltage = doc["battery_12v_voltage"];
  if (!battery_12v_voltage.isNull()) {
    tesla_summary.battery_12v_voltage_V = battery_12v_voltage.as<float>();
    tesla_summary.has_battery_12v_voltage = true;
  }

  tesla_summary.has_battery_12v_current = false;
  tesla_summary.battery_12v_current_A = 0.0f;
  JsonVariantConst battery_12v_current = doc["battery_12v_current"];
  if (!battery_12v_current.isNull()) {
    tesla_summary.battery_12v_current_A = battery_12v_current.as<float>();
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

  // Power: support total_* (master+slave) and per-unit; configure MQTT to publish totals for dual inverter
  if (strcmp(suffix, "total_solar_power") == 0 || strcmp(suffix, "solar_power") == 0) {
    solar_data.solark_pv_power_W = val;
    solar_data.pv_power_W = val;
  } else if (strcmp(suffix, "total_load_power") == 0 || strcmp(suffix, "load_power") == 0) {
    solar_data.solark_load_power_W = val;
    solar_data.load_power_W = val;
  } else if (strcmp(suffix, "total_grid_ct_power") == 0 || strcmp(suffix, "total_grid_power") == 0 ||
             strcmp(suffix, "grid_power") == 0) {
    solar_data.solark_grid_power_W = val;
    solar_data.grid_power_W = val;
  } else if (strcmp(suffix, "total_batt_power") == 0 || strcmp(suffix, "total_battery_power") == 0 ||
             strcmp(suffix, "battery_power") == 0) {
    solar_data.solark_battery_power_W = val;
    solar_data.battery_power_W = val;
  } else if (strcmp(suffix, "battery_voltage") == 0 || strcmp(suffix, "voltage") == 0) {
    solar_data.solark_battery_voltage_V = val;
  } else if (strcmp(suffix, "battery_temperature") == 0 || strcmp(suffix, "battery_temp") == 0) {
    solar_data.solark_battery_temp_C = val;
  } else if (strcmp(suffix, "total_battery_current") == 0 || strcmp(suffix, "battery_current") == 0) {
    solar_data.solark_total_battery_current_A = val;
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

// ── solar/solis/sensors/<suffix> handler (S6 app / Modbus publishes here) ─────

static void handle_solis_sensor(const char* suffix, const char* payload, int payload_len) {
  char val_buf[32];
  int copy_len = (payload_len < (int)(sizeof(val_buf) - 1)) ? payload_len : (int)(sizeof(val_buf) - 1);
  memcpy(val_buf, payload, copy_len);
  val_buf[copy_len] = '\0';

  float val = atof(val_buf);

  // PV power only — do NOT map dc_power (battery DC on hybrid, not solar)
  if (strcmp(suffix, "pv_power") == 0 || strcmp(suffix, "pv_power_W") == 0 ||
      strcmp(suffix, "solar_power") == 0 || strcmp(suffix, "total_solar_power") == 0 ||
      strcmp(suffix, "pv") == 0 || strcmp(suffix, "solis_pv") == 0) {
    solar_data.solis_pv_power_W = val;
  } else if (strcmp(suffix, "load_power") == 0 || strcmp(suffix, "total_load_power") == 0) {
    solar_data.solis_load_power_W = val;
  } else if (strcmp(suffix, "grid_power") == 0 || strcmp(suffix, "total_grid_power") == 0 ||
             strcmp(suffix, "total_grid_ct_power") == 0) {
    solar_data.solis_grid_power_W = val;
  } else if (strcmp(suffix, "battery_power") == 0 || strcmp(suffix, "total_battery_power") == 0) {
    solar_data.solis_battery_power_W = val;
  } else if (strcmp(suffix, "battery_soc") == 0) {
    solar_data.solis_battery_soc_pct = val;
  } else if (strcmp(suffix, "day_pv_energy") == 0) {
    solar_data.solis_day_pv_energy_kWh = val;
  }

  solar_data.solis_last_update_ms = millis();

  // Dual Solis: mirror current single Solis topic into Solis 1 (left). Solis 2 stays 0 until BE/X.
  solar_data.solis1_pv_power_W = solar_data.solis_pv_power_W;
  solar_data.solis1_load_power_W = solar_data.solis_load_power_W;
  solar_data.solis1_grid_power_W = solar_data.solis_grid_power_W;
  solar_data.solis1_battery_power_W = solar_data.solis_battery_power_W;
  solar_data.solis1_battery_soc_pct = solar_data.solis_battery_soc_pct;
  solar_data.solis1_day_pv_energy_kWh = solar_data.solis_day_pv_energy_kWh;
  solar_data.solis1_last_update_ms = solar_data.solis_last_update_ms;
}

// ── envoy/summary/* handler (simple numeric values from server) ─────────────

static void handle_envoy_summary(const char* topic, const char* payload, int payload_len) {
  char val_buf[32];
  int copy_len = (payload_len < (int)(sizeof(val_buf) - 1)) ? payload_len : (int)(sizeof(val_buf) - 1);
  memcpy(val_buf, payload, copy_len);
  val_buf[copy_len] = '\0';
  float val = atof(val_buf);

  const char* suffix = topic + strlen("envoy/summary/");
  bool total_day_updated = false;
  if (strcmp(suffix, "total_live") == 0) {
    solar_data.envoy_total_live_W = val;
  } else if (strcmp(suffix, "total_today") == 0) {
    solar_data.envoy_total_today_kWh = val;
    total_day_updated = true;
  } else if (strcmp(suffix, "house_today") == 0) {
    solar_data.envoy_house_today_kWh = val;
  } else if (strcmp(suffix, "shed_today") == 0) {
    solar_data.envoy_shed_today_kWh = val;
  } else if (strcmp(suffix, "trailer_today") == 0) {
    solar_data.envoy_trailer_today_kWh = val;
  }
  solar_data.envoy_last_update_ms = millis();
  if (total_day_updated) {
    recompute_total_day_pv_energy();
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
  esp_mqtt_client_subscribe(client, "solar/solark/sensors/total_solar_power", 0);
  esp_mqtt_client_subscribe(client, "solar/solark/sensors/load_power", 0);
  esp_mqtt_client_subscribe(client, "solar/solark/sensors/total_load_power", 0);
  esp_mqtt_client_subscribe(client, "solar/solark/sensors/grid_power", 0);
  esp_mqtt_client_subscribe(client, "solar/solark/sensors/total_grid_power", 0);
  esp_mqtt_client_subscribe(client, "solar/solark/sensors/total_grid_ct_power", 0);
  esp_mqtt_client_subscribe(client, "solar/solark/sensors/battery_power", 0);
  esp_mqtt_client_subscribe(client, "solar/solark/sensors/total_battery_power", 0);
  esp_mqtt_client_subscribe(client, "solar/solark/sensors/total_batt_power", 0);
  esp_mqtt_client_subscribe(client, "solar/solark/sensors/battery_voltage", 0);
  esp_mqtt_client_subscribe(client, "solar/solark/sensors/battery_temperature", 0);
  esp_mqtt_client_subscribe(client, "solar/solark/sensors/total_battery_current", 0);
  esp_mqtt_client_subscribe(client, "solar/solark/sensors/battery_soc", 0);
  esp_mqtt_client_subscribe(client, "solar/solark/sensors/day_pv_energy", 0);
  esp_mqtt_client_subscribe(client, "solar/solis/sensors/#", 0);
  Serial.println("[MQTT-BRIDGE] Subscribed to Solark+Solis+BE; tesla/envoy via HTTP poll");
}

void tick_alive_counter() {
  if (datalayer.battery.status.CAN_battery_still_alive > 0) {
    datalayer.battery.status.CAN_battery_still_alive--;
  }

  if (process_pending_solar_action()) {
    return;
  }

  unsigned long now = millis();

  if (storage_bits_last_poll_ms == 0) {
    if (now >= 3000UL) {
      storage_bits_last_poll_ms = now;
      fetch_storage_bits_http();
      return;
    }
  } else if ((now - storage_bits_last_poll_ms) >= STORAGE_BITS_POLL_INTERVAL_MS) {
    storage_bits_last_poll_ms = now;
    fetch_storage_bits_http();
    return;
  }

  if (curtailment_last_poll_ms == 0) {
    if (now >= 3500UL) {
      curtailment_last_poll_ms = now;
      fetch_curtailment_http();
      return;
    }
  } else if ((now - curtailment_last_poll_ms) >= CURTAILMENT_POLL_INTERVAL_MS) {
    curtailment_last_poll_ms = now;
    fetch_curtailment_http();
    return;
  }

  // Poll remote root sensors frequently for Tesla/Solis inverter-side values
  if (root_sensors_last_poll_ms == 0) {
    if (now >= 5000UL) {
      root_sensors_last_poll_ms = now;
      fetch_root_sensors_http();
      return;
    }
  } else if ((now - root_sensors_last_poll_ms) >= ROOT_SENSORS_POLL_INTERVAL_MS) {
    root_sensors_last_poll_ms = now;
    fetch_root_sensors_http();
    return;
  }
  // Poll envoy HTTP API every 60s (staggered: first poll at 10s after boot)
  if (envoy_last_poll_ms == 0) {
    if (now >= 10000UL) {
      envoy_last_poll_ms = now;
      fetch_envoy_http();
      return;
    }
  } else if ((now - envoy_last_poll_ms) >= ENVOY_POLL_INTERVAL_MS) {
    envoy_last_poll_ms = now;
    fetch_envoy_http();
    return;
  }
}

const SolarData& get_solar_data() {
  return solar_data;
}

const TeslaSummaryData& get_tesla_summary() {
  return tesla_summary;
}

bool set_solis_mode_self_use() {
  return queue_solar_action(PendingSolarAction::SolisSelfUse);
}

bool set_solis_mode_feed_in_priority() {
  return queue_solar_action(PendingSolarAction::SolisFeedInPriority);
}

bool set_tabuchi_export_enabled(bool enabled) {
  return queue_solar_action(enabled ? PendingSolarAction::TabuchiOn : PendingSolarAction::TabuchiOff);
}

bool set_shed_micros_enabled(bool enabled) {
  return queue_solar_action(enabled ? PendingSolarAction::ShedOn : PendingSolarAction::ShedOff);
}

bool set_iq8_micros_enabled(bool enabled) {
  return queue_solar_action(enabled ? PendingSolarAction::Iq8On : PendingSolarAction::Iq8Off);
}

const MqttLogEntry* get_mqtt_log() {
  return mqtt_log;
}

int get_mqtt_log_count() {
  return mqtt_log_count;
}

}  // namespace mqtt_display_bridge

#endif  // HW_WAVESHARE7B_DISPLAY_ONLY

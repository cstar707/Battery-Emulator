#ifdef HW_WAVESHARE7B_DISPLAY_ONLY
// ============================================================
// Display-only webserver for the Waveshare ESP32-S3-7B
// Provides: dashboard, cell monitor, events, MQTT log, settings, OTA, reboot
// All battery-emulator-specific endpoints removed.
// ============================================================

#include "webserver.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include "../../communication/nvm/comm_nvm.h"
#include "../../datalayer/datalayer.h"
#include "../../devboard/mqtt/mqtt.h"
#include "../../devboard/wifi/wifi.h"
#include "../../lib/bblanchon-ArduinoJson/ArduinoJson.h"
#include "../../lib/ESP32Async-ESPAsyncWebServer/src/ESPAsyncWebServer.h"
#include "../../lib/ayushsharma82-ElegantOTA/src/ElegantOTA.h"
#include "../../lib/mathieucarbou-AsyncTCPSock/src/AsyncTCP.h"
#include "../display/mqtt_display_bridge.h"
#include "../utils/events.h"
#include "../utils/logging.h"
#include "../utils/timer.h"

extern std::string http_username;
extern std::string http_password;

static AsyncWebServer server(80);
static MyTimer ota_timeout_timer(15000);
bool ota_active = false;

// ── OTA callbacks ─────────────────────────────────────────────────────────────

void onOTAStart() {
  ota_active = true;
  ota_timeout_timer.reset();
  logging.println("OTA update started");
}

void onOTAProgress(size_t current, size_t final_sz) {
  ota_timeout_timer.reset();
  if (millis() % 2000 < 10) {
    logging.printf("OTA progress: %u / %u bytes\n", current, final_sz);
  }
}

void onOTAEnd(bool success) {
  ota_active = false;
  if (success) {
    logging.println("OTA update successful, rebooting...");
  } else {
    logging.println("OTA update failed");
  }
}

void ota_monitor() {
  if (ota_active && ota_timeout_timer.elapsed()) {
    ota_active = false;
    onOTAEnd(false);
  }
}

void init_ElegantOTA() {
  ElegantOTA.begin(&server);
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);
}

// ── HTML helpers ──────────────────────────────────────────────────────────────

static const char HTML_HEADER[] =
    "<!DOCTYPE html><html><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Battery Monitor</title>"
    "<style>"
    "body{background:#0d1117;color:#e6edf3;font-family:monospace;margin:0;padding:12px}"
    "h1{color:#58a6ff;margin:0 0 8px}"
    "nav a{color:#58a6ff;text-decoration:none;margin-right:16px}"
    ".card{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:12px;margin:8px 0}"
    ".grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(180px,1fr));gap:8px}"
    ".stat-label{font-size:11px;color:#8b949e}"
    ".stat-value{font-size:18px;font-weight:bold;margin-top:2px}"
    ".ok{color:#7ee787}.warn{color:#ffa657}.err{color:#ff7b72}"
    "table{border-collapse:collapse;width:100%}"
    "td,th{border:1px solid #30363d;padding:4px 8px;font-size:12px}"
    "th{background:#161b22;color:#8b949e}"
    "input,select{background:#0d1117;color:#e6edf3;border:1px solid #30363d;border-radius:4px;padding:6px;width:100%;box-sizing:border-box}"
    "button,input[type=submit]{background:#1f6feb;color:#fff;border:none;border-radius:6px;padding:8px 18px;cursor:pointer;width:auto}"
    "button:hover{background:#388bfd}"
    ".section-title{color:#f0a500;font-size:14px;font-weight:bold;margin:12px 0 6px}"
    "</style>"
    "</head><body>"
    "<h1>Battery Monitor</h1>"
    "<nav>"
    "<a href='/'>Dashboard</a>"
    "<a href='/cellmonitor'>Cells</a>"
    "<a href='/events'>Events</a>"
    "<a href='/canlog'>MQTT Log</a>"
    "<a href='/log'>Debug</a>"
    "<a href='/settings'>Settings</a>"
    "<a href='/update'>OTA</a>"
    "<a href='/reboot'>Reboot</a>"
    "</nav><hr style='border-color:#30363d'>";

static const char HTML_FOOTER[] = "</body></html>";

// ── /api/status JSON endpoint ─────────────────────────────────────────────────

static void handle_api_status(AsyncWebServerRequest* request) {
  static JsonDocument doc;
  doc.clear();

  auto& bat = datalayer.battery.status;
  auto& info = datalayer.battery.info;

  doc["soc_pct"] = bat.reported_soc / 100.0f;
  doc["soc_real_pct"] = bat.real_soc / 100.0f;
  doc["voltage_v"] = bat.voltage_dV / 10.0f;
  doc["current_a"] = bat.current_dA / 10.0f;
  doc["power_w"] = bat.active_power_W;
  doc["temp_min_c"] = bat.temperature_min_dC / 10.0f;
  doc["temp_max_c"] = bat.temperature_max_dC / 10.0f;
  doc["cell_min_mv"] = bat.cell_min_voltage_mV;
  doc["cell_max_mv"] = bat.cell_max_voltage_mV;
  doc["cell_delta_mv"] = bat.cell_max_voltage_mV - bat.cell_min_voltage_mV;
  doc["capacity_wh"] = info.total_capacity_Wh;
  doc["remaining_wh"] = bat.remaining_capacity_Wh;
  doc["soh_pct"] = bat.soh_pptt / 100.0f;
  doc["num_cells"] = info.number_of_cells;
  doc["mqtt_alive"] = bat.CAN_battery_still_alive;

  const SolarData& sol = mqtt_display_bridge::get_solar_data();
  doc["solar"]["pv_w"] = sol.pv_power_W;
  doc["solar"]["load_w"] = sol.load_power_W;
  doc["solar"]["grid_w"] = sol.grid_power_W;
  doc["solar"]["batt_w"] = sol.battery_power_W;
  doc["solar"]["batt_soc"] = sol.battery_soc_pct;
  doc["solar"]["day_pv_kwh"] = sol.day_pv_energy_kWh;
  doc["solar"]["last_update_ms"] = sol.last_update_ms;

  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
  doc.clear();
}

// ── / Dashboard ───────────────────────────────────────────────────────────────

static void handle_root(AsyncWebServerRequest* request) {
  auto& bat = datalayer.battery.status;
  auto& info = datalayer.battery.info;
  bool alive = bat.CAN_battery_still_alive > 0;

  float soc = bat.reported_soc / 100.0f;
  float voltage = bat.voltage_dV / 10.0f;
  float current = bat.current_dA / 10.0f;
  float power = bat.active_power_W;
  float tmin = bat.temperature_min_dC / 10.0f;
  float tmax = bat.temperature_max_dC / 10.0f;
  float cell_min = bat.cell_min_voltage_mV / 1000.0f;
  float cell_max = bat.cell_max_voltage_mV / 1000.0f;
  uint16_t delta = bat.cell_max_voltage_mV - bat.cell_min_voltage_mV;

  char buf[4096];
  snprintf(buf, sizeof(buf),
           "%s"
           "<div class='card'>"
           "<div class='section-title'>BATTERY</div>"
           "<div class='grid'>"
           "<div><div class='stat-label'>SOC</div><div class='stat-value %s'>%.1f %%</div></div>"
           "<div><div class='stat-label'>VOLTAGE</div><div class='stat-value'>%.1f V</div></div>"
           "<div><div class='stat-label'>CURRENT</div><div class='stat-value'>%.1f A</div></div>"
           "<div><div class='stat-label'>POWER</div><div class='stat-value'>%.0f W</div></div>"
           "<div><div class='stat-label'>TEMP MIN</div><div class='stat-value'>%.1f °C</div></div>"
           "<div><div class='stat-label'>TEMP MAX</div><div class='stat-value'>%.1f °C</div></div>"
           "<div><div class='stat-label'>CELL MIN</div><div class='stat-value'>%.3f V</div></div>"
           "<div><div class='stat-label'>CELL MAX</div><div class='stat-value'>%.3f V</div></div>"
           "<div><div class='stat-label'>CELL DELTA</div><div class='stat-value %s'>%u mV</div></div>"
           "<div><div class='stat-label'>CAPACITY</div><div class='stat-value'>%.1f kWh</div></div>"
           "<div><div class='stat-label'>REMAINING</div><div class='stat-value'>%.1f kWh</div></div>"
           "<div><div class='stat-label'>SOH</div><div class='stat-value'>%.1f %%</div></div>"
           "<div><div class='stat-label'>CELLS</div><div class='stat-value'>%d</div></div>"
           "<div><div class='stat-label'>MQTT DATA</div><div class='stat-value %s'>%s</div></div>"
           "</div></div>",
           HTML_HEADER,
           soc < 20 ? "err" : soc < 50 ? "warn" : "ok", soc,
           voltage, current, power, tmin, tmax, cell_min, cell_max,
           delta < 50 ? "ok" : delta < 100 ? "warn" : "err", delta,
           info.total_capacity_Wh / 1000.0f,
           bat.remaining_capacity_Wh / 1000.0f,
           bat.soh_pptt / 100.0f,
           info.number_of_cells,
           alive ? "ok" : "err", alive ? "LIVE" : "STALE");

  // Solar section
  const SolarData& sol = mqtt_display_bridge::get_solar_data();
  char sol_buf[1024];
  if (sol.last_update_ms == 0) {
    snprintf(sol_buf, sizeof(sol_buf), "<div class='card'><div class='section-title'>SOLAR</div><p class='err'>No solar data yet</p></div>");
  } else {
    snprintf(sol_buf, sizeof(sol_buf),
             "<div class='card'>"
             "<div class='section-title'>SOLAR / INVERTER</div>"
             "<div class='grid'>"
             "<div><div class='stat-label'>PV POWER</div><div class='stat-value ok'>%.0f W</div></div>"
             "<div><div class='stat-label'>LOAD POWER</div><div class='stat-value'>%.0f W</div></div>"
             "<div><div class='stat-label'>GRID POWER</div><div class='stat-value %s'>%.0f W</div></div>"
             "<div><div class='stat-label'>BATT POWER</div><div class='stat-value %s'>%.0f W</div></div>"
             "<div><div class='stat-label'>INV SOC</div><div class='stat-value ok'>%.1f %%</div></div>"
             "<div><div class='stat-label'>TODAY PV</div><div class='stat-value'>%.2f kWh</div></div>"
             "</div></div>",
             sol.pv_power_W,
             sol.load_power_W,
             sol.grid_power_W < 0 ? "ok" : "err", sol.grid_power_W,
             sol.battery_power_W >= 0 ? "ok" : "warn", sol.battery_power_W,
             sol.battery_soc_pct,
             sol.day_pv_energy_kWh);
  }

  String page = String(buf) + sol_buf + HTML_FOOTER;
  request->send(200, "text/html", page);
}

// ── /cellmonitor ──────────────────────────────────────────────────────────────

static void handle_cellmonitor(AsyncWebServerRequest* request) {
  auto& bat = datalayer.battery.status;
  uint8_t n = datalayer.battery.info.number_of_cells;

  String page = HTML_HEADER;
  page += "<div class='card'><div class='section-title'>CELL VOLTAGES</div>";
  if (n == 0) {
    page += "<p class='err'>No cell data — waiting for MQTT</p>";
  } else {
    page += "<table><tr><th>#</th><th>mV</th><th>V</th></tr>";
    for (int i = 0; i < n; i++) {
      uint16_t mv = bat.cell_voltages_mV[i];
      const char* cls = (mv < 3000) ? "err" : (mv < 3200) ? "warn" : "ok";
      char row[80];
      snprintf(row, sizeof(row), "<tr><td>%d</td><td class='%s'>%u</td><td class='%s'>%.3f</td></tr>",
               i + 1, cls, mv, cls, mv / 1000.0f);
      page += row;
    }
    page += "</table>";
  }
  page += "</div>";
  page += HTML_FOOTER;
  request->send(200, "text/html", page);
}

// ── /events ───────────────────────────────────────────────────────────────────

static void handle_events(AsyncWebServerRequest* request) {
  String page = HTML_HEADER;
  page += "<div class='card'><div class='section-title'>EVENT LOG</div>"
          "<table><tr><th>Level</th><th>Event</th><th>Count</th><th>Data</th><th>Time (ms)</th></tr>";

  for (int i = 0; i < (int)EVENT_NOF_EVENTS; i++) {
    EVENTS_ENUM_TYPE ev = (EVENTS_ENUM_TYPE)i;
    const EVENTS_STRUCT_TYPE* e = get_event_pointer(ev);
    if (e == nullptr || e->occurences == 0) continue;
    char row[256];
    snprintf(row, sizeof(row),
             "<tr><td>%s</td><td>%s</td><td>%u</td><td>%u</td><td>%llu</td></tr>",
             get_event_level_string(e->level),
             get_event_message_string(ev).c_str(),
             e->occurences,
             e->data,
             (unsigned long long)e->timestamp);
    page += row;
  }

  page += "</table></div>";
  page += HTML_FOOTER;
  request->send(200, "text/html", page);
}

// ── /canlog (MQTT message log) ────────────────────────────────────────────────

static void handle_canlog(AsyncWebServerRequest* request) {
  String page = HTML_HEADER;
  page += "<div class='card'><div class='section-title'>MQTT MESSAGE LOG (last 50)</div>";
  page += "<p style='font-size:11px;color:#8b949e'>Refreshes manually. Shows last 50 incoming MQTT messages.</p>";
  page += "<table><tr><th>Time (ms)</th><th>Topic</th><th>Payload (preview)</th></tr>";

  int count = mqtt_display_bridge::get_mqtt_log_count();
  const MqttLogEntry* log = mqtt_display_bridge::get_mqtt_log();

  if (count == 0) {
    page += "<tr><td colspan='3'>No messages yet</td></tr>";
  } else {
    // Show newest first (log is a circular buffer — iterate all entries)
    for (int i = count - 1; i >= 0; i--) {
      const MqttLogEntry& e = log[i % MQTT_LOG_SIZE];
      char row[512];
      snprintf(row, sizeof(row),
               "<tr><td>%lu</td><td>%s</td><td>%.100s</td></tr>",
               e.timestamp_ms, e.topic, e.payload);
      page += row;
    }
  }

  page += "</table></div>";
  page += HTML_FOOTER;
  request->send(200, "text/html", page);
}

// ── /log (debug log) ─────────────────────────────────────────────────────────

static void handle_log(AsyncWebServerRequest* request) {
  String page = HTML_HEADER;
  page += "<div class='card'><div class='section-title'>DEBUG LOG</div>"
          "<p style='color:#8b949e;font-size:12px'>Live logging is available via USB serial (115200 baud).<br>"
          "Uptime: " + String(millis() / 1000) + "s &nbsp;|&nbsp; Free heap: " + String(ESP.getFreeHeap()) + " bytes</p>"
          "</div>";
  page += HTML_FOOTER;
  request->send(200, "text/html", page);
}

// ── /settings ─────────────────────────────────────────────────────────────────

static void handle_settings_get(AsyncWebServerRequest* request) {
  String page = HTML_HEADER;
  page += "<div class='card'><div class='section-title'>SETTINGS</div>";
  page += "<form method='POST' action='/settings'>";

  // WiFi
  page += "<div class='section-title'>WiFi</div>";
  page += "<label>SSID <input name='ssid' value='" + String(ssid.c_str()) + "'></label><br><br>";
  page += "<label>Password <input name='pass' type='password' placeholder='(unchanged)'></label><br><br>";
  // MQTT
  page += "<div class='section-title'>MQTT Broker</div>";
  page += "<label>Host <input name='mqtt_host' value='" + String(mqtt_server.c_str()) + "'></label><br><br>";
  page += "<label>Port <input name='mqtt_port' type='number' value='" + String(mqtt_port) + "'></label><br><br>";
  page += "<label>User <input name='mqtt_user' value='" + String(mqtt_user.c_str()) + "'></label><br><br>";
  page += "<label>Password <input name='mqtt_pass' type='password' placeholder='(unchanged)'></label><br><br>";
  // Hostname
  page += "<div class='section-title'>Device</div>";
  page += "<label>Hostname <input name='hostname' value='" + String(custom_hostname.c_str()) + "'></label><br><br>";

  page += "<input type='submit' value='Save &amp; Reboot'>";
  page += "</form></div>";
  page += HTML_FOOTER;
  request->send(200, "text/html", page);
}

static void handle_settings_post(AsyncWebServerRequest* request) {
  if (request->hasParam("ssid", true)) {
    ssid = request->getParam("ssid", true)->value().c_str();
  }
  if (request->hasParam("pass", true) && request->getParam("pass", true)->value().length() > 0) {
    password = request->getParam("pass", true)->value().c_str();
  }
  if (request->hasParam("mqtt_host", true)) {
    mqtt_server = request->getParam("mqtt_host", true)->value().c_str();
  }
  // Auto-enable MQTT when a broker host is configured, disable when cleared
  mqtt_enabled = !mqtt_server.empty();
  if (request->hasParam("mqtt_port", true)) {
    mqtt_port = request->getParam("mqtt_port", true)->value().toInt();
  }
  if (request->hasParam("mqtt_user", true)) {
    mqtt_user = request->getParam("mqtt_user", true)->value().c_str();
  }
  if (request->hasParam("mqtt_pass", true) && request->getParam("mqtt_pass", true)->value().length() > 0) {
    mqtt_password = request->getParam("mqtt_pass", true)->value().c_str();
  }
  if (request->hasParam("hostname", true)) {
    custom_hostname = request->getParam("hostname", true)->value().c_str();
  }
  store_settings();
  request->redirect("/settings");
  delay(500);
  ESP.restart();
}

// ── /reboot ──────────────────────────────────────────────────────────────────

static void handle_reboot(AsyncWebServerRequest* request) {
  request->send(200, "text/html",
                String(HTML_HEADER) +
                    "<div class='card'><p>Rebooting...</p></div>" +
                    HTML_FOOTER);
  delay(500);
  ESP.restart();
}

// ── Stub functions required by webserver.h API ───────────────────────────────

String processor(const String& var) {
  return "";
}

String get_firmware_info_processor(const String& var) {
  if (var == "VERSION") return String(version_number);
  if (var == "BOARD") return "Waveshare ESP32-S3-7B Display";
  return "";
}

// ── init_webserver ────────────────────────────────────────────────────────────

void init_webserver() {
  server.on("/", HTTP_GET, handle_root);
  server.on("/cellmonitor", HTTP_GET, handle_cellmonitor);
  server.on("/events", HTTP_GET, handle_events);
  server.on("/canlog", HTTP_GET, handle_canlog);
  server.on("/log", HTTP_GET, handle_log);
  server.on("/settings", HTTP_GET, handle_settings_get);
  server.on("/settings", HTTP_POST, handle_settings_post);
  server.on("/reboot", HTTP_ANY, handle_reboot);
  server.on("/api/status", HTTP_GET, handle_api_status);

  server.onNotFound([](AsyncWebServerRequest* request) {
    request->send(404, "text/plain", "Not found");
  });

  init_ElegantOTA();
  server.begin();
  logging.println("Display-only webserver started on port 80");
}

#endif  // HW_WAVESHARE7B_DISPLAY_ONLY

/**
 * Solark debug page – live view of all Solark RS485 Modbus data (like 10.10.53.32).
 * Page fetches /solark_data every 2 s and updates the display.
 */

#include "solark_html.h"
#include "../../communication/solark_rs485/solark_rs485.h"
#include "../../datalayer/datalayer_extended.h"
#include "../../lib/bblanchon-ArduinoJson/ArduinoJson.h"

static const char SOLARK_PAGE_STYLE[] =
    "<style>"
    "body { background-color: #000; color: #fff; font-family: Arial, sans-serif; }"
    "button { background-color: #505E67; color: white; border: none; padding: 10px 20px; margin: 5px; cursor: pointer; border-radius: 10px; }"
    "button:hover { background-color: #3A4A52; }"
    ".panel { background-color: #303E47; padding: 15px; margin: 10px 0; border-radius: 10px; }"
    "table { border-collapse: collapse; width: 100%; }"
    "th, td { text-align: left; padding: 8px; border: 1px solid #555; }"
    "th { background-color: #1e2c33; }"
    ".live { color: #7f7; }"
    ".off { color: #f77; }"
    ".raw-regs { font-family: monospace; font-size: 12px; }"
    "</style>";

static const char SOLARK_PAGE_SCRIPT[] =
    "<script>"
    "function home() { window.location.href = '/'; }"
    "function refresh() { fetch('/solark_data').then(r=>r.json()).then(function(d){"
    "  document.getElementById('solark-available').textContent = d.available ? 'Yes' : 'No';"
    "  document.getElementById('solark-available').className = d.available ? 'live' : 'off';"
    "  document.getElementById('solark-last').textContent = d.last_read_millis ? (Date.now() - d.last_read_millis) + ' ms ago' : 'never';"
    "  document.getElementById('solark-batt-pwr').textContent = d.battery_power_W + ' W';"
    "  document.getElementById('solark-batt-soc').textContent = (d.battery_soc_pptt/100).toFixed(2) + ' %';"
    "  document.getElementById('solark-batt-v').textContent = (d.battery_voltage_dV/10).toFixed(1) + ' V';"
    "  document.getElementById('solark-batt-a').textContent = (d.battery_current_dA/10).toFixed(1) + ' A';"
    "  document.getElementById('solark-grid').textContent = d.grid_power_W + ' W';"
    "  document.getElementById('solark-load').textContent = d.load_power_W + ' W';"
    "  document.getElementById('solark-pv').textContent = d.pv_power_W + ' W';"
    "  var arr = d.raw_registers || [];"
    "  var html = '';"
    "  for (var i = 0; i < arr.length; i++) { html += '<span title=\"reg ' + i + '\">' + arr[i] + '</span> '; }"
    "  document.getElementById('solark-raw').innerHTML = html || 'none';"
    "}).catch(function(){ document.getElementById('solark-available').textContent = 'Error'; }); }"
    "setInterval(refresh, 2000);"
    "window.onload = function(){ refresh(); };"
    "</script>";

String solark_processor(const String& var) {
  if (var != "X") {
    return String();
  }
  String content = "";
  content.reserve(2200);
  content += FPSTR(SOLARK_PAGE_STYLE);
  content += "<h2>Solark RS485 Debug</h2>";
  content += "<p>Live data from Solark inverter (Modbus RTU). Updates every 2 s.</p>";
  content += "<button onclick='home()'>Back to main page</button>";
  content += "<button onclick='refresh()'>Refresh now</button>";

  content += "<div class='panel'>";
  content += "<h3>Status</h3>";
  content += "<table><tr><th>RS485 link</th><td id='solark-available' class='off'>-</td></tr>";
  content += "<tr><th>Last read</th><td id='solark-last'>-</td></tr></table>";
  content += "</div>";

  content += "<div class='panel'>";
  content += "<h3>Parsed values</h3>";
  content += "<table>";
  content += "<tr><th>Battery power</th><td id='solark-batt-pwr'>-</td></tr>";
  content += "<tr><th>Battery SOC</th><td id='solark-batt-soc'>-</td></tr>";
  content += "<tr><th>Battery voltage</th><td id='solark-batt-v'>-</td></tr>";
  content += "<tr><th>Battery current</th><td id='solark-batt-a'>-</td></tr>";
  content += "<tr><th>Grid power</th><td id='solark-grid'>-</td></tr>";
  content += "<tr><th>Load power</th><td id='solark-load'>-</td></tr>";
  content += "<tr><th>PV power</th><td id='solark-pv'>-</td></tr>";
  content += "</table></div>";

  content += "<div class='panel'>";
  content += "<h3>Raw Modbus registers</h3>";
  content += "<div id='solark-raw' class='raw-regs'>-</div>";
  content += "</div>";

  content += FPSTR(SOLARK_PAGE_SCRIPT);
  return content;
}

int solark_data_json(char* buf, size_t buf_size) {
  const auto& s = datalayer_extended.solark_rs485;
  StaticJsonDocument<512> doc;
  doc["available"] = s.available;
  doc["last_read_millis"] = s.last_read_millis;
  doc["battery_power_W"] = s.battery_power_W;
  doc["battery_soc_pptt"] = s.battery_soc_pptt;
  doc["battery_voltage_dV"] = s.battery_voltage_dV;
  doc["battery_current_dA"] = s.battery_current_dA;
  doc["grid_power_W"] = s.grid_power_W;
  doc["load_power_W"] = s.load_power_W;
  doc["pv_power_W"] = s.pv_power_W;
  JsonArray arr = doc.createNestedArray("raw_registers");
  for (uint8_t i = 0; i < s.raw_register_count && i < sizeof(s.raw_registers) / sizeof(s.raw_registers[0]); i++) {
    arr.add(s.raw_registers[i]);
  }
  size_t len = serializeJson(doc, buf, buf_size);
  return len <= buf_size ? (int)len : -1;
}

bool solark_sensor_state_by_id(const char* sensor_id, String* out_state) {
  if (out_state == nullptr) return false;
  const auto& s = datalayer_extended.solark_rs485;
  /* ESPHome-style state strings (value + optional unit) for API parity with 10.10.53.32. */
  if (strcmp(sensor_id, "sunsynk_battery_soc") == 0) {
    *out_state = String(s.battery_soc_pptt / 100.0, 2) + " %";
    return true;
  }
  if (strcmp(sensor_id, "sunsynk_battery_voltage") == 0) {
    *out_state = String(s.battery_voltage_dV / 10.0, 1) + " V";
    return true;
  }
  if (strcmp(sensor_id, "sunsynk_battery_current") == 0) {
    *out_state = String(s.battery_current_dA / 10.0, 1) + " A";
    return true;
  }
  if (strcmp(sensor_id, "sunsynk_battery_power") == 0 || strcmp(sensor_id, "sunsynk_total_battery_power") == 0) {
    *out_state = String(s.battery_power_W) + " W";
    return true;
  }
  if (strcmp(sensor_id, "sunsynk_grid_power") == 0) {
    *out_state = String(s.grid_power_W) + " W";
    return true;
  }
  if (strcmp(sensor_id, "sunsynk_load_power") == 0) {
    *out_state = String(s.load_power_W) + " W";
    return true;
  }
  if (strcmp(sensor_id, "sunsynk_total_solar_power") == 0 || strcmp(sensor_id, "sunsynk_pv_power") == 0) {
    *out_state = String(s.pv_power_W) + " W";
    return true;
  }
  if (strcmp(sensor_id, "sunsynk_pv1_power") == 0) {
    uint16_t pv1 = (s.raw_register_count > 20) ? s.raw_registers[19] : 0;
    *out_state = String(pv1) + " W";
    return true;
  }
  if (strcmp(sensor_id, "sunsynk_pv2_power") == 0) {
    uint16_t pv2 = (s.raw_register_count > 20) ? s.raw_registers[20] : 0;
    *out_state = String(pv2) + " W";
    return true;
  }
  /* Slave not polled yet; return 0 so server does not break. */
  if (strcmp(sensor_id, "sunsynk_slave_pv1_power") == 0 || strcmp(sensor_id, "sunsynk_slave_pv2_power") == 0) {
    *out_state = "0 W";
    return true;
  }
  return false;
}

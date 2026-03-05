#ifdef HW_WAVESHARE7B_DISPLAY_ONLY
// Minimal NVM settings for the display-only build.
// Only persists: WiFi credentials, MQTT broker settings, hostname, AP config.
// All battery/inverter/charger/shunt type selections are omitted.

#include "comm_nvm.h"
#include "../../datalayer/datalayer.h"
#include "../../devboard/hal/hal.h"
#include "../../devboard/mqtt/mqtt.h"
#include "../../devboard/utils/events.h"
#include "../../devboard/utils/logging.h"
#include "../../devboard/wifi/wifi.h"

// Global Preferences object — used by BatteryEmulatorSettingsStore (class in comm_nvm.h)
Preferences settings;

void init_stored_settings() {
  settings.begin("batterySettings", false);

  esp32hal->set_default_configuration_values();

  // WiFi credentials
  ssid = settings.getString("SSID", "").c_str();
  password = settings.getString("PASSWORD", "").c_str();

  // AP config
  wifiap_enabled = settings.getBool("WIFIAPENABLED", true);
  wifi_channel = settings.getUInt("WIFICHANNEL", 0);
  ssidAP = settings.getString("APNAME", "battery-monitor").c_str();
  passwordAP = settings.getString("APPASSWORD", "123456789").c_str();

  // Hostname
  custom_hostname = settings.getString("HOSTNAME", "").c_str();

  // Static IP (preserved for compatibility with wifi.cpp)
  static_IP_enabled = settings.getBool("STATICIP", false);
  static_local_IP1 = settings.getUInt("LOCALIP1", 192);
  static_local_IP2 = settings.getUInt("LOCALIP2", 168);
  static_local_IP3 = settings.getUInt("LOCALIP3", 10);
  static_local_IP4 = settings.getUInt("LOCALIP4", 150);
  static_gateway1 = settings.getUInt("GATEWAY1", 192);
  static_gateway2 = settings.getUInt("GATEWAY2", 168);
  static_gateway3 = settings.getUInt("GATEWAY3", 10);
  static_gateway4 = settings.getUInt("GATEWAY4", 1);
  static_subnet1 = settings.getUInt("SUBNET1", 255);
  static_subnet2 = settings.getUInt("SUBNET2", 255);
  static_subnet3 = settings.getUInt("SUBNET3", 255);
  static_subnet4 = settings.getUInt("SUBNET4", 0);

  // MQTT broker
  mqtt_enabled = settings.getBool("MQTTENABLED", false);
  mqtt_timeout_ms = settings.getUInt("MQTTTIMEOUT", 2000);
  mqtt_server = settings.getString("MQTTSERVER", "").c_str();
  mqtt_port = settings.getUInt("MQTTPORT", 0);
  mqtt_user = settings.getString("MQTTUSER", "").c_str();
  mqtt_password = settings.getString("MQTTPASSWORD", "").c_str();
  // Auto-enable MQTT if a server is stored but the enabled flag wasn't saved
  if (!mqtt_enabled && !mqtt_server.empty()) {
    mqtt_enabled = true;
  }

  // Logging
  datalayer.system.info.usb_logging_active = settings.getBool("USBENABLED", false);
  datalayer.system.info.web_logging_active = settings.getBool("WEBENABLED", false);

  settings.end();
}

void store_settings() {
  if (!settings.begin("batterySettings", false)) {
    set_event(EVENT_PERSISTENT_SAVE_INFO, 0);
    return;
  }

  settings.putString("SSID", ssid.c_str());
  settings.putString("PASSWORD", password.c_str());
  settings.putString("APNAME", ssidAP.c_str());
  settings.putString("APPASSWORD", passwordAP.c_str());
  settings.putString("HOSTNAME", custom_hostname.c_str());

  settings.putBool("MQTTENABLED", mqtt_enabled);
  settings.putString("MQTTSERVER", mqtt_server.c_str());
  settings.putUInt("MQTTPORT", mqtt_port);
  settings.putString("MQTTUSER", mqtt_user.c_str());
  settings.putString("MQTTPASSWORD", mqtt_password.c_str());

  settings.putBool("USBENABLED", datalayer.system.info.usb_logging_active);
  settings.putBool("WEBENABLED", datalayer.system.info.web_logging_active);

  settings.end();
}

void store_settings_equipment_stop() {
  // No equipment stop button in display-only build
}

#endif  // HW_WAVESHARE7B_DISPLAY_ONLY

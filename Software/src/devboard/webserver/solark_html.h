#ifndef SOLARK_HTML_H
#define SOLARK_HTML_H

#include <Arduino.h>

/** Template processor for the Solark debug page (var "X"). */
String solark_processor(const String& var);

/** Build JSON payload of all Solark RS485 data for live endpoint. Writes to buf, returns length. */
int solark_data_json(char* buf, size_t buf_size);

/**
 * Get ESPHome-style state string for a sensor id (e.g. sunsynk_battery_soc).
 * Used by GET /sensor/sunsynk_* for API parity with 10.10.53.32.
 * Returns true and sets out_state (e.g. "51" or "52.3 V") for known ids; false for unknown.
 */
bool solark_sensor_state_by_id(const char* sensor_id, String* out_state);

#endif

#include <Arduino.h>

String can_logger_processor(void) {
  return F("<html><body><h1>CAN logger page trimmed from this OTA build.</h1></body></html>");
}

String can_replay_processor(void) {
  return F("<html><body><h1>CAN replay page trimmed from this OTA build.</h1></body></html>");
}

String debug_logger_processor(void) {
  return F("<html><body><h1>Debug logger page trimmed from this OTA build.</h1></body></html>");
}

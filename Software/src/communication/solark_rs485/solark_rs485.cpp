/**
 * Solark RS485 Modbus client – read (and when implemented, write) Solark inverter data over RS485 (Modbus RTU).
 * When replacing the device at 10.10.53.32: use the main RS485 port (Serial2) for
 * Solark by defining FEATURE_SOLARK_ON_MAIN_RS485 (inverter must be CAN, e.g. Sol-Ark LV).
 * Protocol: 9600 8N1, slave 0x01, FC 0x03 (Read Holding Registers).
 * Register map aligned with ESPHome 10.10.53.32 config (SunSynk/Sol-Ark compatible):
 *   docs/esphome-10.10.53.32-solark.yaml, docs/solark-rs485-integration.md
 * Single read: start 167, count 25 → addresses 167..191 (grid/load/inverter/battery/PV).
 */

#include "solark_rs485.h"
#include "../../datalayer/datalayer_extended.h"
#include "../../devboard/hal/hal.h"
#include "../../inverter/INVERTERS.h"

#include <Arduino.h>
#include "../../lib/eModbus-eModbus/ModbusMessage.h"
#include "../../lib/eModbus-eModbus/ModbusTypeDefs.h"
#include "../../lib/eModbus-eModbus/RTUutils.h"

using namespace Modbus;

#ifndef SOLARK_RS485_BAUD
#define SOLARK_RS485_BAUD 9600
#endif
#ifndef SOLARK_MODBUS_SLAVE_ID
#define SOLARK_MODBUS_SLAVE_ID 1
#endif
/** Start address for holding registers (ESPHome 10.10.53.32: 167 = Grid LD, 169 Grid Power, 175 Inverter, 178 Load, 182–184 Battery, 186–187 PV, 190–191 Battery P/I). */
#ifndef SOLARK_MODBUS_START
#define SOLARK_MODBUS_START 167
#endif
#ifndef SOLARK_MODBUS_COUNT
#define SOLARK_MODBUS_COUNT 25
#endif
#ifndef SOLARK_POLL_INTERVAL_MS
#define SOLARK_POLL_INTERVAL_MS 3000
#endif

static bool s_enabled = false;
static bool s_serial_ready = false;
static bool s_use_main_rs485 = false;  /* true when replacing 10.10.53.32 on main port */
static unsigned long s_last_poll_millis = 0;
static HardwareSerial* s_serial = nullptr;
static gpio_num_t s_de_pin = GPIO_NUM_NC;

static void set_de_tx(bool tx) {
  if (s_de_pin != GPIO_NUM_NC) {
    digitalWrite(s_de_pin, tx ? HIGH : LOW);
  }
}

bool solark_rs485_init(void) {
  gpio_num_t tx = esp32hal->SOLARK_RS485_TX_PIN();
  gpio_num_t rx = esp32hal->SOLARK_RS485_RX_PIN();
  s_de_pin = esp32hal->SOLARK_RS485_DE_PIN();

#ifdef FEATURE_SOLARK_ON_MAIN_RS485
  /* Replace 10.10.53.32: use main RS485 (Serial2) for Solark when inverter is CAN-only. */
  if ((tx == GPIO_NUM_NC || rx == GPIO_NUM_NC) && inverter &&
      inverter->interface_type() == InverterInterfaceType::Can) {
    tx = esp32hal->RS485_TX_PIN();
    rx = esp32hal->RS485_RX_PIN();
    s_de_pin = esp32hal->RS485_SE_PIN();  /* direction: SE or DE on main RS485 */
    if (tx != GPIO_NUM_NC && rx != GPIO_NUM_NC) {
      if (!esp32hal->alloc_pins_ignore_unused("SolarkRS485", tx, rx, s_de_pin)) {
        return false;
      }
      s_enabled = true;
      s_use_main_rs485 = true;
      s_serial = &Serial2;
      s_serial->begin(SOLARK_RS485_BAUD, SERIAL_8N1, rx, tx);
      if (s_de_pin != GPIO_NUM_NC) {
        pinMode(s_de_pin, OUTPUT);
        digitalWrite(s_de_pin, LOW);
      }
      s_serial_ready = true;
      s_last_poll_millis = 0;
      return true;
    }
  }
#endif

  if (tx == GPIO_NUM_NC || rx == GPIO_NUM_NC) {
    s_enabled = false;
    s_serial_ready = false;
    s_use_main_rs485 = false;
    return true;
  }

  if (!esp32hal->alloc_pins_ignore_unused("SolarkRS485", tx, rx, s_de_pin)) {
    return false;
  }

  s_enabled = true;
  s_use_main_rs485 = false;
  s_serial = &Serial1;
  s_serial->begin(SOLARK_RS485_BAUD, SERIAL_8N1, rx, tx);

  if (s_de_pin != GPIO_NUM_NC) {
    pinMode(s_de_pin, OUTPUT);
    digitalWrite(s_de_pin, LOW);
  }

  s_serial_ready = true;
  s_last_poll_millis = 0;
  return true;
}

bool solark_rs485_enabled(void) {
  return s_enabled && s_serial_ready;
}

static void send_request(void) {
  ModbusMessage req(SOLARK_MODBUS_SLAVE_ID, READ_HOLD_REGISTER,
                   (uint16_t)SOLARK_MODBUS_START,
                   (uint16_t)SOLARK_MODBUS_COUNT);
  RTUutils::addCRC(req);

  set_de_tx(true);
  delayMicroseconds(50);
  for (uint16_t i = 0; i < req.size(); i++) {
    s_serial->write(req[i]);
  }
  s_serial->flush();
  delayMicroseconds(50);
  set_de_tx(false);
}

/** Read response with timeout. Returns length received, 0 on timeout or error. */
static int read_response(uint8_t* buf, int max_len, unsigned long timeout_ms) {
  unsigned long start = millis();
  int len = 0;
  while (millis() - start < timeout_ms && len < max_len) {
    if (s_serial->available()) {
      buf[len++] = (uint8_t)s_serial->read();
      start = millis();
    }
    delay(1);
  }
  return len;
}

/** Parse FC03 response into registers. Response: [slave, 0x03, byteCount, data..., crcLo, crcHi]. */
static void parse_and_update(uint8_t* buf, int len) {
  if (len < 5) return;
  if (buf[0] != SOLARK_MODBUS_SLAVE_ID || buf[1] != 0x03) return;
  uint8_t byte_count = buf[2];
  int expected = 3 + byte_count + 2;
  if (len < expected) return;
  if (!RTUutils::validCRC(buf, (uint16_t)len)) return;

  unsigned long now = millis();
  datalayer_extended.solark_rs485.last_read_millis = now;
  datalayer_extended.solark_rs485.available = true;

  uint16_t* regs = datalayer_extended.solark_rs485.raw_registers;
  uint8_t n = byte_count / 2;
  if (n > sizeof(datalayer_extended.solark_rs485.raw_registers) / sizeof(uint16_t)) {
    n = sizeof(datalayer_extended.solark_rs485.raw_registers) / sizeof(uint16_t);
  }
  datalayer_extended.solark_rs485.raw_register_count = n;
  for (uint8_t i = 0; i < n; i++) {
    regs[i] = (uint16_t)buf[3 + i * 2] << 8 | buf[3 + i * 2 + 1];
  }

  /* Map registers to datalayer – indices relative to start 167 (see docs/solark-rs485-integration.md). */
  if (n >= 3) {
    datalayer_extended.solark_rs485.grid_power_W = (int32_t)(int16_t)regs[2];   /* 169: S_WORD W */
  }
  if (n >= 12) {
    datalayer_extended.solark_rs485.load_power_W = (int32_t)(int16_t)regs[11];  /* 178: S_WORD W */
  }
  if (n >= 18) {
    datalayer_extended.solark_rs485.battery_voltage_dV = (uint16_t)((uint32_t)regs[16] / 10u); /* 183: ×0.01 V → dV (37.00 V → 370 dV) */
    datalayer_extended.solark_rs485.battery_soc_pptt = regs[17] <= 100 ? (uint16_t)(regs[17] * 100u) : regs[17]; /* 184: % → pptt */
  }
  if (n >= 21) {
    datalayer_extended.solark_rs485.pv_power_W = (int32_t)((uint32_t)regs[19] + (uint32_t)regs[20]); /* 186 PV1 + 187 PV2, U_WORD W */
  }
  if (n >= 25) {
    datalayer_extended.solark_rs485.battery_power_W = (int32_t)(int16_t)regs[23];  /* 190: S_WORD W */
    /* 191: S_WORD ×0.01 A → dA (deciamps): value * 0.01 = A, dA = A*10 = value/10 */
    datalayer_extended.solark_rs485.battery_current_dA = (int16_t)((int32_t)(int16_t)regs[24] / 10);
  }
}

void solark_rs485_poll(void) {
  if (!solark_rs485_enabled()) return;
  unsigned long now = millis();
  if (now - s_last_poll_millis < SOLARK_POLL_INTERVAL_MS) return;
  s_last_poll_millis = now;

  send_request();
  delay(RTUutils::calculateInterval(SOLARK_RS485_BAUD) / 1000 + 5);

  uint8_t rx[256];
  int len = read_response(rx, sizeof(rx), 200);
  if (len > 0) {
    parse_and_update(rx, len);
  }
}

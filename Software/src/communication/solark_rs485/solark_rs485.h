#ifndef SOLARK_RS485_H
#define SOLARK_RS485_H

#include <stdint.h>
#include <stdbool.h>

/** Initialize Solark RS485 Modbus client. Call from setup().
 *  Uses HAL SOLARK_RS485_* pins; no-op if pins are NC.
 *  @return true if init attempted (even when pins are NC), false on allocation failure.
 */
bool solark_rs485_init(void);

/** Poll Solark over RS485 (Modbus FC 0x03). Call from main loop at ~2–5 s interval.
 *  Updates datalayer_extended.solark_rs485 on success.
 */
void solark_rs485_poll(void);

/** Whether the Solark RS485 feature is enabled (second RS485 pins configured). */
bool solark_rs485_enabled(void);

#endif

#ifndef __HW_TCONNECT_H__
#define __HW_TCONNECT_H__

#include "hal.h"

#include "../utils/types.h"

/**
 * LILYGO T-Connect (ESP32-S3): 16MB Flash, 8MB PSRAM, 1x CAN (TWAI) + up to 3x RS485.
 * Pinout: https://wiki.lilygo.cc/get_started/en/High_speed/T-Connect/T-Connect.html#Pin-Overview
 * CAN/RS485 shared pins: TX_1/RX_1=4/5, TX_2/RX_2=6/7, TX_3/RX_3=17/18, TX_4/RX_4=9/10
 * LED: APA102_DATA=8, APA102_CLOCK=3
 */
class TConnectHal : public Esp32Hal {
 public:
  const char* name() { return "LILYGO T-Connect"; }

  virtual int max_gpio() { return 48; }

  // Native CAN (TWAI) on UART pair 1
  virtual gpio_num_t CAN_TX_PIN() { return GPIO_NUM_4; }
  virtual gpio_num_t CAN_RX_PIN() { return GPIO_NUM_5; }
  virtual gpio_num_t CAN_SE_PIN() { return GPIO_NUM_NC; }

  // RS485 on UART pair 2 (DE/RE not in wiki; use NC or configure on board)
  virtual gpio_num_t RS485_EN_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t RS485_TX_PIN() { return GPIO_NUM_6; }
  virtual gpio_num_t RS485_RX_PIN() { return GPIO_NUM_7; }
  virtual gpio_num_t RS485_SE_PIN() { return GPIO_NUM_NC; }

  virtual gpio_num_t PIN_5V_EN() { return GPIO_NUM_NC; }

  // No MCP2515 / MCP2517 on board
  // Contactor / BMS / SD / CHAdeMO not defined on T-Connect
  virtual gpio_num_t POSITIVE_CONTACTOR_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t NEGATIVE_CONTACTOR_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t PRECHARGE_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t BMS_POWER() { return GPIO_NUM_NC; }
  virtual gpio_num_t SECOND_BATTERY_CONTACTORS_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t TRIPLE_BATTERY_CONTACTORS_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t HIA4V1_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t INVERTER_DISCONNECT_CONTACTOR_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t INVERTER_CONTACTOR_ENABLE_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t SD_MISO_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t SD_MOSI_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t SD_SCLK_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t SD_CS_PIN() { return GPIO_NUM_NC; }

  // LED: APA102 data line (IO8); simple LED use
  virtual gpio_num_t LED_PIN() { return GPIO_NUM_8; }
  virtual uint8_t LED_MAX_BRIGHTNESS() { return 40; }

  virtual gpio_num_t EQUIPMENT_STOP_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t WUP_PIN1() { return GPIO_NUM_NC; }
  virtual gpio_num_t WUP_PIN2() { return GPIO_NUM_NC; }

  std::vector<comm_interface> available_interfaces() {
    return {comm_interface::Modbus, comm_interface::RS485, comm_interface::CanNative};
  }
};

#endif  // __HW_TCONNECT_H__

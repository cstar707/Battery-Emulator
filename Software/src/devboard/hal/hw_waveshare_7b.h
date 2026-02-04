#ifndef __HW_WAVESHARE_7B_H__
#define __HW_WAVESHARE_7B_H__

/*
 * Waveshare ESP32-S3-Touch-LCD-7B
 * https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-7B
 *
 * ESP32-S3-N16R8 (16MB Flash, 8MB PSRAM), 7" 1024×600 RGB LCD.
 * CAN: GPIO20 TX, GPIO19 RX (set board to CAN mode via EXIO5 / DIP so CAN transceiver is active).
 * RS485: GPIO15 TX, GPIO16 RX (auto direction).
 * Use the UART Type-C port (silk "UART") for flashing and serial monitor.
 */

#include "hal.h"
#include "../utils/types.h"

class Waveshare7BHal : public Esp32Hal {
 public:
  const char* name() { return "Waveshare ESP32-S3-Touch-LCD-7B"; }

  virtual void set_default_configuration_values() {
    BatteryEmulatorSettingsStore settings;
    if (!settings.settingExists("CANFREQ")) {
      settings.saveUInt("CANFREQ", 8);
    }
  }

  // CAN (TJA1051): TX=20, RX=19. Board must be in CAN mode (EXIO5 high).
  virtual gpio_num_t CAN_TX_PIN() { return GPIO_NUM_20; }
  virtual gpio_num_t CAN_RX_PIN() { return GPIO_NUM_19; }
  virtual gpio_num_t CAN_SE_PIN() { return GPIO_NUM_NC; }

  // RS485: TX=15, RX=16 (board has auto direction switching)
  virtual gpio_num_t RS485_TX_PIN() { return GPIO_NUM_15; }
  virtual gpio_num_t RS485_RX_PIN() { return GPIO_NUM_16; }
  virtual gpio_num_t RS485_EN_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t RS485_SE_PIN() { return GPIO_NUM_NC; }

  // Contactor / BMS / precharge – not on 7B; use NC (user can wire external board if needed)
  virtual gpio_num_t POSITIVE_CONTACTOR_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t NEGATIVE_CONTACTOR_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t PRECHARGE_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t BMS_POWER() { return GPIO_NUM_NC; }
  virtual gpio_num_t SECOND_BATTERY_CONTACTORS_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t HIA4V1_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t INVERTER_DISCONNECT_CONTACTOR_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t INVERTER_CONTACTOR_ENABLE_PIN() { return GPIO_NUM_NC; }

  // LED – GPIO6 used in Waveshare demos (optional)
  virtual gpio_num_t LED_PIN() { return GPIO_NUM_6; }

  // Equipment stop / wake – not wired on 7B
  virtual gpio_num_t EQUIPMENT_STOP_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t WUP_PIN1() { return GPIO_NUM_NC; }
  virtual gpio_num_t WUP_PIN2() { return GPIO_NUM_NC; }

  // SD/TF uses SPI + EXIO4 on 7B; not mapped for BE (would need IO expander)
  virtual gpio_num_t SD_MISO_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t SD_MOSI_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t SD_SCLK_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t SD_CS_PIN() { return GPIO_NUM_NC; }

  std::vector<comm_interface> available_interfaces() {
    return {comm_interface::Modbus, comm_interface::RS485, comm_interface::CanNative};
  }

  virtual const char* name_for_comm_interface(comm_interface comm) {
    switch (comm) {
      case comm_interface::CanNative:
        return "CAN (Native)";
      case comm_interface::Modbus:
        return "Modbus";
      case comm_interface::RS485:
        return "RS485";
      default:
        return Esp32Hal::name_for_comm_interface(comm);
    }
  }
};

#define HalClass Waveshare7BHal

#ifndef HW_CONFIGURED
#define HW_CONFIGURED
#else
#error Multiple HW defined! Please select a single HW
#endif

#endif

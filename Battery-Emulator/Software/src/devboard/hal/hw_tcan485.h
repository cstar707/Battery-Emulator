#ifndef __HW_TCAN485_H__
#define __HW_TCAN485_H__

/*
 * LilyGO T-CAN485
 * https://github.com/Xinyuan-LilyGO/T-CAN485
 *
 * ESP32, 4MB Flash, no PSRAM. 1x CAN (SN65HVD231), 1x RS485 (MAX13487EESA+).
 * 5V–12V input, WS2812 on IO4, TF card. ME2107 booster EN=IO16.
 *
 * Pin overview (from T-CAN485 README):
 *   RS485:  TX=IO22, RX=IO21, CALLBACK=IO17, EN=IO9
 *   WS2812: DATA=IO4
 *   ME2107: EN=IO16
 *   SD:     MISO=IO2, MOSI=IO15, SCLK=IO14, CS=IO13
 *
 * CAN (SN65HVD231): GPIO not in official pin table; verify from schematic.
 *   Assumed TWAI: TX=26, RX=25 (common ESP32 CAN pair).
 *
 * Free for contactors: 0, 1, 3, 5, 12, 18, 19, 23, 27, 32, 33 (if CAN=25,26).
 */

#include "hal.h"
#include "../utils/types.h"

class TCan485Hal : public Esp32Hal {
 public:
  const char* name() { return "LilyGO T-CAN485"; }

  virtual void set_default_configuration_values() {
    BatteryEmulatorSettingsStore settings;
    if (!settings.settingExists("CANFREQ")) {
      settings.saveUInt("CANFREQ", 8);
    }
  }

  // RS485 (per T-CAN485: TX=22, RX=21, EN=9 for direction)
  virtual gpio_num_t RS485_TX_PIN() { return GPIO_NUM_22; }
  virtual gpio_num_t RS485_RX_PIN() { return GPIO_NUM_21; }
  virtual gpio_num_t RS485_EN_PIN() { return GPIO_NUM_9; }

  // Native CAN (SN65HVD231) – verify TX/RX from schematic
  virtual gpio_num_t CAN_TX_PIN() { return GPIO_NUM_26; }
  virtual gpio_num_t CAN_RX_PIN() { return GPIO_NUM_25; }

  // No MCP2515 on board
  virtual gpio_num_t MCP2515_SCK() { return GPIO_NUM_NC; }
  virtual gpio_num_t MCP2515_MOSI() { return GPIO_NUM_NC; }
  virtual gpio_num_t MCP2515_MISO() { return GPIO_NUM_NC; }
  virtual gpio_num_t MCP2515_CS() { return GPIO_NUM_NC; }
  virtual gpio_num_t MCP2515_INT() { return GPIO_NUM_NC; }

  // SD card (TF): MISO=2, MOSI=15, SCLK=14, CS=13
  virtual gpio_num_t SD_MISO_PIN() { return GPIO_NUM_2; }
  virtual gpio_num_t SD_MOSI_PIN() { return GPIO_NUM_15; }
  virtual gpio_num_t SD_SCLK_PIN() { return GPIO_NUM_14; }
  virtual gpio_num_t SD_CS_PIN() { return GPIO_NUM_13; }

  // No simple LED (WS2812 on IO4 needs special driver)
  virtual gpio_num_t LED_PIN() { return GPIO_NUM_NC; }

  // Contactor outputs (free GPIOs)
  virtual gpio_num_t POSITIVE_CONTACTOR_PIN() { return GPIO_NUM_32; }
  virtual gpio_num_t NEGATIVE_CONTACTOR_PIN() { return GPIO_NUM_33; }
  virtual gpio_num_t PRECHARGE_PIN() { return GPIO_NUM_27; }
  virtual gpio_num_t BMS_POWER() { return GPIO_NUM_NC; }

  std::vector<comm_interface> available_interfaces() {
    return {comm_interface::RS485, comm_interface::CanNative, comm_interface::CanAddonMcp2515};
  }

  virtual const char* name_for_comm_interface(comm_interface comm) {
    switch (comm) {
      case comm_interface::CanNative:
        return "CAN (Native - Tesla Battery)";
      case comm_interface::CanAddonMcp2515:
        return "CAN (MCP2515 add-on)";
      case comm_interface::Modbus:
        return "Modbus";
      case comm_interface::RS485:
        return "RS485";
      default:
        return Esp32Hal::name_for_comm_interface(comm);
    }
  }
};

#define HalClass TCan485Hal

#ifndef HW_CONFIGURED
#define HW_CONFIGURED
#else
#error Multiple HW defined! Please select a single HW
#endif

#endif

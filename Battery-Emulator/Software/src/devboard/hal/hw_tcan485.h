#ifndef __HW_TCAN485_H__
#define __HW_TCAN485_H__

/*
 * LilyGO T-CAN485
 * https://github.com/Xinyuan-LilyGO/T-CAN485
 *
 * Pinout aligned with official Battery-Emulator (dalathegreat/Battery-Emulator)
 * which uses HW_LILYGO for this board. See docs/t-can485-pins.md and
 * https://github.com/dalathegreat/Battery-Emulator/wiki
 *
 * ESP32, 4MB Flash, no PSRAM. 1x CAN (SN65HVD231), 1x RS485 (MAX13487EESA+).
 * 5V–12V input (PIN_5V_EN=16), WS2812 on IO4, TF card.
 *
 * Official BE (hw_lilygo): RS485 EN=17, SE=19; CAN TX=27, RX=26; CAN_SE=23.
 * LilyGO hardware README: RS485 EN=9, CALLBACK=17 – if EN=9 on your board,
 * use RS485_EN_PIN in schematic or try EN=17 for half-duplex driver.
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

  // 5V enable (ME2107 / booster per official BE)
  virtual gpio_num_t PIN_5V_EN() { return GPIO_NUM_16; }

  // RS485 – official BE: TX=22, RX=21, EN=17, SE=19 (LilyGO hardware doc: EN=9)
  virtual gpio_num_t RS485_TX_PIN() { return GPIO_NUM_22; }
  virtual gpio_num_t RS485_RX_PIN() { return GPIO_NUM_21; }
  virtual gpio_num_t RS485_EN_PIN() { return GPIO_NUM_17; }
  virtual gpio_num_t RS485_SE_PIN() { return GPIO_NUM_19; }

  // Native CAN (SN65HVD231) – official BE: TX=27, RX=26, SE=23
  virtual gpio_num_t CAN_TX_PIN() { return GPIO_NUM_27; }
  virtual gpio_num_t CAN_RX_PIN() { return GPIO_NUM_26; }
  virtual gpio_num_t CAN_SE_PIN() { return GPIO_NUM_23; }

  // MCP2515 add-on – official BE pinout (optional second CAN)
  virtual gpio_num_t MCP2515_SCK() { return GPIO_NUM_12; }
  virtual gpio_num_t MCP2515_MOSI() { return GPIO_NUM_5; }
  virtual gpio_num_t MCP2515_MISO() { return GPIO_NUM_34; }
  virtual gpio_num_t MCP2515_CS() { return GPIO_NUM_18; }
  virtual gpio_num_t MCP2515_INT() { return GPIO_NUM_35; }

  // MCP2517 FD add-on – official BE (same SPI-ish assignment)
  virtual gpio_num_t MCP2517_SCK() { return GPIO_NUM_12; }
  virtual gpio_num_t MCP2517_SDI() { return GPIO_NUM_5; }
  virtual gpio_num_t MCP2517_SDO() { return GPIO_NUM_34; }
  virtual gpio_num_t MCP2517_CS() { return GPIO_NUM_18; }
  virtual gpio_num_t MCP2517_INT() { return GPIO_NUM_35; }

  // SD card (TF): MISO=2, MOSI=15, SCLK=14, CS=13
  virtual gpio_num_t SD_MISO_PIN() { return GPIO_NUM_2; }
  virtual gpio_num_t SD_MOSI_PIN() { return GPIO_NUM_15; }
  virtual gpio_num_t SD_SCLK_PIN() { return GPIO_NUM_14; }
  virtual gpio_num_t SD_CS_PIN() { return GPIO_NUM_13; }

  // LED – WS2812 on IO4 (status LED per official BE wiki)
  virtual gpio_num_t LED_PIN() { return GPIO_NUM_4; }

  // Contactor / precharge – official BE
  virtual gpio_num_t POSITIVE_CONTACTOR_PIN() { return GPIO_NUM_32; }
  virtual gpio_num_t NEGATIVE_CONTACTOR_PIN() { return GPIO_NUM_33; }
  virtual gpio_num_t PRECHARGE_PIN() { return GPIO_NUM_25; }
  virtual gpio_num_t BMS_POWER() {
    if (user_selected_gpioopt2 == GPIOOPT2::DEFAULT_OPT_BMS_POWER_18) {
      return GPIO_NUM_18;
    }
    return GPIO_NUM_25;
  }
  virtual gpio_num_t SECOND_BATTERY_CONTACTORS_PIN() { return GPIO_NUM_15; }

  // Automatic precharging / SMA – official BE
  virtual gpio_num_t HIA4V1_PIN() { return GPIO_NUM_25; }
  virtual gpio_num_t INVERTER_DISCONNECT_CONTACTOR_PIN() { return GPIO_NUM_32; }
  virtual gpio_num_t INVERTER_CONTACTOR_ENABLE_PIN() {
    if (user_selected_gpioopt3 == GPIOOPT3::DEFAULT_SMA_ENABLE_05) {
      return GPIO_NUM_5;
    }
    return GPIO_NUM_33;
  }

  // Equipment stop, wake-up – official BE
  virtual gpio_num_t EQUIPMENT_STOP_PIN() { return GPIO_NUM_35; }
  virtual gpio_num_t WUP_PIN1() { return GPIO_NUM_25; }
  virtual gpio_num_t WUP_PIN2() { return GPIO_NUM_32; }

  std::vector<comm_interface> available_interfaces() {
    return {comm_interface::Modbus, comm_interface::RS485, comm_interface::CanNative,
            comm_interface::CanAddonMcp2515, comm_interface::CanFdAddonMcp2518};
  }

  virtual const char* name_for_comm_interface(comm_interface comm) {
    switch (comm) {
      case comm_interface::CanNative:
        return "CAN (Native)";
      case comm_interface::CanAddonMcp2515:
        return "CAN (MCP2515 add-on)";
      case comm_interface::CanFdAddonMcp2518:
        return "CAN FD (MCP2518 add-on)";
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

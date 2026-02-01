#ifndef __HW_WAVESHARE_P4_7B_H__
#define __HW_WAVESHARE_P4_7B_H__

#include "hal.h"

#include "../utils/types.h"

/*
 * Waveshare ESP32-P4-WIFI6-Touch-LCD-7B Development Board
 * - 7" 1024×600 MIPI DSI LCD with capacitive touch (GT911)
 * - ESP32-P4 (RISC-V dual-core + LP core) + 32MB Nor Flash, 32MB PSRAM in package
 * - WiFi 6 / BLE 5 via ESP32-C6-MINI over SDIO
 * - Native CAN on GPIO21/22, RS485 on GPIO26/27
 * - I2C on GPIO7 (SDA) / GPIO8 (SCL) for touch and peripherals
 * - SDMMC 4-wire TF slot: CLK=43, CMD=44, D0-D3=39,40,41,42
 * - No CH422G IO expander (unlike ESP32-S3 7B)
 *
 * Pin usage (from Waveshare wiki):
 *   I2C:     SDA=7,  SCL=8
 *   CAN:     RX=21,  TX=22
 *   RS485:   RX=27,  TX=26
 *   SDMMC:  CLK=43, CMD=44, D0=39, D1=40, D2=41, D3=42
 *   I2S:    MCLK=13, SCLK=12, DOUT=11, LRCK=10, DIN=9, PA_Ctrl=53
 *   PH2.0 12PIN header: 17 programmable GPIOs (see docs/waveshare-p4-7b-pins.md)
 *
 * Contactor pins: GPIO14, GPIO15 (typical free GPIOs on P4; verify against schematic).
 */

class WaveshareP47BHal : public Esp32Hal {
 public:
  const char* name() { return "Waveshare P4 7B"; }

  virtual duration BOOTUP_TIME() { return milliseconds(3000); }

  virtual void set_default_configuration_values() {
    BatteryEmulatorSettingsStore settings;
    if (!settings.settingExists("CANFREQ")) {
      settings.saveUInt("CANFREQ", 8);
    }
  }

  // Native CAN (GPIO21/22 per Waveshare wiki)
  virtual gpio_num_t CAN_TX_PIN() { return GPIO_NUM_22; }
  virtual gpio_num_t CAN_RX_PIN() { return GPIO_NUM_21; }

  // RS485 (GPIO26/27 per wiki)
  virtual gpio_num_t RS485_TX_PIN() { return GPIO_NUM_26; }
  virtual gpio_num_t RS485_RX_PIN() { return GPIO_NUM_27; }

  // MCP2515 add-on: not on-board; user must wire if needed
  virtual gpio_num_t MCP2515_SCK() { return GPIO_NUM_NC; }
  virtual gpio_num_t MCP2515_MOSI() { return GPIO_NUM_NC; }
  virtual gpio_num_t MCP2515_MISO() { return GPIO_NUM_NC; }
  virtual gpio_num_t MCP2515_CS() { return GPIO_NUM_NC; }
  virtual gpio_num_t MCP2515_INT() { return GPIO_NUM_NC; }

  // SD card (SDMMC 4-wire; CS not used in same way)
  virtual gpio_num_t SD_MISO_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t SD_MOSI_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t SD_SCLK_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t SD_CS_PIN() { return GPIO_NUM_NC; }

  // I2C for touch (GT911) and peripherals
  virtual gpio_num_t DISPLAY_SDA_PIN() { return GPIO_NUM_7; }
  virtual gpio_num_t DISPLAY_SCL_PIN() { return GPIO_NUM_8; }

  virtual gpio_num_t LED_PIN() { return GPIO_NUM_NC; }

  // Contactor outputs (free GPIOs; verify against board schematic)
  virtual gpio_num_t POSITIVE_CONTACTOR_PIN() { return GPIO_NUM_14; }
  virtual gpio_num_t NEGATIVE_CONTACTOR_PIN() { return GPIO_NUM_15; }
  virtual gpio_num_t PRECHARGE_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t BMS_POWER() { return GPIO_NUM_NC; }

  std::vector<comm_interface> available_interfaces() {
    return {comm_interface::RS485, comm_interface::CanNative, comm_interface::CanAddonMcp2515};
  }

  virtual const char* name_for_comm_interface(comm_interface comm) {
    switch (comm) {
      case comm_interface::CanNative:
        return "CAN (Native - Tesla Battery)";
      case comm_interface::CanFdNative:
        return "";
      case comm_interface::CanAddonMcp2515:
        return "CAN (MCP2515 - Solis Inverter)";
      case comm_interface::CanFdAddonMcp2518:
        return "";
      case comm_interface::Modbus:
        return "Modbus";
      case comm_interface::RS485:
        return "RS485";
      case comm_interface::Highest:
        return "";
    }
    return Esp32Hal::name_for_comm_interface(comm);
  }
};

#define HalClass WaveshareP47BHal

#ifndef HW_CONFIGURED
#define HW_CONFIGURED
#else
#error Multiple HW defined! Please select a single HW
#endif

#endif

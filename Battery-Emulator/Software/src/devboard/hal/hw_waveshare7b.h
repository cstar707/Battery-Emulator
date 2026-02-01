#ifndef __HW_WAVESHARE7B_H__
#define __HW_WAVESHARE7B_H__

#include "hal.h"

#include "../utils/types.h"

/*
Waveshare ESP32-S3-Touch-LCD-7B Development Board
- 7" 1024x600 RGB LCD with capacitive touch (GT911)
- ESP32-S3-WROOM-1-N16R8 (16MB Flash, 8MB PSRAM)
- Built-in CAN bus (TJA1051) on GPIO19/20 (shared with USB)
- Built-in RS485 on GPIO15/16
- I2C on GPIO8/9 (shared with touch)
- SD card slot on SPI (GPIO11/12/13)
- CH422G IO expander for USB/CAN switching and display control

Pin Usage:
-----------
LCD (RGB interface):
  GPIO0:G3, GPIO1:R3, GPIO2:R4, GPIO3:VSYNC, GPIO5:DE, GPIO7:PCLK
  GPIO10:B7, GPIO14:B3, GPIO17:B6, GPIO18:B5, GPIO21:G7
  GPIO38:B4, GPIO39:G2, GPIO40:R7, GPIO41:R6, GPIO42:R5
  GPIO45:G4, GPIO46:HSYNC, GPIO47:G6, GPIO48:G5

Touch (GT911 via I2C):
  GPIO4:TP_IRQ, GPIO8:TP_SDA, GPIO9:TP_SCL
  EXIO1:TP_RST (via IO expander)

Native CAN (shared with USB):
  GPIO19:CANRX/USB_DN, GPIO20:CANTX/USB_DP
  EXIO5:CAN_SEL (HIGH=CAN, LOW=USB)

RS485:
  GPIO15:RS485_TXD, GPIO16:RS485_RXD

SD Card (SPI):
  GPIO11:MOSI, GPIO12:SCK, GPIO13:MISO
  EXIO4:SD_CS (via IO expander)

MCP2515 CAN Add-on (shares SPI with SD card):
  GPIO11:MOSI, GPIO12:SCK, GPIO13:MISO
  GPIO6:CS (directly wired)
  User-wired:INT pin (suggest external wire to available GPIO)

IO Expander (CH422G at I2C addr 0x24):
  EXIO1:TP_RST, EXIO2:DISP (backlight), EXIO4:SD_CS
  EXIO5:USB_SEL/CAN_SEL, EXIO6:LCD_VDD_EN
  EXIO0, EXIO7: unused (could drive via IO_EXTENSION_Output if needed)

Free GPIOs for integration (only 2 left - see docs/waveshare7b-pins.md):
  GPIO43, GPIO44 -> contactors / relays (wire to sensor/PH2.0 breakout if present)
*/

class Waveshare7BHal : public Esp32Hal {
 public:
  const char* name() { return "Waveshare 7B"; }

  // Longer bootup time due to RGB LCD initialization
  virtual duration BOOTUP_TIME() { return milliseconds(3000); }

  virtual void set_default_configuration_values() {
    BatteryEmulatorSettingsStore settings;
    // Default CAN frequency for MCP2515 - common 8MHz crystal
    if (!settings.settingExists("CANFREQ")) {
      settings.saveUInt("CANFREQ", 8);
    }
  }

  // Native CAN (for Tesla battery communication)
  // Note: Requires IO expander to set EXIO5 HIGH to enable CAN mode
  virtual gpio_num_t CAN_TX_PIN() { return GPIO_NUM_20; }
  virtual gpio_num_t CAN_RX_PIN() { return GPIO_NUM_19; }

  // RS485 interface
  virtual gpio_num_t RS485_TX_PIN() { return GPIO_NUM_15; }
  virtual gpio_num_t RS485_RX_PIN() { return GPIO_NUM_16; }

  // MCP2515 CAN Add-on (for Solis inverter via Pylon protocol)
  // Shares SPI bus with SD card
  virtual gpio_num_t MCP2515_SCK() { return GPIO_NUM_12; }
  virtual gpio_num_t MCP2515_MOSI() { return GPIO_NUM_11; }
  virtual gpio_num_t MCP2515_MISO() { return GPIO_NUM_13; }
  virtual gpio_num_t MCP2515_CS() { return GPIO_NUM_6; }
  // INT pin needs to be wired by user - return NC for now
  // User can wire to any available GPIO and modify if needed
  virtual gpio_num_t MCP2515_INT() { return GPIO_NUM_NC; }

  // SD Card pins (directly connected, but CS is via IO expander)
  virtual gpio_num_t SD_MISO_PIN() { return GPIO_NUM_13; }
  virtual gpio_num_t SD_MOSI_PIN() { return GPIO_NUM_11; }
  virtual gpio_num_t SD_SCLK_PIN() { return GPIO_NUM_12; }
  // SD_CS is on IO expander EXIO4, not directly controllable
  virtual gpio_num_t SD_CS_PIN() { return GPIO_NUM_NC; }

  // I2C for display/touch (GT911) and IO expander (CH422G)
  // Note: This board uses RGB LCD, not I2C OLED display
  // These pins are used for touch controller and IO expander
  virtual gpio_num_t DISPLAY_SDA_PIN() { return GPIO_NUM_8; }
  virtual gpio_num_t DISPLAY_SCL_PIN() { return GPIO_NUM_9; }

  // No onboard LED on this board - NC
  virtual gpio_num_t LED_PIN() { return GPIO_NUM_NC; }

  // Contactor pins - only GPIO43 and GPIO44 are free on this board
  // Wire to sensor/PH2.0 breakout; use relay board or optocoupler, not direct coil drive
  // See docs/waveshare7b-pins.md for full pin reference
  virtual gpio_num_t POSITIVE_CONTACTOR_PIN() { return GPIO_NUM_43; }
  virtual gpio_num_t NEGATIVE_CONTACTOR_PIN() { return GPIO_NUM_44; }
  virtual gpio_num_t PRECHARGE_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t BMS_POWER() { return GPIO_NUM_NC; }

  // Available communication interfaces on this hardware
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

#define HalClass Waveshare7BHal

/* ----- Error checks below, don't change (can't be moved to separate file) ----- */
#ifndef HW_CONFIGURED
#define HW_CONFIGURED
#else
#error Multiple HW defined! Please select a single HW
#endif

#endif

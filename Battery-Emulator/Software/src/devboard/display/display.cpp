#ifdef SMALL_FLASH_DEVICE

// Don't build the display code on small flash devices to save space

void init_display() {}
void update_display() {}

#elif defined(HW_WAVESHARE7B)

// Waveshare ESP32-S3-Touch-LCD-7B display driver
// 1024x600 RGB LCD with GT911 capacitive touch and LVGL
// Using native Waveshare ESP-IDF LCD driver

#include "../../battery/BATTERIES.h"
#include "../../datalayer/datalayer.h"
#include "../hal/hal.h"
#include "../utils/events.h"
#include "../utils/logging.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <lvgl.h>

// Include Waveshare native LCD drivers
#include "waveshare_lcd/i2c.h"
#include "waveshare_lcd/io_extension.h"
#include "waveshare_lcd/rgb_lcd_port.h"

// Display configuration
#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 600

// UI Version - increment with each UI change
#define UI_VERSION "2.2.2"

// GT911 Touch pins
#define TOUCH_INT 4

// LCD panel handle from Waveshare driver
static esp_lcd_panel_handle_t panel_handle = NULL;

// LVGL variables - separate draw buffers to avoid LCD scan conflicts
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* lvgl_buf1 = NULL;
static lv_color_t* lvgl_buf2 = NULL;
static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;

// UI elements
static lv_obj_t* lbl_soc;
static lv_obj_t* lbl_voltage;
static lv_obj_t* lbl_current;
static lv_obj_t* lbl_power;
static lv_obj_t* lbl_energy;
static lv_obj_t* lbl_temp;
static lv_obj_t* lbl_temp_min;
static lv_obj_t* lbl_temp_max;
static lv_obj_t* lbl_cell_delta;
static lv_obj_t* lbl_cell_min;
static lv_obj_t* lbl_cell_max;
static lv_obj_t* lbl_status;
static lv_obj_t* lbl_contactor;
static lv_obj_t* lbl_wifi;
static lv_obj_t* lbl_batt_info;
static lv_obj_t* lbl_sys_info;
static lv_obj_t* bar_soc;

static bool display_initialized = false;
static unsigned long lastUpdateMillis = 0;

// Button UI elements
static lv_obj_t* btn_wifi_ap;
static lv_obj_t* lbl_btn_wifi;
static lv_obj_t* btn_settings;
static lv_obj_t* btn_reboot;
static lv_obj_t* settings_panel;
static lv_obj_t* reboot_confirm_panel;
static lv_obj_t* wifi_panel;
static lv_obj_t* wifi_confirm_panel;
static lv_obj_t* lbl_wifi_status;
static lv_obj_t* lbl_wifi_power;
static lv_obj_t* brightness_slider;
static lv_obj_t* lbl_brightness;
static lv_obj_t* lbl_backup_battery;
static bool wifi_ap_enabled = false;  // Track AP state
static uint8_t brightness_level = 100;  // 0-100%
static uint8_t wifi_tx_power = 1;  // Index into power levels (default 8.5dBm)

// Auto-dim feature
static unsigned long lastTouchMillis = 0;
static bool screen_dimmed = false;
static const unsigned long DIM_TIMEOUT_MS = 60000;  // 60 seconds
static const uint8_t DIM_BRIGHTNESS = 70;  // Dimmed PWM value (higher = dimmer)

// CAN status indicator
static lv_obj_t* lbl_can_status;

// Multi-screen system
static lv_obj_t* screen_main;
static lv_obj_t* screen_cells;
static lv_obj_t* screen_alerts;
static lv_obj_t* tab_btns[3];
static uint8_t current_screen = 0;

// Cell voltage grid (up to 108 cells for Model Y)
#define MAX_CELLS 108
static lv_obj_t* cell_labels[MAX_CELLS];
static lv_obj_t* lbl_cells_title;

// Min/Max tracking
static float voltage_min_ever = 9999.0f;
static float voltage_max_ever = 0.0f;
static float temp_min_ever = 999.0f;
static float temp_max_ever = -999.0f;
static float cell_min_ever = 9.999f;
static float cell_max_ever = 0.0f;
static lv_obj_t* lbl_minmax_voltage;
static lv_obj_t* lbl_minmax_temp;
static lv_obj_t* lbl_minmax_cell;

// Alerts/Events log
#define MAX_EVENTS 10
static char event_log[MAX_EVENTS][64];
static uint8_t event_count = 0;
static lv_obj_t* lbl_alerts;
static lv_obj_t* lbl_event_list;

// GT911 touch controller using ESP-IDF I2C (not Wire library)
static i2c_master_dev_handle_t gt911_dev = NULL;
static uint8_t gt911_addr = 0;
static bool touch_initialized = false;
static uint16_t touch_x = 0, touch_y = 0;
static bool touch_pressed = false;

// GT911 registers
#define GT911_REG_STATUS  0x814E
#define GT911_REG_POINTS  0x8150

static bool gt911_read_reg(uint16_t reg, uint8_t* data, uint8_t len) {
  if (!gt911_dev) return false;
  uint8_t reg_buf[2] = {(uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF)};
  esp_err_t ret = i2c_master_transmit_receive(gt911_dev, reg_buf, 2, data, len, 100);
  return (ret == ESP_OK);
}

static bool gt911_write_reg(uint16_t reg, uint8_t value) {
  if (!gt911_dev) return false;
  uint8_t data[3] = {(uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF), value};
  esp_err_t ret = i2c_master_transmit(gt911_dev, data, 3, 100);
  return (ret == ESP_OK);
}

static void gt911_read_touch() {
  if (!touch_initialized) {
    touch_pressed = false;
    return;
  }
  
  uint8_t status;
  if (!gt911_read_reg(GT911_REG_STATUS, &status, 1)) {
    touch_pressed = false;
    return;
  }
  
  // Check buffer ready bit and touch count
  if ((status & 0x80) && (status & 0x0F) > 0) {
    uint8_t point_data[6];
    if (gt911_read_reg(GT911_REG_POINTS, point_data, 6)) {
      touch_x = point_data[0] | (point_data[1] << 8);
      touch_y = point_data[2] | (point_data[3] << 8);
      touch_pressed = true;
      lastTouchMillis = millis();  // Track touch time for auto-dim
    }
    // Clear buffer status
    gt911_write_reg(GT911_REG_STATUS, 0);
  } else {
    touch_pressed = false;
    if (status & 0x80) {
      gt911_write_reg(GT911_REG_STATUS, 0);
    }
  }
}

// Initialize touch controller using ESP-IDF I2C
static void init_touch() {
  // Touch reset via IO expander (IO1) - set address to 0x5D
  IO_EXTENSION_Output(IO_EXTENSION_IO_1, 0);  // Reset low
  pinMode(TOUCH_INT, OUTPUT);
  digitalWrite(TOUCH_INT, LOW);  // INT low during reset = address 0x5D
  delay(10);
  IO_EXTENSION_Output(IO_EXTENSION_IO_1, 1);  // Reset high
  delay(50);
  pinMode(TOUCH_INT, INPUT);
  delay(50);
  
  // Try both GT911 addresses
  uint8_t addrs[] = {0x5D, 0x14};
  for (int i = 0; i < 2; i++) {
    DEV_I2C_Set_Slave_Addr(&gt911_dev, addrs[i]);
    
    // Try to read product ID (register 0x8140)
    uint8_t prod_id[4] = {0};
    uint8_t reg_buf[2] = {0x81, 0x40};
    esp_err_t ret = i2c_master_transmit_receive(gt911_dev, reg_buf, 2, prod_id, 4, 100);
    
    if (ret == ESP_OK && prod_id[0] == '9' && prod_id[1] == '1' && prod_id[2] == '1') {
      gt911_addr = addrs[i];
      touch_initialized = true;
      DEBUG_PRINTF("GT911 touch found at 0x%02X (ID: %.4s)\n", addrs[i], prod_id);
      return;
    }
  }
  DEBUG_PRINTF("GT911 touch controller not found\n");
}

// Display flush callback for LVGL - copy dirty region to panel
static void disp_flush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
  if (panel_handle) {
    // Copy the rendered region to the RGB panel
    esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, 
                               area->x2 + 1, area->y2 + 1, color_p);
  }
  lv_disp_flush_ready(disp);
}

// Touch read callback for LVGL
static void touchpad_read(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  gt911_read_touch();
  if (touch_pressed) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = touch_x;
    data->point.y = touch_y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

// Create the battery status UI
// Helper to create a stat card
static lv_obj_t* create_stat_card(lv_obj_t* parent, const char* title, int x, int y, int w, int h) {
  lv_obj_t* card = lv_obj_create(parent);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_size(card, w, h);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x16213e), 0);
  lv_obj_set_style_border_color(card, lv_color_hex(0x0f3460), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_radius(card, 8, 0);
  lv_obj_set_style_pad_all(card, 10, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  
  lv_obj_t* lbl_title = lv_label_create(card);
  lv_label_set_text(lbl_title, title);
  lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x888888), 0);
  lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, 0);
  lv_obj_align(lbl_title, LV_ALIGN_TOP_LEFT, 0, 0);
  
  return card;
}

// Flag to freeze display during WiFi operations
static volatile bool display_frozen = false;

// Read backup battery voltage from IO expander ADC
// Returns voltage in millivolts
// Board divider scales battery (3.7V nominal) down; raw 0.69V display -> need ~11x scale
static uint32_t read_backup_battery_mv() {
  uint16_t adc_raw = IO_EXTENSION_Adc_Input();
  // ADC 12-bit (0-4095), 3.3V ref; divider so V_batt ≈ (adc/4095)*3.3*11
  uint32_t mv = ((uint32_t)adc_raw * 3300 * 11) / 4095;
  return mv;
}

// WiFi TX power levels (dBm values and display names)
static const wifi_power_t wifi_power_values[] = {
  WIFI_POWER_5dBm,
  WIFI_POWER_8_5dBm,
  WIFI_POWER_11dBm,
  WIFI_POWER_13dBm,
  WIFI_POWER_15dBm,
  WIFI_POWER_17dBm,
  WIFI_POWER_19_5dBm
};
static const char* wifi_power_names[] = {
  "5 dBm (Min)",
  "8.5 dBm",
  "11 dBm",
  "13 dBm",
  "15 dBm",
  "17 dBm",
  "19.5 dBm (Max)"
};
#define WIFI_POWER_COUNT 7

// Open WiFi settings panel
static void btn_wifi_ap_cb(lv_event_t* e) {
  if (wifi_panel) {
    lv_obj_clear_flag(wifi_panel, LV_OBJ_FLAG_HIDDEN);
  }
}

// Close WiFi settings panel
static void btn_wifi_close_cb(lv_event_t* e) {
  if (wifi_panel) {
    lv_obj_add_flag(wifi_panel, LV_OBJ_FLAG_HIDDEN);
  }
}

// Update WiFi status display
static void update_wifi_status_display() {
  if (lbl_wifi_status) {
    if (wifi_ap_enabled) {
      lv_label_set_text(lbl_wifi_status, "AP: ON (BatteryEmulator)");
    } else {
      lv_label_set_text(lbl_wifi_status, "AP: OFF");
    }
  }
  if (lbl_wifi_power) {
    lv_label_set_text(lbl_wifi_power, wifi_power_names[wifi_tx_power]);
  }
  // Update main button
  if (wifi_ap_enabled) {
    lv_label_set_text(lbl_btn_wifi, "WiFi");
    lv_obj_set_style_bg_color(btn_wifi_ap, lv_color_hex(0x238636), 0);
  } else {
    lv_label_set_text(lbl_btn_wifi, "WiFi");
    lv_obj_set_style_bg_color(btn_wifi_ap, lv_color_hex(0x21262d), 0);
  }
}

// Show AP enable confirmation
static void btn_wifi_enable_cb(lv_event_t* e) {
  if (!wifi_ap_enabled && wifi_confirm_panel) {
    lv_obj_clear_flag(wifi_confirm_panel, LV_OBJ_FLAG_HIDDEN);
  }
}

// Cancel AP enable
static void btn_wifi_confirm_cancel_cb(lv_event_t* e) {
  if (wifi_confirm_panel) {
    lv_obj_add_flag(wifi_confirm_panel, LV_OBJ_FLAG_HIDDEN);
  }
}

// Actually enable AP (after confirmation)
static void btn_wifi_confirm_enable_cb(lv_event_t* e) {
  // Hide confirmation
  if (wifi_confirm_panel) {
    lv_obj_add_flag(wifi_confirm_panel, LV_OBJ_FLAG_HIDDEN);
  }
  
  // Freeze display updates
  display_frozen = true;
  lv_label_set_text(lbl_wifi_status, "Enabling...");
  lv_refr_now(NULL);
  delay(100);
  
  // Enable WiFi AP
  wifi_ap_enabled = true;
  WiFi.mode(WIFI_AP);
  WiFi.setTxPower(wifi_power_values[wifi_tx_power]);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.softAP("BatteryEmulator", "12345678", 1, 0, 4);
  DEBUG_PRINTF("WiFi AP enabled at %s\n", wifi_power_names[wifi_tx_power]);
  
  // Wait for WiFi radio to stabilize
  delay(1500);
  
  update_wifi_status_display();
  display_frozen = false;
}

// Disable AP (no confirmation needed)
static void btn_wifi_disable_cb(lv_event_t* e) {
  if (!wifi_ap_enabled) return;
  
  // Freeze display updates
  display_frozen = true;
  lv_label_set_text(lbl_wifi_status, "Disabling...");
  lv_refr_now(NULL);
  delay(100);
  
  // Disable WiFi
  wifi_ap_enabled = false;
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  DEBUG_PRINTF("WiFi AP disabled\n");
  
  // Wait for WiFi radio to stabilize
  delay(1500);
  
  update_wifi_status_display();
  display_frozen = false;
}

// Decrease TX power
static void btn_wifi_power_down_cb(lv_event_t* e) {
  if (wifi_tx_power > 0) {
    wifi_tx_power--;
    lv_label_set_text(lbl_wifi_power, wifi_power_names[wifi_tx_power]);
    // If AP is on, apply new power
    if (wifi_ap_enabled) {
      WiFi.setTxPower(wifi_power_values[wifi_tx_power]);
      DEBUG_PRINTF("TX power changed to %s\n", wifi_power_names[wifi_tx_power]);
    }
  }
}

// Increase TX power
static void btn_wifi_power_up_cb(lv_event_t* e) {
  if (wifi_tx_power < WIFI_POWER_COUNT - 1) {
    wifi_tx_power++;
    lv_label_set_text(lbl_wifi_power, wifi_power_names[wifi_tx_power]);
    // If AP is on, apply new power
    if (wifi_ap_enabled) {
      WiFi.setTxPower(wifi_power_values[wifi_tx_power]);
      DEBUG_PRINTF("TX power changed to %s\n", wifi_power_names[wifi_tx_power]);
    }
  }
}

// Brightness slider callback
static void brightness_slider_cb(lv_event_t* e) {
  lv_obj_t* slider = lv_event_get_target(e);
  brightness_level = (uint8_t)lv_slider_get_value(slider);
  
  // PWM is inverted: higher PWM = dimmer, so invert the value
  // Slider 100% -> PWM 10 (brightest), Slider 10% -> PWM 97 (dimmest)
  uint8_t pwm_val = 97 - ((brightness_level - 10) * 87) / 90;
  if (pwm_val < 10) pwm_val = 10;
  if (pwm_val > 97) pwm_val = 97;
  IO_EXTENSION_Pwm_Output(pwm_val);
  
  // Update label
  char buf[16];
  snprintf(buf, sizeof(buf), "%d%%", brightness_level);
  lv_label_set_text(lbl_brightness, buf);
  DEBUG_PRINTF("Brightness: %d%% (PWM: %d)\n", brightness_level, pwm_val);
}

// Close settings panel callback
static void btn_close_settings_cb(lv_event_t* e) {
  if (settings_panel) {
    lv_obj_add_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
  }
}

// Settings button callback
static void btn_settings_cb(lv_event_t* e) {
  if (settings_panel) {
    // Toggle visibility
    if (lv_obj_has_flag(settings_panel, LV_OBJ_FLAG_HIDDEN)) {
      lv_obj_clear_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

// Reboot button callback
// Show reboot confirmation panel
static void btn_reboot_cb(lv_event_t* e) {
  if (reboot_confirm_panel) {
    lv_obj_clear_flag(reboot_confirm_panel, LV_OBJ_FLAG_HIDDEN);
  }
}

// Actually reboot
static void btn_reboot_confirm_cb(lv_event_t* e) {
  DEBUG_PRINTF("Rebooting...\n");
  delay(100);
  ESP.restart();
}

// Cancel reboot
static void btn_reboot_cancel_cb(lv_event_t* e) {
  if (reboot_confirm_panel) {
    lv_obj_add_flag(reboot_confirm_panel, LV_OBJ_FLAG_HIDDEN);
  }
}

// Add event to log
static void add_event(const char* msg) {
  // Shift events down
  for (int i = MAX_EVENTS - 1; i > 0; i--) {
    strncpy(event_log[i], event_log[i-1], 63);
  }
  // Add new event at top with timestamp
  unsigned long secs = millis() / 1000;
  snprintf(event_log[0], 63, "[%lu] %s", secs, msg);
  if (event_count < MAX_EVENTS) event_count++;
}

// Screen switching callbacks
static void show_screen_main(lv_event_t* e);
static void show_screen_cells(lv_event_t* e);
static void show_screen_alerts(lv_event_t* e);

static void show_screen_main(lv_event_t* e) {
  if (screen_main) lv_obj_clear_flag(screen_main, LV_OBJ_FLAG_HIDDEN);
  if (screen_cells) lv_obj_add_flag(screen_cells, LV_OBJ_FLAG_HIDDEN);
  if (screen_alerts) lv_obj_add_flag(screen_alerts, LV_OBJ_FLAG_HIDDEN);
  current_screen = 0;
  // Update tab colors
  lv_obj_set_style_bg_color(tab_btns[0], lv_color_hex(0x1f6feb), 0);
  lv_obj_set_style_bg_color(tab_btns[1], lv_color_hex(0x21262d), 0);
  lv_obj_set_style_bg_color(tab_btns[2], lv_color_hex(0x21262d), 0);
}

static void show_screen_cells(lv_event_t* e) {
  if (screen_main) lv_obj_add_flag(screen_main, LV_OBJ_FLAG_HIDDEN);
  if (screen_cells) lv_obj_clear_flag(screen_cells, LV_OBJ_FLAG_HIDDEN);
  if (screen_alerts) lv_obj_add_flag(screen_alerts, LV_OBJ_FLAG_HIDDEN);
  current_screen = 1;
  lv_obj_set_style_bg_color(tab_btns[0], lv_color_hex(0x21262d), 0);
  lv_obj_set_style_bg_color(tab_btns[1], lv_color_hex(0x1f6feb), 0);
  lv_obj_set_style_bg_color(tab_btns[2], lv_color_hex(0x21262d), 0);
}

static void show_screen_alerts(lv_event_t* e) {
  if (screen_main) lv_obj_add_flag(screen_main, LV_OBJ_FLAG_HIDDEN);
  if (screen_cells) lv_obj_add_flag(screen_cells, LV_OBJ_FLAG_HIDDEN);
  if (screen_alerts) lv_obj_clear_flag(screen_alerts, LV_OBJ_FLAG_HIDDEN);
  current_screen = 2;
  lv_obj_set_style_bg_color(tab_btns[0], lv_color_hex(0x21262d), 0);
  lv_obj_set_style_bg_color(tab_btns[1], lv_color_hex(0x21262d), 0);
  lv_obj_set_style_bg_color(tab_btns[2], lv_color_hex(0x1f6feb), 0);
}

static void create_ui() {
  // Dark background
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x0d1117), LV_PART_MAIN);
  lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
  
  // TESLA wordmark logo (stylized with lines) + Battery Emulator
  // Using the distinctive Tesla font style with horizontal accents
  int tx = 15, ty = 6;
  lv_color_t tesla_red = lv_color_hex(0xe82127);
  
  // T
  static lv_point_t t1[] = {{0,0}, {18,0}};
  static lv_point_t t2[] = {{9,0}, {9,22}};
  lv_obj_t* lt1 = lv_line_create(lv_scr_act()); lv_line_set_points(lt1, t1, 2);
  lv_obj_set_style_line_width(lt1, 3, 0); lv_obj_set_style_line_color(lt1, tesla_red, 0);
  lv_obj_set_pos(lt1, tx, ty);
  lv_obj_t* lt2 = lv_line_create(lv_scr_act()); lv_line_set_points(lt2, t2, 2);
  lv_obj_set_style_line_width(lt2, 3, 0); lv_obj_set_style_line_color(lt2, tesla_red, 0);
  lv_obj_set_pos(lt2, tx, ty);
  
  // E (3 horizontal lines)
  static lv_point_t e1[] = {{0,0}, {14,0}};
  static lv_point_t e2[] = {{0,11}, {12,11}};
  static lv_point_t e3[] = {{0,22}, {14,22}};
  static lv_point_t e4[] = {{0,0}, {0,22}};
  lv_obj_t* le1 = lv_line_create(lv_scr_act()); lv_line_set_points(le1, e1, 2);
  lv_obj_set_style_line_width(le1, 3, 0); lv_obj_set_style_line_color(le1, tesla_red, 0);
  lv_obj_set_pos(le1, tx+28, ty);
  lv_obj_t* le2 = lv_line_create(lv_scr_act()); lv_line_set_points(le2, e2, 2);
  lv_obj_set_style_line_width(le2, 3, 0); lv_obj_set_style_line_color(le2, tesla_red, 0);
  lv_obj_set_pos(le2, tx+28, ty);
  lv_obj_t* le3 = lv_line_create(lv_scr_act()); lv_line_set_points(le3, e3, 2);
  lv_obj_set_style_line_width(le3, 3, 0); lv_obj_set_style_line_color(le3, tesla_red, 0);
  lv_obj_set_pos(le3, tx+28, ty);
  lv_obj_t* le4 = lv_line_create(lv_scr_act()); lv_line_set_points(le4, e4, 2);
  lv_obj_set_style_line_width(le4, 3, 0); lv_obj_set_style_line_color(le4, tesla_red, 0);
  lv_obj_set_pos(le4, tx+28, ty);
  
  // S (stylized)
  static lv_point_t s1[] = {{14,0}, {0,0}};
  static lv_point_t s2[] = {{0,0}, {0,11}};
  static lv_point_t s3[] = {{0,11}, {14,11}};
  static lv_point_t s4[] = {{14,11}, {14,22}};
  static lv_point_t s5[] = {{14,22}, {0,22}};
  lv_obj_t* ls1 = lv_line_create(lv_scr_act()); lv_line_set_points(ls1, s1, 2);
  lv_obj_set_style_line_width(ls1, 3, 0); lv_obj_set_style_line_color(ls1, tesla_red, 0);
  lv_obj_set_pos(ls1, tx+52, ty);
  lv_obj_t* ls2 = lv_line_create(lv_scr_act()); lv_line_set_points(ls2, s2, 2);
  lv_obj_set_style_line_width(ls2, 3, 0); lv_obj_set_style_line_color(ls2, tesla_red, 0);
  lv_obj_set_pos(ls2, tx+52, ty);
  lv_obj_t* ls3 = lv_line_create(lv_scr_act()); lv_line_set_points(ls3, s3, 2);
  lv_obj_set_style_line_width(ls3, 3, 0); lv_obj_set_style_line_color(ls3, tesla_red, 0);
  lv_obj_set_pos(ls3, tx+52, ty);
  lv_obj_t* ls4 = lv_line_create(lv_scr_act()); lv_line_set_points(ls4, s4, 2);
  lv_obj_set_style_line_width(ls4, 3, 0); lv_obj_set_style_line_color(ls4, tesla_red, 0);
  lv_obj_set_pos(ls4, tx+52, ty);
  lv_obj_t* ls5 = lv_line_create(lv_scr_act()); lv_line_set_points(ls5, s5, 2);
  lv_obj_set_style_line_width(ls5, 3, 0); lv_obj_set_style_line_color(ls5, tesla_red, 0);
  lv_obj_set_pos(ls5, tx+52, ty);
  
  // L
  static lv_point_t l1[] = {{0,0}, {0,22}};
  static lv_point_t l2[] = {{0,22}, {14,22}};
  lv_obj_t* ll1 = lv_line_create(lv_scr_act()); lv_line_set_points(ll1, l1, 2);
  lv_obj_set_style_line_width(ll1, 3, 0); lv_obj_set_style_line_color(ll1, tesla_red, 0);
  lv_obj_set_pos(ll1, tx+76, ty);
  lv_obj_t* ll2 = lv_line_create(lv_scr_act()); lv_line_set_points(ll2, l2, 2);
  lv_obj_set_style_line_width(ll2, 3, 0); lv_obj_set_style_line_color(ll2, tesla_red, 0);
  lv_obj_set_pos(ll2, tx+76, ty);
  
  // A
  static lv_point_t a1[] = {{7,0}, {0,22}};
  static lv_point_t a2[] = {{7,0}, {14,22}};
  static lv_point_t a3[] = {{3,14}, {11,14}};
  lv_obj_t* la1 = lv_line_create(lv_scr_act()); lv_line_set_points(la1, a1, 2);
  lv_obj_set_style_line_width(la1, 3, 0); lv_obj_set_style_line_color(la1, tesla_red, 0);
  lv_obj_set_pos(la1, tx+98, ty);
  lv_obj_t* la2 = lv_line_create(lv_scr_act()); lv_line_set_points(la2, a2, 2);
  lv_obj_set_style_line_width(la2, 3, 0); lv_obj_set_style_line_color(la2, tesla_red, 0);
  lv_obj_set_pos(la2, tx+98, ty);
  lv_obj_t* la3 = lv_line_create(lv_scr_act()); lv_line_set_points(la3, a3, 2);
  lv_obj_set_style_line_width(la3, 3, 0); lv_obj_set_style_line_color(la3, tesla_red, 0);
  lv_obj_set_pos(la3, tx+98, ty);
  
  // "Battery Emulator" text after TESLA logo
  lv_obj_t* title_sub = lv_label_create(lv_scr_act());
  lv_label_set_text(title_sub, "Battery Emulator");
  lv_obj_set_style_text_font(title_sub, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(title_sub, lv_color_hex(0xc9d1d9), 0);
  lv_obj_set_pos(title_sub, tx + 125, ty + 4);
  
  // Status indicator (right side)
  lbl_status = lv_label_create(lv_scr_act());
  lv_label_set_text(lbl_status, "STANDBY");
  lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xffcc00), 0);
  lv_obj_set_pos(lbl_status, 900, 12);
  
  // CAN status moved to system card - keep variable for updates
  lbl_can_status = lv_label_create(lv_scr_act());
  lv_obj_add_flag(lbl_can_status, LV_OBJ_FLAG_HIDDEN);  // Hidden - we'll show in system card;
  lv_obj_set_style_text_color(lbl_can_status, lv_color_hex(0x8b949e), 0);
  lv_obj_set_pos(lbl_can_status, 180, 14);
  
  // ===== TOP ROW: SOC Section =====
  lv_obj_t* soc_card = create_stat_card(lv_scr_act(), "STATE OF CHARGE", 20, 45, 984, 90);
  
  // SOC percentage (large)
  lbl_soc = lv_label_create(soc_card);
  lv_label_set_text(lbl_soc, "-- %");
  lv_obj_set_style_text_font(lbl_soc, &lv_font_montserrat_32, 0);
  lv_obj_set_style_text_color(lbl_soc, lv_color_hex(0x00ff00), 0);
  lv_obj_set_pos(lbl_soc, 10, 25);
  
  // SOC bar (wide)
  bar_soc = lv_bar_create(soc_card);
  lv_obj_set_size(bar_soc, 820, 30);
  lv_obj_set_pos(bar_soc, 130, 30);
  lv_bar_set_range(bar_soc, 0, 100);
  lv_bar_set_value(bar_soc, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar_soc, lv_color_hex(0x333333), LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar_soc, lv_color_hex(0x00ff00), LV_PART_INDICATOR);
  lv_obj_set_style_radius(bar_soc, 4, LV_PART_MAIN);
  lv_obj_set_style_radius(bar_soc, 4, LV_PART_INDICATOR);
  
  // ===== MIDDLE ROW: Power Stats (4 cards) =====
  int card_w = 235;
  int card_h = 100;
  int row2_y = 145;
  int gap = 12;
  
  // Voltage card
  lv_obj_t* volt_card = create_stat_card(lv_scr_act(), "VOLTAGE", 20, row2_y, card_w, card_h);
  lbl_voltage = lv_label_create(volt_card);
  lv_label_set_text(lbl_voltage, "---.- V");
  lv_obj_set_style_text_font(lbl_voltage, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(lbl_voltage, lv_color_hex(0x58a6ff), 0);
  lv_obj_align(lbl_voltage, LV_ALIGN_BOTTOM_LEFT, 0, -5);
  
  // Current card
  lv_obj_t* curr_card = create_stat_card(lv_scr_act(), "CURRENT", 20 + card_w + gap, row2_y, card_w, card_h);
  lbl_current = lv_label_create(curr_card);
  lv_label_set_text(lbl_current, "---.- A");
  lv_obj_set_style_text_font(lbl_current, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(lbl_current, lv_color_hex(0xf0883e), 0);
  lv_obj_align(lbl_current, LV_ALIGN_BOTTOM_LEFT, 0, -5);
  
  // Power card
  lv_obj_t* pwr_card = create_stat_card(lv_scr_act(), "POWER", 20 + (card_w + gap)*2, row2_y, card_w, card_h);
  lbl_power = lv_label_create(pwr_card);
  lv_label_set_text(lbl_power, "---- W");
  lv_obj_set_style_text_font(lbl_power, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(lbl_power, lv_color_hex(0xa371f7), 0);
  lv_obj_align(lbl_power, LV_ALIGN_BOTTOM_LEFT, 0, -5);
  
  // Energy card
  lv_obj_t* energy_card = create_stat_card(lv_scr_act(), "CAPACITY", 20 + (card_w + gap)*3, row2_y, card_w, card_h);
  lbl_energy = lv_label_create(energy_card);
  lv_label_set_text(lbl_energy, "-- kWh");
  lv_obj_set_style_text_font(lbl_energy, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(lbl_energy, lv_color_hex(0x7ee787), 0);
  lv_obj_align(lbl_energy, LV_ALIGN_BOTTOM_LEFT, 0, -5);
  
  // ===== THIRD ROW: Cell & Temp Stats (4 cards) =====
  int row3_y = 255;
  
  // Cell Min card
  lv_obj_t* cell_min_card = create_stat_card(lv_scr_act(), "CELL MIN", 20, row3_y, card_w, card_h);
  lbl_cell_min = lv_label_create(cell_min_card);
  lv_label_set_text(lbl_cell_min, "-.--- V");
  lv_obj_set_style_text_font(lbl_cell_min, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(lbl_cell_min, lv_color_hex(0xff7b72), 0);
  lv_obj_align(lbl_cell_min, LV_ALIGN_BOTTOM_LEFT, 0, -5);
  
  // Cell Max card
  lv_obj_t* cell_max_card = create_stat_card(lv_scr_act(), "CELL MAX", 20 + card_w + gap, row3_y, card_w, card_h);
  lbl_cell_max = lv_label_create(cell_max_card);
  lv_label_set_text(lbl_cell_max, "-.--- V");
  lv_obj_set_style_text_font(lbl_cell_max, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(lbl_cell_max, lv_color_hex(0x7ee787), 0);
  lv_obj_align(lbl_cell_max, LV_ALIGN_BOTTOM_LEFT, 0, -5);
  
  // Cell Delta card
  lv_obj_t* delta_card = create_stat_card(lv_scr_act(), "CELL DELTA", 20 + (card_w + gap)*2, row3_y, card_w, card_h);
  lbl_cell_delta = lv_label_create(delta_card);
  lv_label_set_text(lbl_cell_delta, "--- mV");
  lv_obj_set_style_text_font(lbl_cell_delta, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(lbl_cell_delta, lv_color_hex(0xffa657), 0);
  lv_obj_align(lbl_cell_delta, LV_ALIGN_BOTTOM_LEFT, 0, -5);
  
  // Temp card
  lv_obj_t* temp_card = create_stat_card(lv_scr_act(), "TEMPERATURE", 20 + (card_w + gap)*3, row3_y, card_w, card_h);
  lbl_temp = lv_label_create(temp_card);
  lv_label_set_text(lbl_temp, "--.- C");
  lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(lbl_temp, lv_color_hex(0x79c0ff), 0);
  lv_obj_align(lbl_temp, LV_ALIGN_BOTTOM_LEFT, 0, -5);
  
  // ===== FOURTH ROW: System Stats (4 cards) =====
  int row4_y = 365;
  
  // Temp Min card
  lv_obj_t* temp_min_card = create_stat_card(lv_scr_act(), "TEMP MIN", 20, row4_y, card_w, card_h);
  lbl_temp_min = lv_label_create(temp_min_card);
  lv_label_set_text(lbl_temp_min, "--.- C");
  lv_obj_set_style_text_font(lbl_temp_min, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(lbl_temp_min, lv_color_hex(0x79c0ff), 0);
  lv_obj_align(lbl_temp_min, LV_ALIGN_BOTTOM_LEFT, 0, -5);
  
  // Temp Max card
  lv_obj_t* temp_max_card = create_stat_card(lv_scr_act(), "TEMP MAX", 20 + card_w + gap, row4_y, card_w, card_h);
  lbl_temp_max = lv_label_create(temp_max_card);
  lv_label_set_text(lbl_temp_max, "--.- C");
  lv_obj_set_style_text_font(lbl_temp_max, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(lbl_temp_max, lv_color_hex(0xff7b72), 0);
  lv_obj_align(lbl_temp_max, LV_ALIGN_BOTTOM_LEFT, 0, -5);
  
  // Contactor card
  lv_obj_t* contactor_card = create_stat_card(lv_scr_act(), "CONTACTOR", 20 + (card_w + gap)*2, row4_y, card_w, card_h);
  lbl_contactor = lv_label_create(contactor_card);
  lv_label_set_text(lbl_contactor, "OPEN");
  lv_obj_set_style_text_font(lbl_contactor, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(lbl_contactor, lv_color_hex(0xff7b72), 0);
  lv_obj_align(lbl_contactor, LV_ALIGN_BOTTOM_LEFT, 0, -5);
  
  // WiFi card
  lv_obj_t* wifi_card = create_stat_card(lv_scr_act(), "NETWORK", 20 + (card_w + gap)*3, row4_y, card_w, card_h);
  lbl_wifi = lv_label_create(wifi_card);
  lv_label_set_text(lbl_wifi, "Disconnected");
  lv_obj_set_style_text_font(lbl_wifi, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_wifi, lv_color_hex(0x8b949e), 0);
  lv_obj_align(lbl_wifi, LV_ALIGN_BOTTOM_LEFT, 0, -5);
  
  // ===== BOTTOM ROW: Additional Info + Controls =====
  int row5_y = 475;
  int info_w = 380;
  
  // Battery info card
  lv_obj_t* batt_card = create_stat_card(lv_scr_act(), "BATTERY INFO", 20, row5_y, info_w, 115);
  lbl_batt_info = lv_label_create(batt_card);
  lv_label_set_text(lbl_batt_info, "Remaining: -- kWh\nSOH: -- %");
  lv_obj_set_style_text_font(lbl_batt_info, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_batt_info, lv_color_hex(0xc9d1d9), 0);
  lv_obj_set_pos(lbl_batt_info, 0, 22);
  
  // Tesla "T" Logo (right side of battery card) - positioned higher
  int logo_x = info_w - 75;
  int logo_y = 15;
  lv_color_t t_red = lv_color_hex(0xe82127);
  
  // Tesla T emblem: curved arch top + pointed stem
  // Arc/roof shape at top (multiple line segments to simulate curve)
  static lv_point_t arc1[] = {{0, 18}, {8, 8}};      // Left side up
  static lv_point_t arc2[] = {{8, 8}, {20, 2}};      // Left curve to center-left
  static lv_point_t arc3[] = {{20, 2}, {30, 0}};     // Center-left to peak
  static lv_point_t arc4[] = {{30, 0}, {40, 2}};     // Peak to center-right
  static lv_point_t arc5[] = {{40, 2}, {52, 8}};     // Center-right curve
  static lv_point_t arc6[] = {{52, 8}, {60, 18}};    // Right side down
  // Stem (pointed at bottom) - shortened
  static lv_point_t stem1[] = {{26, 12}, {30, 60}};  // Left side of stem
  static lv_point_t stem2[] = {{34, 12}, {30, 60}};  // Right side of stem
  
  lv_obj_t* ba1 = lv_line_create(batt_card); lv_line_set_points(ba1, arc1, 2);
  lv_obj_set_style_line_width(ba1, 5, 0); lv_obj_set_style_line_color(ba1, t_red, 0);
  lv_obj_set_style_line_rounded(ba1, true, 0); lv_obj_set_pos(ba1, logo_x, logo_y);
  
  lv_obj_t* ba2 = lv_line_create(batt_card); lv_line_set_points(ba2, arc2, 2);
  lv_obj_set_style_line_width(ba2, 5, 0); lv_obj_set_style_line_color(ba2, t_red, 0);
  lv_obj_set_style_line_rounded(ba2, true, 0); lv_obj_set_pos(ba2, logo_x, logo_y);
  
  lv_obj_t* ba3 = lv_line_create(batt_card); lv_line_set_points(ba3, arc3, 2);
  lv_obj_set_style_line_width(ba3, 5, 0); lv_obj_set_style_line_color(ba3, t_red, 0);
  lv_obj_set_style_line_rounded(ba3, true, 0); lv_obj_set_pos(ba3, logo_x, logo_y);
  
  lv_obj_t* ba4 = lv_line_create(batt_card); lv_line_set_points(ba4, arc4, 2);
  lv_obj_set_style_line_width(ba4, 5, 0); lv_obj_set_style_line_color(ba4, t_red, 0);
  lv_obj_set_style_line_rounded(ba4, true, 0); lv_obj_set_pos(ba4, logo_x, logo_y);
  
  lv_obj_t* ba5 = lv_line_create(batt_card); lv_line_set_points(ba5, arc5, 2);
  lv_obj_set_style_line_width(ba5, 5, 0); lv_obj_set_style_line_color(ba5, t_red, 0);
  lv_obj_set_style_line_rounded(ba5, true, 0); lv_obj_set_pos(ba5, logo_x, logo_y);
  
  lv_obj_t* ba6 = lv_line_create(batt_card); lv_line_set_points(ba6, arc6, 2);
  lv_obj_set_style_line_width(ba6, 5, 0); lv_obj_set_style_line_color(ba6, t_red, 0);
  lv_obj_set_style_line_rounded(ba6, true, 0); lv_obj_set_pos(ba6, logo_x, logo_y);
  
  lv_obj_t* lst1 = lv_line_create(batt_card); lv_line_set_points(lst1, stem1, 2);
  lv_obj_set_style_line_width(lst1, 5, 0); lv_obj_set_style_line_color(lst1, t_red, 0);
  lv_obj_set_style_line_rounded(lst1, true, 0); lv_obj_set_pos(lst1, logo_x, logo_y);
  
  lv_obj_t* lst2 = lv_line_create(batt_card); lv_line_set_points(lst2, stem2, 2);
  lv_obj_set_style_line_width(lst2, 5, 0); lv_obj_set_style_line_color(lst2, t_red, 0);
  lv_obj_set_style_line_rounded(lst2, true, 0); lv_obj_set_pos(lst2, logo_x, logo_y);
  
  // System info card (wider, split into System | CAN)
  lv_obj_t* sys_card = create_stat_card(lv_scr_act(), "SYSTEM / CAN STATUS", 20 + info_w + gap, row5_y, info_w, 115);
  
  // Left side: System info
  lbl_sys_info = lv_label_create(sys_card);
  lv_label_set_text(lbl_sys_info, "Uptime: --\nHeap: -- KB");
  lv_obj_set_style_text_font(lbl_sys_info, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl_sys_info, lv_color_hex(0xc9d1d9), 0);
  lv_obj_set_pos(lbl_sys_info, 0, 22);
  
  // Divider line
  lv_obj_t* divider = lv_obj_create(sys_card);
  lv_obj_set_size(divider, 2, 80);
  lv_obj_set_pos(divider, info_w/2 - 10, 18);
  lv_obj_set_style_bg_color(divider, lv_color_hex(0x30363d), 0);
  lv_obj_set_style_border_width(divider, 0, 0);
  lv_obj_set_style_radius(divider, 1, 0);
  
  // Right side: CAN status
  lv_obj_t* lbl_can_title = lv_label_create(sys_card);
  lv_label_set_text(lbl_can_title, "CAN Bus");
  lv_obj_set_style_text_font(lbl_can_title, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(lbl_can_title, lv_color_hex(0x8b949e), 0);
  lv_obj_set_pos(lbl_can_title, info_w/2, 22);
  
  lv_obj_t* lbl_can_detail = lv_label_create(sys_card);
  lv_label_set_text(lbl_can_detail, "BATT: --\nINV: --");
  lv_obj_set_style_text_font(lbl_can_detail, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl_can_detail, lv_color_hex(0x8b949e), 0);
  lv_obj_set_pos(lbl_can_detail, info_w/2, 40);
  // Store reference for updates
  lbl_can_status = lbl_can_detail;
  
  // ===== CONTROL BUTTONS =====
  int btn_x = 20 + (info_w + gap) * 2;
  int btn_w = 95;
  int btn_h = 45;
  
  // WiFi AP Toggle Button
  // WiFi AP toggle button
  btn_wifi_ap = lv_btn_create(lv_scr_act());
  lv_obj_set_pos(btn_wifi_ap, btn_x, row5_y);
  lv_obj_set_size(btn_wifi_ap, btn_w, btn_h);
  lv_obj_set_style_bg_color(btn_wifi_ap, lv_color_hex(0x21262d), 0);
  lv_obj_set_style_bg_color(btn_wifi_ap, lv_color_hex(0x30363d), LV_STATE_PRESSED);
  lv_obj_set_style_radius(btn_wifi_ap, 8, 0);
  lv_obj_add_event_cb(btn_wifi_ap, btn_wifi_ap_cb, LV_EVENT_CLICKED, NULL);
  
  lbl_btn_wifi = lv_label_create(btn_wifi_ap);
  lv_label_set_text(lbl_btn_wifi, "AP: OFF");
  lv_obj_set_style_text_font(lbl_btn_wifi, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl_btn_wifi);
  
  // Settings Button
  btn_settings = lv_btn_create(lv_scr_act());
  lv_obj_set_pos(btn_settings, btn_x + btn_w + 10, row5_y);
  lv_obj_set_size(btn_settings, btn_w, btn_h);
  lv_obj_set_style_bg_color(btn_settings, lv_color_hex(0x1f6feb), 0);  // Blue
  lv_obj_set_style_bg_color(btn_settings, lv_color_hex(0x388bfd), LV_STATE_PRESSED);
  lv_obj_set_style_radius(btn_settings, 8, 0);
  lv_obj_add_event_cb(btn_settings, btn_settings_cb, LV_EVENT_CLICKED, NULL);
  
  lv_obj_t* lbl_settings = lv_label_create(btn_settings);
  lv_label_set_text(lbl_settings, "Settings");
  lv_obj_set_style_text_font(lbl_settings, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl_settings);
  
  // Reboot Button
  btn_reboot = lv_btn_create(lv_scr_act());
  lv_obj_set_pos(btn_reboot, btn_x, row5_y + btn_h + 10);
  lv_obj_set_size(btn_reboot, btn_w, btn_h);
  lv_obj_set_style_bg_color(btn_reboot, lv_color_hex(0xda3633), 0);  // Red
  lv_obj_set_style_bg_color(btn_reboot, lv_color_hex(0xf85149), LV_STATE_PRESSED);
  lv_obj_set_style_radius(btn_reboot, 8, 0);
  lv_obj_add_event_cb(btn_reboot, btn_reboot_cb, LV_EVENT_CLICKED, NULL);
  
  lv_obj_t* lbl_reboot = lv_label_create(btn_reboot);
  lv_label_set_text(lbl_reboot, "Reboot");
  lv_obj_set_style_text_font(lbl_reboot, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl_reboot);
  
  // ===== SETTINGS PANEL (overlay, hidden by default) =====
  settings_panel = lv_obj_create(lv_scr_act());
  lv_obj_set_size(settings_panel, 400, 230);
  lv_obj_center(settings_panel);
  lv_obj_set_style_bg_color(settings_panel, lv_color_hex(0x161b22), 0);
  lv_obj_set_style_border_color(settings_panel, lv_color_hex(0x30363d), 0);
  lv_obj_set_style_border_width(settings_panel, 2, 0);
  lv_obj_set_style_radius(settings_panel, 12, 0);
  lv_obj_add_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);  // Start hidden
  lv_obj_clear_flag(settings_panel, LV_OBJ_FLAG_SCROLLABLE);  // No scrolling
  
  // Settings title
  lv_obj_t* settings_title = lv_label_create(settings_panel);
  lv_label_set_text(settings_title, "Settings");
  lv_obj_set_style_text_font(settings_title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(settings_title, lv_color_hex(0xc9d1d9), 0);
  lv_obj_align(settings_title, LV_ALIGN_TOP_MID, 0, 10);
  
  // Close button (X)
  lv_obj_t* btn_close = lv_btn_create(settings_panel);
  lv_obj_set_size(btn_close, 40, 40);
  lv_obj_align(btn_close, LV_ALIGN_TOP_RIGHT, -5, 5);
  lv_obj_set_style_bg_color(btn_close, lv_color_hex(0x21262d), 0);
  lv_obj_set_style_radius(btn_close, 20, 0);
  lv_obj_add_event_cb(btn_close, btn_close_settings_cb, LV_EVENT_CLICKED, NULL);
  
  lv_obj_t* lbl_close = lv_label_create(btn_close);
  lv_label_set_text(lbl_close, "X");
  lv_obj_set_style_text_font(lbl_close, &lv_font_montserrat_16, 0);
  lv_obj_center(lbl_close);
  
  // Brightness label
  lv_obj_t* lbl_bright_title = lv_label_create(settings_panel);
  lv_label_set_text(lbl_bright_title, "Display Brightness");
  lv_obj_set_style_text_font(lbl_bright_title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_bright_title, lv_color_hex(0xc9d1d9), 0);
  lv_obj_set_pos(lbl_bright_title, 20, 60);
  
  // Brightness slider
  brightness_slider = lv_slider_create(settings_panel);
  lv_obj_set_size(brightness_slider, 300, 20);
  lv_obj_set_pos(brightness_slider, 20, 90);
  lv_slider_set_range(brightness_slider, 10, 100);  // Min 10% to keep visible
  lv_slider_set_value(brightness_slider, 100, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0x21262d), LV_PART_MAIN);
  lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0x1f6feb), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0xc9d1d9), LV_PART_KNOB);
  lv_obj_add_event_cb(brightness_slider, brightness_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
  
  // Brightness value label
  lbl_brightness = lv_label_create(settings_panel);
  lv_label_set_text(lbl_brightness, "100%");
  lv_obj_set_style_text_font(lbl_brightness, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_brightness, lv_color_hex(0x58a6ff), 0);
  lv_obj_set_pos(lbl_brightness, 330, 87);
  
  // Backup Battery section
  lv_obj_t* lbl_batt_title = lv_label_create(settings_panel);
  lv_label_set_text(lbl_batt_title, "Backup Battery");
  lv_obj_set_style_text_font(lbl_batt_title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_batt_title, lv_color_hex(0xc9d1d9), 0);
  lv_obj_set_pos(lbl_batt_title, 20, 120);
  
  lbl_backup_battery = lv_label_create(settings_panel);
  lv_label_set_text(lbl_backup_battery, "-- V");
  lv_obj_set_style_text_font(lbl_backup_battery, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_backup_battery, lv_color_hex(0x7ee787), 0);
  lv_obj_set_pos(lbl_backup_battery, 180, 120);
  
  // Version info - brighter text
  lv_obj_t* version_info = lv_label_create(settings_panel);
  static char ver_text[96];
  snprintf(ver_text, sizeof(ver_text), "Firmware: Battery Emulator\nUI Version: %s\nBoard: Waveshare ESP32-S3-7B", UI_VERSION);
  lv_label_set_text(version_info, ver_text);
  lv_obj_set_style_text_font(version_info, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(version_info, lv_color_hex(0x8b949e), 0);
  lv_obj_set_pos(version_info, 20, 155);
  
  // ===== REBOOT CONFIRMATION PANEL =====
  reboot_confirm_panel = lv_obj_create(lv_scr_act());
  lv_obj_set_size(reboot_confirm_panel, 300, 150);
  lv_obj_center(reboot_confirm_panel);
  lv_obj_set_style_bg_color(reboot_confirm_panel, lv_color_hex(0x161b22), 0);
  lv_obj_set_style_border_color(reboot_confirm_panel, lv_color_hex(0xda3633), 0);
  lv_obj_set_style_border_width(reboot_confirm_panel, 2, 0);
  lv_obj_set_style_radius(reboot_confirm_panel, 12, 0);
  lv_obj_add_flag(reboot_confirm_panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(reboot_confirm_panel, LV_OBJ_FLAG_SCROLLABLE);
  
  lv_obj_t* reboot_title = lv_label_create(reboot_confirm_panel);
  lv_label_set_text(reboot_title, "Confirm Reboot?");
  lv_obj_set_style_text_font(reboot_title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(reboot_title, lv_color_hex(0xff7b72), 0);
  lv_obj_align(reboot_title, LV_ALIGN_TOP_MID, 0, 10);
  
  lv_obj_t* reboot_msg = lv_label_create(reboot_confirm_panel);
  lv_label_set_text(reboot_msg, "System will restart");
  lv_obj_set_style_text_font(reboot_msg, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(reboot_msg, lv_color_hex(0x8b949e), 0);
  lv_obj_align(reboot_msg, LV_ALIGN_TOP_MID, 0, 45);
  
  // Cancel button
  lv_obj_t* btn_cancel = lv_btn_create(reboot_confirm_panel);
  lv_obj_set_size(btn_cancel, 100, 40);
  lv_obj_set_pos(btn_cancel, 30, 85);
  lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x21262d), 0);
  lv_obj_set_style_radius(btn_cancel, 8, 0);
  lv_obj_add_event_cb(btn_cancel, btn_reboot_cancel_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* lbl_cancel = lv_label_create(btn_cancel);
  lv_label_set_text(lbl_cancel, "Cancel");
  lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl_cancel);
  
  // Confirm button
  lv_obj_t* btn_confirm = lv_btn_create(reboot_confirm_panel);
  lv_obj_set_size(btn_confirm, 100, 40);
  lv_obj_set_pos(btn_confirm, 160, 85);
  lv_obj_set_style_bg_color(btn_confirm, lv_color_hex(0xda3633), 0);
  lv_obj_set_style_radius(btn_confirm, 8, 0);
  lv_obj_add_event_cb(btn_confirm, btn_reboot_confirm_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* lbl_confirm = lv_label_create(btn_confirm);
  lv_label_set_text(lbl_confirm, "Reboot");
  lv_obj_set_style_text_font(lbl_confirm, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl_confirm);
  
  // ===== WIFI SETTINGS PANEL =====
  wifi_panel = lv_obj_create(lv_scr_act());
  lv_obj_set_size(wifi_panel, 400, 240);
  lv_obj_center(wifi_panel);
  lv_obj_set_style_bg_color(wifi_panel, lv_color_hex(0x161b22), 0);
  lv_obj_set_style_border_color(wifi_panel, lv_color_hex(0x1f6feb), 0);
  lv_obj_set_style_border_width(wifi_panel, 2, 0);
  lv_obj_set_style_radius(wifi_panel, 12, 0);
  lv_obj_add_flag(wifi_panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(wifi_panel, LV_OBJ_FLAG_SCROLLABLE);
  
  // WiFi panel title
  lv_obj_t* wifi_title = lv_label_create(wifi_panel);
  lv_label_set_text(wifi_title, "WiFi Settings");
  lv_obj_set_style_text_font(wifi_title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(wifi_title, lv_color_hex(0xc9d1d9), 0);
  lv_obj_align(wifi_title, LV_ALIGN_TOP_MID, 0, 10);
  
  // Close button (X)
  lv_obj_t* btn_wifi_close = lv_btn_create(wifi_panel);
  lv_obj_set_size(btn_wifi_close, 40, 40);
  lv_obj_align(btn_wifi_close, LV_ALIGN_TOP_RIGHT, -5, 5);
  lv_obj_set_style_bg_color(btn_wifi_close, lv_color_hex(0x21262d), 0);
  lv_obj_set_style_radius(btn_wifi_close, 20, 0);
  lv_obj_add_event_cb(btn_wifi_close, btn_wifi_close_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* lbl_wifi_close = lv_label_create(btn_wifi_close);
  lv_label_set_text(lbl_wifi_close, "X");
  lv_obj_set_style_text_font(lbl_wifi_close, &lv_font_montserrat_16, 0);
  lv_obj_center(lbl_wifi_close);
  
  // Status label
  lv_obj_t* lbl_status_title = lv_label_create(wifi_panel);
  lv_label_set_text(lbl_status_title, "Status:");
  lv_obj_set_style_text_font(lbl_status_title, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl_status_title, lv_color_hex(0x8b949e), 0);
  lv_obj_set_pos(lbl_status_title, 20, 50);
  
  lbl_wifi_status = lv_label_create(wifi_panel);
  lv_label_set_text(lbl_wifi_status, "AP: OFF");
  lv_obj_set_style_text_font(lbl_wifi_status, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_wifi_status, lv_color_hex(0xc9d1d9), 0);
  lv_obj_set_pos(lbl_wifi_status, 100, 48);
  
  // TX Power label and controls
  lv_obj_t* lbl_power_title = lv_label_create(wifi_panel);
  lv_label_set_text(lbl_power_title, "TX Power:");
  lv_obj_set_style_text_font(lbl_power_title, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl_power_title, lv_color_hex(0x8b949e), 0);
  lv_obj_set_pos(lbl_power_title, 20, 85);
  
  // Power down button
  lv_obj_t* btn_power_down = lv_btn_create(wifi_panel);
  lv_obj_set_size(btn_power_down, 40, 30);
  lv_obj_set_pos(btn_power_down, 100, 80);
  lv_obj_set_style_bg_color(btn_power_down, lv_color_hex(0x21262d), 0);
  lv_obj_add_event_cb(btn_power_down, btn_wifi_power_down_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* lbl_pd = lv_label_create(btn_power_down);
  lv_label_set_text(lbl_pd, "-");
  lv_obj_set_style_text_font(lbl_pd, &lv_font_montserrat_16, 0);
  lv_obj_center(lbl_pd);
  
  // Power value label
  lbl_wifi_power = lv_label_create(wifi_panel);
  lv_label_set_text(lbl_wifi_power, wifi_power_names[wifi_tx_power]);
  lv_obj_set_style_text_font(lbl_wifi_power, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl_wifi_power, lv_color_hex(0xc9d1d9), 0);
  lv_obj_set_pos(lbl_wifi_power, 150, 85);
  lv_obj_set_width(lbl_wifi_power, 120);
  
  // Power up button
  lv_obj_t* btn_power_up = lv_btn_create(wifi_panel);
  lv_obj_set_size(btn_power_up, 40, 30);
  lv_obj_set_pos(btn_power_up, 280, 80);
  lv_obj_set_style_bg_color(btn_power_up, lv_color_hex(0x21262d), 0);
  lv_obj_add_event_cb(btn_power_up, btn_wifi_power_up_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* lbl_pu = lv_label_create(btn_power_up);
  lv_label_set_text(lbl_pu, "+");
  lv_obj_set_style_text_font(lbl_pu, &lv_font_montserrat_16, 0);
  lv_obj_center(lbl_pu);
  
  // Enable AP button
  lv_obj_t* btn_wifi_enable = lv_btn_create(wifi_panel);
  lv_obj_set_size(btn_wifi_enable, 160, 45);
  lv_obj_set_pos(btn_wifi_enable, 20, 130);
  lv_obj_set_style_bg_color(btn_wifi_enable, lv_color_hex(0x238636), 0);
  lv_obj_set_style_radius(btn_wifi_enable, 8, 0);
  lv_obj_add_event_cb(btn_wifi_enable, btn_wifi_enable_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* lbl_enable = lv_label_create(btn_wifi_enable);
  lv_label_set_text(lbl_enable, "Enable AP");
  lv_obj_set_style_text_font(lbl_enable, &lv_font_montserrat_16, 0);
  lv_obj_center(lbl_enable);
  
  // Disable AP button
  lv_obj_t* btn_wifi_disable = lv_btn_create(wifi_panel);
  lv_obj_set_size(btn_wifi_disable, 160, 45);
  lv_obj_set_pos(btn_wifi_disable, 200, 130);
  lv_obj_set_style_bg_color(btn_wifi_disable, lv_color_hex(0xda3633), 0);
  lv_obj_set_style_radius(btn_wifi_disable, 8, 0);
  lv_obj_add_event_cb(btn_wifi_disable, btn_wifi_disable_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* lbl_disable = lv_label_create(btn_wifi_disable);
  lv_label_set_text(lbl_disable, "Disable AP");
  lv_obj_set_style_text_font(lbl_disable, &lv_font_montserrat_16, 0);
  lv_obj_center(lbl_disable);
  
  // Note about display freeze
  lv_obj_t* lbl_note = lv_label_create(wifi_panel);
  lv_label_set_text(lbl_note, "Display pauses briefly during WiFi changes");
  lv_obj_set_style_text_font(lbl_note, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(lbl_note, lv_color_hex(0x8b949e), 0);
  lv_obj_set_pos(lbl_note, 20, 190);
  
  // ===== WIFI CONFIRM PANEL =====
  wifi_confirm_panel = lv_obj_create(lv_scr_act());
  lv_obj_set_size(wifi_confirm_panel, 320, 160);
  lv_obj_center(wifi_confirm_panel);
  lv_obj_set_style_bg_color(wifi_confirm_panel, lv_color_hex(0x161b22), 0);
  lv_obj_set_style_border_color(wifi_confirm_panel, lv_color_hex(0xf0883e), 0);
  lv_obj_set_style_border_width(wifi_confirm_panel, 2, 0);
  lv_obj_set_style_radius(wifi_confirm_panel, 12, 0);
  lv_obj_add_flag(wifi_confirm_panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(wifi_confirm_panel, LV_OBJ_FLAG_SCROLLABLE);
  
  lv_obj_t* wifi_conf_title = lv_label_create(wifi_confirm_panel);
  lv_label_set_text(wifi_conf_title, "Enable WiFi AP?");
  lv_obj_set_style_text_font(wifi_conf_title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(wifi_conf_title, lv_color_hex(0xf0883e), 0);
  lv_obj_align(wifi_conf_title, LV_ALIGN_TOP_MID, 0, 15);
  
  lv_obj_t* wifi_conf_msg = lv_label_create(wifi_confirm_panel);
  lv_label_set_text(wifi_conf_msg, "Display will pause during enable");
  lv_obj_set_style_text_font(wifi_conf_msg, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(wifi_conf_msg, lv_color_hex(0x8b949e), 0);
  lv_obj_align(wifi_conf_msg, LV_ALIGN_TOP_MID, 0, 45);
  
  lv_obj_t* btn_wifi_cancel = lv_btn_create(wifi_confirm_panel);
  lv_obj_set_size(btn_wifi_cancel, 100, 40);
  lv_obj_set_pos(btn_wifi_cancel, 30, 90);
  lv_obj_set_style_bg_color(btn_wifi_cancel, lv_color_hex(0x21262d), 0);
  lv_obj_set_style_radius(btn_wifi_cancel, 8, 0);
  lv_obj_add_event_cb(btn_wifi_cancel, btn_wifi_confirm_cancel_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* lbl_wifi_cancel = lv_label_create(btn_wifi_cancel);
  lv_label_set_text(lbl_wifi_cancel, "Cancel");
  lv_obj_set_style_text_font(lbl_wifi_cancel, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl_wifi_cancel);
  
  lv_obj_t* btn_wifi_confirm = lv_btn_create(wifi_confirm_panel);
  lv_obj_set_size(btn_wifi_confirm, 100, 40);
  lv_obj_set_pos(btn_wifi_confirm, 170, 90);
  lv_obj_set_style_bg_color(btn_wifi_confirm, lv_color_hex(0x238636), 0);
  lv_obj_set_style_radius(btn_wifi_confirm, 8, 0);
  lv_obj_add_event_cb(btn_wifi_confirm, btn_wifi_confirm_enable_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* lbl_wifi_confirm = lv_label_create(btn_wifi_confirm);
  lv_label_set_text(lbl_wifi_confirm, "Enable");
  lv_obj_set_style_text_font(lbl_wifi_confirm, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl_wifi_confirm);
  
  // ===== CELLS SCREEN (overlay, hidden by default) =====
  screen_cells = lv_obj_create(lv_scr_act());
  lv_obj_set_size(screen_cells, 984, 550);
  lv_obj_set_pos(screen_cells, 20, 45);
  lv_obj_set_style_bg_color(screen_cells, lv_color_hex(0x0d1117), 0);
  lv_obj_set_style_border_width(screen_cells, 0, 0);
  lv_obj_set_style_radius(screen_cells, 0, 0);
  lv_obj_add_flag(screen_cells, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_scrollbar_mode(screen_cells, LV_SCROLLBAR_MODE_AUTO);
  
  // Cells title
  lbl_cells_title = lv_label_create(screen_cells);
  lv_label_set_text(lbl_cells_title, "CELL VOLTAGES (0 cells)");
  lv_obj_set_style_text_font(lbl_cells_title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_cells_title, lv_color_hex(0x00d4ff), 0);
  lv_obj_set_pos(lbl_cells_title, 0, 0);
  
  // Min/Max tracking display
  lbl_minmax_voltage = lv_label_create(screen_cells);
  lv_label_set_text(lbl_minmax_voltage, "Pack V: Min -- / Max --");
  lv_obj_set_style_text_font(lbl_minmax_voltage, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl_minmax_voltage, lv_color_hex(0x8b949e), 0);
  lv_obj_set_pos(lbl_minmax_voltage, 0, 25);
  
  lbl_minmax_temp = lv_label_create(screen_cells);
  lv_label_set_text(lbl_minmax_temp, "Temp: Min -- / Max --");
  lv_obj_set_style_text_font(lbl_minmax_temp, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl_minmax_temp, lv_color_hex(0x8b949e), 0);
  lv_obj_set_pos(lbl_minmax_temp, 300, 25);
  
  lbl_minmax_cell = lv_label_create(screen_cells);
  lv_label_set_text(lbl_minmax_cell, "Cell V: Min -- / Max --");
  lv_obj_set_style_text_font(lbl_minmax_cell, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl_minmax_cell, lv_color_hex(0x8b949e), 0);
  lv_obj_set_pos(lbl_minmax_cell, 600, 25);
  
  // Create cell voltage grid (12 columns x 9 rows = 108 cells max)
  int cell_w = 75, cell_h = 45;
  int cells_per_row = 12;
  for (int i = 0; i < MAX_CELLS; i++) {
    int col = i % cells_per_row;
    int row = i / cells_per_row;
    cell_labels[i] = lv_label_create(screen_cells);
    lv_label_set_text(cell_labels[i], "--");
    lv_obj_set_style_text_font(cell_labels[i], &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(cell_labels[i], lv_color_hex(0x6e7681), 0);
    lv_obj_set_pos(cell_labels[i], col * cell_w, 55 + row * cell_h);
  }
  
  // ===== ALERTS SCREEN (overlay, hidden by default) =====
  screen_alerts = lv_obj_create(lv_scr_act());
  lv_obj_set_size(screen_alerts, 984, 550);
  lv_obj_set_pos(screen_alerts, 20, 45);
  lv_obj_set_style_bg_color(screen_alerts, lv_color_hex(0x0d1117), 0);
  lv_obj_set_style_border_width(screen_alerts, 0, 0);
  lv_obj_set_style_radius(screen_alerts, 0, 0);
  lv_obj_add_flag(screen_alerts, LV_OBJ_FLAG_HIDDEN);
  
  // Active alerts section
  lbl_alerts = lv_label_create(screen_alerts);
  lv_label_set_text(lbl_alerts, "ACTIVE ALERTS\n\nNo active alerts");
  lv_obj_set_style_text_font(lbl_alerts, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_alerts, lv_color_hex(0x7ee787), 0);
  lv_obj_set_pos(lbl_alerts, 0, 0);
  lv_obj_set_width(lbl_alerts, 450);
  
  // Event log section
  lv_obj_t* events_title = lv_label_create(screen_alerts);
  lv_label_set_text(events_title, "EVENT LOG");
  lv_obj_set_style_text_font(events_title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(events_title, lv_color_hex(0x00d4ff), 0);
  lv_obj_set_pos(events_title, 500, 0);
  
  lbl_event_list = lv_label_create(screen_alerts);
  lv_label_set_text(lbl_event_list, "(no events)");
  lv_obj_set_style_text_font(lbl_event_list, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(lbl_event_list, lv_color_hex(0x8b949e), 0);
  lv_obj_set_pos(lbl_event_list, 500, 25);
  lv_obj_set_width(lbl_event_list, 480);
  
  // Initialize event log
  for (int i = 0; i < MAX_EVENTS; i++) {
    event_log[i][0] = '\0';
  }
  add_event("System started");
  
  // Mark main screen container (the default active screen)
  screen_main = NULL;  // Main content uses lv_scr_act() directly
  
  // ===== TAB BUTTONS (top right) =====
  int tab_w = 80, tab_h = 30;
  int tab_x = 700;
  
  tab_btns[0] = lv_btn_create(lv_scr_act());
  lv_obj_set_pos(tab_btns[0], tab_x, 8);
  lv_obj_set_size(tab_btns[0], tab_w, tab_h);
  lv_obj_set_style_bg_color(tab_btns[0], lv_color_hex(0x1f6feb), 0);  // Active
  lv_obj_set_style_radius(tab_btns[0], 6, 0);
  lv_obj_add_event_cb(tab_btns[0], show_screen_main, LV_EVENT_CLICKED, NULL);
  lv_obj_t* lbl_tab0 = lv_label_create(tab_btns[0]);
  lv_label_set_text(lbl_tab0, "Main");
  lv_obj_set_style_text_font(lbl_tab0, &lv_font_montserrat_12, 0);
  lv_obj_center(lbl_tab0);
  
  tab_btns[1] = lv_btn_create(lv_scr_act());
  lv_obj_set_pos(tab_btns[1], tab_x + tab_w + 5, 8);
  lv_obj_set_size(tab_btns[1], tab_w, tab_h);
  lv_obj_set_style_bg_color(tab_btns[1], lv_color_hex(0x21262d), 0);
  lv_obj_set_style_radius(tab_btns[1], 6, 0);
  lv_obj_add_event_cb(tab_btns[1], show_screen_cells, LV_EVENT_CLICKED, NULL);
  lv_obj_t* lbl_tab1 = lv_label_create(tab_btns[1]);
  lv_label_set_text(lbl_tab1, "Cells");
  lv_obj_set_style_text_font(lbl_tab1, &lv_font_montserrat_12, 0);
  lv_obj_center(lbl_tab1);
  
  tab_btns[2] = lv_btn_create(lv_scr_act());
  lv_obj_set_pos(tab_btns[2], tab_x + (tab_w + 5) * 2, 8);
  lv_obj_set_size(tab_btns[2], tab_w, tab_h);
  lv_obj_set_style_bg_color(tab_btns[2], lv_color_hex(0x21262d), 0);
  lv_obj_set_style_radius(tab_btns[2], 6, 0);
  lv_obj_add_event_cb(tab_btns[2], show_screen_alerts, LV_EVENT_CLICKED, NULL);
  lv_obj_t* lbl_tab2 = lv_label_create(tab_btns[2]);
  lv_label_set_text(lbl_tab2, "Alerts");
  lv_obj_set_style_text_font(lbl_tab2, &lv_font_montserrat_12, 0);
  lv_obj_center(lbl_tab2);
}

void init_display() {
  DEBUG_PRINTF("Initializing Waveshare 7B display (1024x600)...\n");
  
  // Initialize I2C using Waveshare driver
  DEV_I2C_Init();
  DEBUG_PRINTF("I2C initialized\n");
  
  // Initialize IO expander
  IO_EXTENSION_Init();
  DEBUG_PRINTF("IO expander initialized\n");
  
  // Initialize LCD panel using Waveshare native driver
  panel_handle = waveshare_esp32_s3_rgb_lcd_init();
  if (!panel_handle) {
    DEBUG_PRINTF("ERROR: LCD panel initialization failed!\n");
    return;
  }
  DEBUG_PRINTF("LCD panel initialized\n");
  
  // Turn on backlight (no PWM - PWM can cause visible flicker)
  wavesahre_rgb_lcd_bl_on();
  // Skip PWM control - just use digital on/off for backlight
  DEBUG_PRINTF("Backlight enabled (no PWM)\n");
  
  // Enable CAN mode (EXIO5=HIGH routes GPIO19/20 to CAN transceiver instead of USB)
  // Must be done before LVGL starts to avoid display issues
  IO_EXTENSION_Output(IO_EXTENSION_IO_5, 1);
  DEBUG_PRINTF("CAN mode enabled (EXIO5=HIGH)\n");
  
  // Initialize LVGL
  lv_init();
  
  // Allocate double partial draw buffers in PSRAM (100 lines each)
  // Using separate buffers from panel to avoid conflicts with LCD scanning
  size_t buf_size = SCREEN_WIDTH * 100 * sizeof(lv_color_t);
  lvgl_buf1 = (lv_color_t*)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
  lvgl_buf2 = (lv_color_t*)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
  if (!lvgl_buf1 || !lvgl_buf2) {
    DEBUG_PRINTF("ERROR: Failed to allocate LVGL draw buffers!\n");
    return;
  }
  DEBUG_PRINTF("Allocated 2x %d KB LVGL draw buffers\n", buf_size / 1024);
  
  // Double buffer mode for smooth rendering
  lv_disp_draw_buf_init(&draw_buf, lvgl_buf1, lvgl_buf2, SCREEN_WIDTH * 100);
  
  // Initialize LVGL display driver
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = SCREEN_WIDTH;
  disp_drv.ver_res = SCREEN_HEIGHT;
  disp_drv.flush_cb = disp_flush;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.full_refresh = 0;   // Partial refresh - only redraw changed areas
  disp_drv.direct_mode = 0;    // Not direct mode - use separate buffers
  lv_disp_drv_register(&disp_drv);
  DEBUG_PRINTF("LVGL display driver registered (partial refresh, double buffer)\n");
  
  // Initialize touch
  init_touch();
  
  // Register touch input device with LVGL
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touchpad_read;
  lv_indev_drv_register(&indev_drv);
  
  // Create UI
  create_ui();
  
  // Initialize touch timer for auto-dim
  lastTouchMillis = millis();
  
  display_initialized = true;
  DEBUG_PRINTF("Display initialization complete!\n");
}

// Track last LVGL handler time
static unsigned long lastLvglMillis = 0;

void update_display() {
  if (!display_initialized) return;
  
  // Skip all display updates while frozen (during WiFi transitions)
  if (display_frozen) return;
  
  // Rate-limit LVGL timer handler to ~30fps to reduce flicker
  // Running too frequently can cause excessive redraws
  if (millis() - lastLvglMillis >= 33) {
    lastLvglMillis = millis();
    lv_timer_handler();
  }
  
  // Update UI values every 2000ms to minimize flicker
  if (millis() - lastUpdateMillis > 2000) {
    lastUpdateMillis = millis();
    
    // Update SOC
    uint8_t soc = datalayer.battery.status.reported_soc / 100;
    static char soc_text[16];
    snprintf(soc_text, sizeof(soc_text), "%d %%", soc);
    lv_label_set_text(lbl_soc, soc_text);
    lv_bar_set_value(bar_soc, soc, LV_ANIM_OFF);
    
    // Color SOC bar and text based on level
    uint32_t soc_color = (soc < 20) ? 0xff7b72 : (soc < 50) ? 0xffa657 : 0x7ee787;
    lv_obj_set_style_bg_color(bar_soc, lv_color_hex(soc_color), LV_PART_INDICATOR);
    lv_obj_set_style_text_color(lbl_soc, lv_color_hex(soc_color), 0);
    
    // Update voltage
    float voltage = datalayer.battery.status.voltage_dV / 10.0f;
    static char volt_text[16];
    snprintf(volt_text, sizeof(volt_text), "%.1f V", voltage);
    lv_label_set_text(lbl_voltage, volt_text);
    
    // Update current
    float current = datalayer.battery.status.current_dA / 10.0f;
    static char curr_text[16];
    snprintf(curr_text, sizeof(curr_text), "%.1f A", current);
    lv_label_set_text(lbl_current, curr_text);
    
    // Update power
    int32_t power = (int32_t)(voltage * current);
    static char pwr_text[16];
    if (abs(power) >= 1000) {
      snprintf(pwr_text, sizeof(pwr_text), "%.1f kW", power / 1000.0f);
    } else {
      snprintf(pwr_text, sizeof(pwr_text), "%ld W", (long)power);
    }
    lv_label_set_text(lbl_power, pwr_text);
    
    // Update energy/capacity
    float remaining_wh = datalayer.battery.status.remaining_capacity_Wh;
    static char energy_text[16];
    snprintf(energy_text, sizeof(energy_text), "%.1f kWh", remaining_wh / 1000.0f);
    lv_label_set_text(lbl_energy, energy_text);
    
    // Update cell voltages
    float cell_min_v = datalayer.battery.status.cell_min_voltage_mV / 1000.0f;
    float cell_max_v = datalayer.battery.status.cell_max_voltage_mV / 1000.0f;
    static char cell_min_text[16], cell_max_text[16];
    snprintf(cell_min_text, sizeof(cell_min_text), "%.3f V", cell_min_v);
    snprintf(cell_max_text, sizeof(cell_max_text), "%.3f V", cell_max_v);
    lv_label_set_text(lbl_cell_min, cell_min_text);
    lv_label_set_text(lbl_cell_max, cell_max_text);
    
    // Update cell delta
    uint16_t cell_delta = datalayer.battery.status.cell_max_voltage_mV - 
                          datalayer.battery.status.cell_min_voltage_mV;
    static char delta_text[16];
    snprintf(delta_text, sizeof(delta_text), "%d mV", cell_delta);
    lv_label_set_text(lbl_cell_delta, delta_text);
    // Color delta based on health (< 50mV good, > 100mV concerning)
    uint32_t delta_color = (cell_delta < 50) ? 0x7ee787 : (cell_delta < 100) ? 0xffa657 : 0xff7b72;
    lv_obj_set_style_text_color(lbl_cell_delta, lv_color_hex(delta_color), 0);
    
    // Update temperatures
    float temp_max = datalayer.battery.status.temperature_max_dC / 10.0f;
    float temp_min = datalayer.battery.status.temperature_min_dC / 10.0f;
    float temp_avg = (temp_max + temp_min) / 2.0f;
    static char temp_text[16], temp_min_text[16], temp_max_text[16];
    snprintf(temp_text, sizeof(temp_text), "%.1f C", temp_avg);
    snprintf(temp_min_text, sizeof(temp_min_text), "%.1f C", temp_min);
    snprintf(temp_max_text, sizeof(temp_max_text), "%.1f C", temp_max);
    lv_label_set_text(lbl_temp, temp_text);
    lv_label_set_text(lbl_temp_min, temp_min_text);
    lv_label_set_text(lbl_temp_max, temp_max_text);
    
    // Update status
    if (datalayer.battery.status.bms_status == ACTIVE) {
      lv_label_set_text(lbl_status, "ACTIVE");
      lv_obj_set_style_text_color(lbl_status, lv_color_hex(0x7ee787), 0);
    } else if (datalayer.battery.status.bms_status == FAULT) {
      lv_label_set_text(lbl_status, "FAULT");
      lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xff7b72), 0);
    } else {
      lv_label_set_text(lbl_status, "STANDBY");
      lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xffa657), 0);
    }
    
    // Update contactor status
    if (datalayer.system.status.contactors_engaged) {
      lv_label_set_text(lbl_contactor, "CLOSED");
      lv_obj_set_style_text_color(lbl_contactor, lv_color_hex(0x7ee787), 0);
    } else {
      lv_label_set_text(lbl_contactor, "OPEN");
      lv_obj_set_style_text_color(lbl_contactor, lv_color_hex(0xff7b72), 0);
    }
    
    // Update WiFi/Network status
    if (wifi_ap_enabled) {
      // Show AP info when AP is enabled
      lv_label_set_text(lbl_wifi, "AP: BatteryEmulator\nPass: 12345678\nIP: 192.168.4.1");
      lv_obj_set_style_text_color(lbl_wifi, lv_color_hex(0x58a6ff), 0);  // Blue for AP mode
    } else if (WiFi.status() == WL_CONNECTED) {
      static char wifi_text[64];
      snprintf(wifi_text, sizeof(wifi_text), "%s\n%s", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
      lv_label_set_text(lbl_wifi, wifi_text);
      lv_obj_set_style_text_color(lbl_wifi, lv_color_hex(0x7ee787), 0);  // Green for connected
    } else {
      lv_label_set_text(lbl_wifi, "WiFi Off");
      lv_obj_set_style_text_color(lbl_wifi, lv_color_hex(0x8b949e), 0);
    }
    
    // Update battery info
    static char batt_info[96];
    snprintf(batt_info, sizeof(batt_info), "Total: %.1f kWh\nRemaining: %.1f kWh\nSOH: %d %%",
             datalayer.battery.info.total_capacity_Wh / 1000.0f,
             datalayer.battery.status.remaining_capacity_Wh / 1000.0f,
             datalayer.battery.status.soh_pptt / 100);
    lv_label_set_text(lbl_batt_info, batt_info);
    
    // Update system info (left side of split card)
    static char sys_info[64];
    unsigned long uptime_sec = millis() / 1000;
    unsigned long hours = uptime_sec / 3600;
    unsigned long mins = (uptime_sec % 3600) / 60;
    snprintf(sys_info, sizeof(sys_info), "Uptime: %luh %lum\nHeap: %lu KB",
             hours, mins,
             (unsigned long)(ESP.getFreeHeap() / 1024));
    lv_label_set_text(lbl_sys_info, sys_info);
    
    // Update CAN status (detailed view in system card)
    bool can_batt_ok = datalayer.battery.status.CAN_battery_still_alive > 0;
    bool can_inv_ok = datalayer.system.status.CAN_inverter_still_alive > 0;
    
    static char can_detail[48];
    snprintf(can_detail, sizeof(can_detail), "BATT: %s\nINV: %s",
             can_batt_ok ? "OK" : "--",
             can_inv_ok ? "OK" : "--");
    lv_label_set_text(lbl_can_status, can_detail);
    
    // Color based on overall status
    if (can_batt_ok && can_inv_ok) {
      lv_obj_set_style_text_color(lbl_can_status, lv_color_hex(0x7ee787), 0);  // Green
    } else if (can_batt_ok || can_inv_ok) {
      lv_obj_set_style_text_color(lbl_can_status, lv_color_hex(0xffa657), 0);  // Yellow
    } else {
      lv_obj_set_style_text_color(lbl_can_status, lv_color_hex(0x8b949e), 0);  // Gray
    }
    
    // Mirror contactor/precharge state to IO expander (EXIO0, EXIO7) for relays/LEDs
    // EXIO0 = main contactors engaged, EXIO7 = precharge active or completed
    {
      uint8_t ex0 = datalayer.system.status.contactors_engaged ? 1 : 0;
      uint8_t ex7 = (datalayer.system.status.precharge_status == AUTO_PRECHARGE_PRECHARGING ||
                     datalayer.system.status.precharge_status == AUTO_PRECHARGE_COMPLETED ||
                     datalayer.system.status.contactors_engaged) ? 1 : 0;
      IO_EXTENSION_Output(IO_EXTENSION_IO_0, ex0);
      IO_EXTENSION_Output(IO_EXTENSION_IO_7, ex7);
    }
    
    // Update backup battery voltage (screen's built-in battery)
    if (lbl_backup_battery) {
      uint32_t batt_mv = read_backup_battery_mv();
      static char batt_text[24];
      snprintf(batt_text, sizeof(batt_text), "%.2f V", batt_mv / 1000.0f);
      lv_label_set_text(lbl_backup_battery, batt_text);
      
      // Color based on voltage (3.7V nominal, 4.2V full, 3.0V low)
      uint32_t batt_color;
      if (batt_mv >= 3900) {
        batt_color = 0x7ee787;  // Green - good/full
      } else if (batt_mv >= 3500) {
        batt_color = 0xffa657;  // Orange - medium
      } else if (batt_mv > 0) {
        batt_color = 0xff7b72;  // Red - low
      } else {
        batt_color = 0x8b949e;  // Gray - not connected
      }
      lv_obj_set_style_text_color(lbl_backup_battery, lv_color_hex(batt_color), 0);
    }
    
    // Update min/max tracking (reusing voltage, temp_max, temp_min, cell_min_v, cell_max_v from above)
    if (voltage > 0 && voltage < voltage_min_ever) voltage_min_ever = voltage;
    if (voltage > voltage_max_ever) voltage_max_ever = voltage;
    if (temp_min > -40 && temp_min < temp_min_ever) temp_min_ever = temp_min;
    if (temp_max > temp_max_ever) temp_max_ever = temp_max;
    if (cell_min_v > 0 && cell_min_v < cell_min_ever) cell_min_ever = cell_min_v;
    if (cell_max_v > cell_max_ever) cell_max_ever = cell_max_v;
    
    // Update cells screen min/max labels
    static char minmax_v[48], minmax_t[48], minmax_c[48];
    snprintf(minmax_v, sizeof(minmax_v), "Pack V: Min %.1f / Max %.1f", voltage_min_ever, voltage_max_ever);
    snprintf(minmax_t, sizeof(minmax_t), "Temp: Min %.1f / Max %.1f C", temp_min_ever, temp_max_ever);
    snprintf(minmax_c, sizeof(minmax_c), "Cell V: Min %.3f / Max %.3f", cell_min_ever, cell_max_ever);
    lv_label_set_text(lbl_minmax_voltage, minmax_v);
    lv_label_set_text(lbl_minmax_temp, minmax_t);
    lv_label_set_text(lbl_minmax_cell, minmax_c);
    
    // Update cell voltage grid (only when cells screen visible)
    if (current_screen == 1) {
      uint16_t cell_count = datalayer.battery.info.number_of_cells;
      static char cells_title[32];
      snprintf(cells_title, sizeof(cells_title), "CELL VOLTAGES (%d cells)", cell_count);
      lv_label_set_text(lbl_cells_title, cells_title);
      
      for (int i = 0; i < MAX_CELLS; i++) {
        if (i < cell_count && datalayer.battery.status.cell_voltages_mV[i] > 0) {
          float cv = datalayer.battery.status.cell_voltages_mV[i] / 1000.0f;
          static char cv_text[12];
          snprintf(cv_text, sizeof(cv_text), "%.3f", cv);
          lv_label_set_text(cell_labels[i], cv_text);
          // Color code: green=good, yellow=low, red=critical
          uint32_t cv_color = (cv < 3.0f) ? 0xff7b72 : (cv < 3.2f) ? 0xffa657 : 0x7ee787;
          lv_obj_set_style_text_color(cell_labels[i], lv_color_hex(cv_color), 0);
        } else {
          lv_label_set_text(cell_labels[i], "--");
          lv_obj_set_style_text_color(cell_labels[i], lv_color_hex(0x6e7681), 0);
        }
      }
    }
    
    // Update alerts screen
    if (current_screen == 2) {
      static char alerts_text[256];
      int alert_count = 0;
      alerts_text[0] = '\0';
      strcat(alerts_text, "ACTIVE ALERTS\n\n");
      
      // Check for alert conditions
      uint8_t soc = datalayer.battery.status.reported_soc / 100;
      uint16_t cell_delta = datalayer.battery.status.cell_max_voltage_mV - 
                            datalayer.battery.status.cell_min_voltage_mV;
      
      if (soc < 10) {
        strcat(alerts_text, "! LOW SOC (<10%)\n");
        alert_count++;
      }
      if (temp_max > 45.0f) {
        strcat(alerts_text, "! HIGH TEMP (>45C)\n");
        alert_count++;
      }
      if (temp_min < 0.0f) {
        strcat(alerts_text, "! LOW TEMP (<0C)\n");
        alert_count++;
      }
      if (cell_delta > 100) {
        strcat(alerts_text, "! CELL IMBALANCE (>100mV)\n");
        alert_count++;
      }
      if (cell_min_v < 2.8f && cell_min_v > 0) {
        strcat(alerts_text, "! CELL UNDERVOLTAGE\n");
        alert_count++;
      }
      if (cell_max_v > 4.25f) {
        strcat(alerts_text, "! CELL OVERVOLTAGE\n");
        alert_count++;
      }
      if (!can_batt_ok) {
        strcat(alerts_text, "! NO BATTERY CAN\n");
        alert_count++;
      }
      
      if (alert_count == 0) {
        strcat(alerts_text, "No active alerts");
        lv_obj_set_style_text_color(lbl_alerts, lv_color_hex(0x7ee787), 0);
      } else {
        lv_obj_set_style_text_color(lbl_alerts, lv_color_hex(0xff7b72), 0);
      }
      lv_label_set_text(lbl_alerts, alerts_text);
      
      // Update event log display
      static char events_text[640];
      events_text[0] = '\0';
      for (int i = 0; i < event_count && i < MAX_EVENTS; i++) {
        strcat(events_text, event_log[i]);
        strcat(events_text, "\n");
      }
      if (event_count == 0) {
        strcpy(events_text, "(no events)");
      }
      lv_label_set_text(lbl_event_list, events_text);
    }
  }
  
  // Auto-dim disabled - causes display flickering via I2C interference
  // if (!screen_dimmed && (millis() - lastTouchMillis > DIM_TIMEOUT_MS)) {
  //   screen_dimmed = true;
  //   IO_EXTENSION_Pwm_Output(DIM_BRIGHTNESS);
  //   DEBUG_PRINTF("Screen dimmed due to inactivity\n");
  // }
}

#else

// Default display driver for other hardware (unchanged)
#include "../hal/hal.h"
#include "../utils/events.h"
#include "../webserver/webserver.h"
#include "../../datalayer/datalayer.h"
#include "../../battery/BATTERIES.h"

void init_display() {
  // Placeholder for other hardware
}

void update_display() {
  // Placeholder for other hardware
}

#endif

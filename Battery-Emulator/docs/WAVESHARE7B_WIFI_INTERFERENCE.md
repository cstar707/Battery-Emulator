# WiFi Interference Mitigation for Waveshare 7B Display

## Problem
WiFi radio emissions (2.4GHz) can couple into the display's I2C touch controller and data lines, causing:
- Screen jumping/shifting
- Display alignment issues
- Touch response problems
- Visual artifacts

## Software Mitigations (Implemented)

### 1. WiFi TX Power Reduction
- **Location**: Display UI → WiFi Settings → Power Level
- **Range**: 5dBm (minimum) to 19.5dBm (maximum)
- **Default**: 8.5dBm
- **Effect**: Lower power = less EMI, but shorter range

### 2. Power Save Mode (WIFI_PS_MIN_MODEM)
- Reduces WiFi beacon frequency and radio duty cycle
- Less radio activity = less interference
- Minimal impact on throughput

### 3. Channel Selection
- Use channels 1, 6, or 11 (non-overlapping)
- Avoid channels that may create harmonic interference with display clock

## Hardware Mitigations (Recommendations)

### 1. Ferrite Beads (Most Effective)
Add ferrite beads on I2C lines near the ESP32-S3:
- **SCL (GPIO8)** - Series ferrite bead
- **SDA (GPIO9)** - Series ferrite bead
- **TOUCH_INT** - Series ferrite bead
- Recommended: 600-1000Ω @ 100MHz (e.g., Murata BLM series)

### 2. RC Filters on I2C
Add small RC filters to suppress high-frequency noise:
- 47-100Ω series resistor + 47pF-100pF to GND on each line
- Place close to ESP32 pins

### 3. Shielding
- Add copper tape or foil shielding around WiFi antenna area
- Ground shield to ESP32 GND
- Keep shield away from display panel back

### 4. Physical Separation
- Keep WiFi antenna (on ESP32 module) away from display I2C traces
- Route I2C lines perpendicular to any WiFi traces if possible

### 5. Capacitor on Reset Line
The touch controller reset line (IO1 on IO expander) can pick up noise:
- Add 100nF capacitor to GND near the touch controller reset pin

## Display Panel Settings

### Reduce Refresh Rate
In display initialization, reduce LVGL refresh rate:
```cpp
// In display.cpp, reduce from 60Hz to 30Hz
disp_cfg.refresh_rate = 30;  // Lower refresh = less I2C activity during WiFi
```

### Disable Auto-Dimming During WiFi Activity
The PWM brightness control can interact with WiFi interference:
- Set fixed brightness instead of auto-dim
- Or increase DIM_TIMEOUT to reduce PWM changes

## Quick Fixes to Try

1. **Lower WiFi power to minimum** (5dBm) in display settings
2. **Use only STA mode** (disable AP) if possible
3. **Move device away from metal surfaces** that reflect RF
4. **Try different WiFi channel** (1, 6, 11)
5. **Reduce display update frequency** in code

## Recommended Action Plan

1. Start with software: Set WiFi power to 5dBm minimum
2. If still problematic, add ferrite beads to I2C lines
3. If needed, add shielding around WiFi module
4. Consider external WiFi antenna away from display

## References
- ESP32-S3 WiFi RF specs: Can emit up to 20dBm (100mW)
- GT911 touch controller: Sensitive to EMI on I2C lines
- 2.4GHz WiFi harmonics can interfere with digital signals

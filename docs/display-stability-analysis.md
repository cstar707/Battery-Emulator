# Display Stability Analysis — Vertical Shift & Memory Leaks

After several hours of runtime, the display can exhibit vertical shift/corruption. This document summarizes likely causes and suggested fixes.

## 1. Heap Fragmentation (Memory Leak Pattern)

### Problem
`mqtt_display_bridge.cpp` uses patterns that fragment the ESP32 heap over time:

- **`String body = http.getString()`** — Allocates variable-sized string on heap every 5–60 seconds
- **`JsonDocument doc`** (stack-local) — ArduinoJson 7 allocates on heap; each HTTP handler creates a new one
- HTTP polls run every 5s (root_sensors, storage_bits, curtailment) and 60s (envoy)

After hours, fragmented heap can cause:
- Allocation failures (LVGL, display driver, WiFi)
- Crashes or corruption when large contiguous blocks are needed

### Fix (Implement in mqtt_display_bridge.cpp)
- Replace `getString()` with `WiFiClient::read()` into a **static buffer** (e.g. 4KB)
- Use **StaticJsonDocument\<N\>** with fixed capacity instead of dynamic `JsonDocument`
- Reuse a single static document per handler where possible

---

## 2. RGB LCD Vertical Shift (GDMA FIFO Underflow)

### Root Cause (per Espressif docs)
Vertical shift/corruption on RGB LCD panels is often caused by **GDMA FIFO underflow**: the LCD controller reads from the wrong memory address when DMA is interrupted, causing subsequent scanlines to display incorrectly.

### Triggers
- Blocking operations during render (HTTP, WiFi, SPI)
- Insufficient bounce buffer → not enough VBlank time for ISR refill
- Flash/Preferences writes during display update

### Current Config (rgb_lcd_port.h)
```c
#define EXAMPLE_RGB_BOUNCE_BUFFER_SIZE  (EXAMPLE_LCD_H_RES * 10)  // 10 lines
```
Comment: "reduced from 20 to ease DMA/WiFi contention"

### Espressif Recommendation
`bounce_buffer_size_px` should be **at least 20 lines** to provide sufficient VBlank time and prevent FIFO pointer misalignment.

### Suggested Fixes
1. **Increase bounce buffer** to 15–20 lines (tradeoff: may increase DMA/WiFi contention)
2. **Stagger HTTP polls** so they don’t overlap with heavy LVGL flush activity
3. **Periodic full refresh** — Force a full-screen invalidation every 30–60 min to reset any accumulated drift (if supported)

---

## 3. HTTP Polling vs Display Timing

- `update_display()` runs in **connectivity_loop** task
- `tick_alive_counter()` (HTTP fetches) runs in **core_loop** task
- Both share the same heap; blocking HTTP with heap allocation can starve/fragment memory for LVGL

**Suggestion:** Ensure HTTP responses are bounded (max size) and use static buffers to avoid heap churn.

---

## 4. Summary of Recommended Changes

| Area            | Change                                                   | Risk   | Status  |
|-----------------|----------------------------------------------------------|--------|---------|
| mqtt_display_bridge | Static buffer + reusable JsonDocument for HTTP handlers | Low    | DONE    |
| rgb_lcd_port    | Bounce buffer 10 → 20 lines                              | Medium | DONE    |
| display         | Periodic full invalidation every 30 min                  | Low    | DONE    |

## 5. Implemented Fixes (commit)

- **mqtt_display_bridge.cpp**: Added `http_body_buf[4096]` and `http_get_to_buffer()`; replaced `String body = http.getString()` + stack JsonDocument with static buffer + static JsonDocument + `shrinkToFit()`.
- **rgb_lcd_port.h**: `EXAMPLE_RGB_BOUNCE_BUFFER_SIZE` increased from 10 to 20 lines.
- **display.cpp**: Every 30 min, `lv_obj_invalidate(lv_scr_act())` to force full redraw and reset vertical drift.

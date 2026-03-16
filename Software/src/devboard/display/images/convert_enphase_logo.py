#!/usr/bin/env python3
"""Convert enphase_logo_source.png to enphase_logo.h (LVGL TRUE_COLOR_ALPHA).
Output: no background - only the orange logo symbol and ENPHASE text visible;
everything else is fully transparent.
Requires: pip install Pillow"""
from PIL import Image
import os
import colorsys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(SCRIPT_DIR, "enphase_logo_source.png")
OUT = os.path.join(SCRIPT_DIR, "enphase_logo.h")
W, H = 80, 30

def to_lvgl(r, g, b, a):
    r5 = min(31, r >> 3)
    g6 = min(63, g >> 2)
    b5 = min(31, b >> 3)
    rgb565 = (r5 << 11) | (g6 << 5) | b5
    return [rgb565 & 0xFF, (rgb565 >> 8) & 0xFF, a]

def luminance(r, g, b):
    return 0.299 * r + 0.587 * g + 0.114 * b

def is_orange(r, g, b):
    """Orange symbol: red-dominant, hue in orange/red range."""
    h, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
    hue_deg = h * 360
    return (hue_deg <= 50 or hue_deg >= 330) and s >= 0.12 and v >= 0.15

def is_background(r, g, b):
    """Dark grey or white background -> fully transparent."""
    L = luminance(r, g, b)
    return L < 92 or (r >= 248 and g >= 248 and b >= 248)

def is_text(r, g, b):
    """Medium/light grey text (ENPHASE) - keep visible on dark UI."""
    L = luminance(r, g, b)
    spread = max(r, g, b) - min(r, g, b)
    return 92 <= L < 240 and spread < 100

if not os.path.exists(SRC):
    print("Missing:", SRC)
    exit(1)

img = Image.open(SRC).convert("RGBA").resize((W, H), Image.Resampling.LANCZOS)
pixels = img.load()
out = []
for y in range(H):
    for x in range(W):
        r, g, b, a = pixels[x, y]
        L = luminance(r, g, b)
        # Use source alpha: already transparent in PNG -> keep transparent
        if a < 200:
            out.extend(to_lvgl(0, 0, 0, 0))
            continue
        # Background (white/near-white) -> fully transparent
        if is_background(r, g, b):
            out.extend(to_lvgl(0, 0, 0, 0))
            continue
        # Orange logo symbol
        if is_orange(r, g, b):
            h, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
            s = min(1.0, s * 1.35)
            v = min(1.0, v * 1.15)
            r, g, b = colorsys.hsv_to_rgb(h, s, v)
            r, g, b = int(r * 255), int(g * 255), int(b * 255)
            out.extend(to_lvgl(r, g, b, 255))
        elif is_text(r, g, b):
            # Force visible light grey for ENPHASE text on dark UI
            r, g, b = 220, 220, 220
            out.extend(to_lvgl(r, g, b, 255))
        else:
            out.extend(to_lvgl(0, 0, 0, 0))
n = len(out)

with open(OUT, "w") as f:
    f.write('/* Enphase logo - logo + text only, no background */\n')
    f.write('#include "lvgl.h"\n\n')
    f.write('#ifndef LV_ATTRIBUTE_MEM_ALIGN\n')
    f.write('#define LV_ATTRIBUTE_MEM_ALIGN\n')
    f.write('#endif\n\n')
    f.write('static LV_ATTRIBUTE_MEM_ALIGN const uint8_t enphase_logo_map[] = {\n')
    for i in range(0, n, 16):
        chunk = out[i : i + 16]
        f.write('  ' + ', '.join('0x%02x' % b for b in chunk) + ',\n')
    f.write('};\n\n')
    f.write('const lv_img_dsc_t img_enphase_logo = {\n')
    f.write('  {LV_IMG_CF_TRUE_COLOR_ALPHA, 0, 0, %u, %u},\n' % (W, H))
    f.write('  %u,\n' % n)
    f.write('  enphase_logo_map,\n')
    f.write('};\n')
print("Created %s from %s (%ux%u, %u bytes) - logo and text only, no background" % (OUT, SRC, W, H, n))

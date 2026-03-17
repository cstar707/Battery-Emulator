# Display firmware: Settings in top bar + dual-Solis (uncommitted in other worktree)

These changes live in the **other worktree** (`battery-emulator-tesla-rewrite-stable-15ef950`, branch `feature/dual-solis-display-S3-1024`) as **uncommitted** edits. This doc records them so they are not lost.

**Modified files there:**
- `Software/src/devboard/display/display.cpp`
- `Software/src/devboard/display/mqtt_display_bridge.cpp`
- `Software/src/devboard/webserver/index_html.h`
- `Software/src/devboard/webserver/webserver.cpp`

---

## 1. Settings tab at top (7" display) – `display.cpp`

- **Tabs:** 3 → 4 tabs: Tesla-1, Tesla-2, Solar, Alerts. `tab_btns[4]`, added `current_tesla_board`.
- **Settings/Reboot in top bar:** Removed hiding of `btn_settings` / `btn_reboot` in `set_main_bottom_row_hidden()` so Settings and Reboot stay visible on all tabs. Wrapped in `#ifndef HW_WAVESHARE7B_DISPLAY_ONLY` the old bottom-row buttons (WiFi AP, Reboot, Contactors) so display-only build doesn’t create them; Settings/Reboot are in the top bar instead.
- **Popups in front:** When opening Settings or Reboot confirm, call `lv_obj_move_foreground(settings_backdrop)` and `lv_obj_move_foreground(settings_panel)` (and same for reboot_backdrop / reboot_confirm_panel) so popups appear above tabs.
- **Tab handlers:** Added `show_screen_tesla1`, `show_screen_tesla2`; Solar is tab index 2, Alerts index 3. Tab highlight loop uses `for (int i = 0; i < 4; i++)` and `i == 0/1/2/3` for active tab color.

---

## 2. Web UI – Settings in top bar + Tesla-1 / Tesla-2 – `webserver.cpp`

- **Tabs:** Tesla-1 | Tesla-2 | Solar | Cells | Alerts, with **Settings** and **Reboot** buttons in the top bar (inline after the tabs):
  - `Settings` → `window.location.href='/settings'`
  - `Reboot` → confirm "Reboot battery monitor?" then `fetch('/reboot')`, reload after 2s.
- **Tab indices:** showTab(0)=Tesla-1, (1)=Tesla-2, (2)=Solar, (3)=Cells, (4)=Alerts. Page IDs: p0=Tesla-1, p1=Tesla-2 (placeholder), p2=Solar (active by default), p3=Cells, p4=Alerts.
- **Tesla-2 placeholder (p1):** Full placeholder block with SOC, Voltage, Current, Power, Capacity, Cell Min/Max/Delta, Temp, Contactor, Network, Battery Info, System/CAN (ids like `soc_t2`, `v_t2`, etc.; "No board").
- **Solar section:** Solis #1 block (S6-EH1P, contactor LED, sl_soc_pct, sl_batt, sl_volt, sl_temp, sl_curr, sl_solar, sl_load, sl_grid, sl_day). Solark and Enphase sections as before.

---

## 3. Reboot wording – `index_html.h`

- Confirm text: **"Are you sure you want to reboot the battery monitor? NOTE: If contactors are controlled by this device, they will open during reboot!"** (replacing "reboot the emulator").

---

## 4. `mqtt_display_bridge.cpp`

- (Diffs were truncated; this file had Solis #1 battery-from-datalayer or similar bridge logic. Check the other worktree for full diff.)

---

## How to preserve these changes

1. **Option A – Commit in the other worktree**  
   Open `…/battery-emulator-tesla-rewrite-stable-15ef950`, run `git add` and `git commit` for the four files (and `docs/` if desired). Then the branch has the “Settings in top bar” and dual-Solis web/display changes.

2. **Option B – Copy files into this repo**  
   After checking out `feature/dual-solis-display-S3-1024` in this workspace (which requires removing the other worktree first), copy the four modified files from the other worktree over before removing it.

3. **Option C – Reapply from this doc**  
   Use this doc and the diffs (e.g. from `git diff` in the other worktree) to reapply the same edits in this repo once the display branch is checked out here.

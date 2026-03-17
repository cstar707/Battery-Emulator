# Dual Solis + Solark + Envoy Screen Layout — Staged Tasks

**Scope:** This doc covers **both** the **7" display** (firmware in other worktree, branch `feature/dual-solis-display-S3-1024`) and the **server/Solis S6 app** (this repo, branch `feature/solis-s6-app`). See `docs/solis-app-vs-display.md` for which branch to use for which work.

**Where each stage lives (don’t mix them up):**

| Stage | System | Where to work |
|-------|--------|----------------|
| 0 | Mostly **display** (0.2, 0.5, 0.6), plus shared doc (0.4) and parity (0.3) | Display: other worktree, `feature/dual-solis-display-S3-1024`. 0.3 “web UI” = display device’s web UI. |
| 1 | **BE firmware** (boards at .90, .X) | BE firmware repo/worktree (not this Solis app repo). |
| 2–5 | **Display** (MQTT subscribe, layout, master SOC, testing) | Display worktree, `feature/dual-solis-display-S3-1024`. |
| 6 | **Display** (7" + web UI parity, Tesla-1/2 tabs) | Display worktree, `feature/dual-solis-display-S3-1024`. |
| **Solis S6 app** | **Server only** (Modbus poll, MQTT publish, web dashboard) | **This repo**, `feature/solis-s6-app`. Not broken out as stages here; app publishes data so display can subscribe. |

**Goal:** Two Solis inverters (left/right) with identical 8-card blocks + contactor LEDs; Solark below (Envoy-style layout); Envoy below Solark. Per–battery-emulator MQTT (last-octet topics) so data does not overwrite. **Master SOC = average of all three battery systems** (Solis 1, Solis 2, Solark). All fits on 7" (1024×600) without scrolling. **Display stays lightweight:** all processing and number-crunching on remote server (e.g. solis_s6 app); this device only displays data to remain stable. Changes must be **clean and stable** — no crashing.
 **Device mapping:**
- **Inverter 1:** Solis modbus `10.10.53.16` ↔ BE board `10.10.53.90` (MQTT last octet `90`)
- **Inverter 2:** Solis modbus `10.10.53.15` ↔ BE board `10.10.53.X` (X TBD; device not online yet) — show 0s until online

**Future (do not add until system is stable):** Per-system kW capacity, remaining capacity, remaining hours — add only after a stable working system.

**Branching:** Create a **new branch** `feature/dual-solis-display-S3-1024` from the current stable commit (names the S3 hardware and 1024px display). Keep `main` (or your current stable branch) unchanged so you can roll back to the working build at any time. Merge only when the new display + BE changes are tested and stable. **Later ports:** this layout/firmware can be ported to other hardware: **P4 display** and **older S3-800** screen size — three possible targets (S3-1024, P4, S3-800).

**Rollback plan (if the new layout fails):** The known-good display code is **commit f1218c73** on branch **waveshare-7b-uiv2.4-tesla-rewrite-stable-15ef950** (“Display + Web UI: contactor LED, uptime, last-uptime when open; Solis PV mapping fix”). To restore the working display:
1. **Worktree:** Use the stable worktree: `cd /Users/chad/intent/workspaces/bizarre-basilisk/battery-emulator-tesla-rewrite-stable-15ef950`
2. **Checkout:** `git checkout waveshare-7b-uiv2.4-tesla-rewrite-stable-15ef950` (or `git checkout f1218c73`)
3. **Build & flash:** `pio run -e waveshare7b_330` then `pio run -e waveshare7b_330 -t upload`
That returns the display to the last verified working firmware. No need to revert commits on the feature branch; just build from the stable branch/commit and re-flash.

---

## Stage 0 — Stability and code cleanup (do first)

- [ ] **0.1** **Stability:** Review and enforce lightweight display architecture: no heavy processing or number-crunching on the display device; all aggregation/calculations (including master SOC average) should be computed on the server and published via MQTT, or kept to trivial operations (e.g. simple average of three SOC values from already-received data). Avoid large stack allocations, long HTTP in main loop, and dynamic JSON in hot paths to prevent crashes.
- [ ] **0.2** **Code review — remove unused display code:** Remove or gate legacy battery-emulator solar cards that are not used. Current finding: `lbl_solar_pv`, `lbl_solar_load`, `lbl_solar_grid`, `lbl_solar_batt_power`, `lbl_solar_batt_soc`, `lbl_solar_day_pv` are declared and updated in the “Legacy section” but are **never created** in `build_solar_screen()` (only Solark and Solis labels are created). Remove these unused label declarations and the legacy update block, or confirm they are dead and remove to reduce confusion and risk.
- [ ] **0.3** **Web UI parity:** Ensure the web UI (dashboard, `/solar` page, and any JSON API used by the UI) is updated in lockstep with the display so it reflects the same layout and data: dual Solis (Solis 1 + Solis 2), Solark, Envoy, and **master SOC (average of three systems)**. No discrepancy between what the 7" screen shows and what the web UI shows.
- [ ] **0.4** **Document data objects and MQTT mappings:** Create a single reference doc (e.g. `docs/display-data-mqtt-mapping.md`) that lists: (1) every data object (e.g. `SolarData` fields, BE info fields, contactor state); (2) MQTT topic → payload field → data object (truth/source); (3) data object → UI label (display and web). So for every label on screen and in the web UI, the doc states where the value comes from (topic + field). Update this doc when adding dual Solis, master SOC, or new topics so we don’t lose track and avoid memory/consistency issues.
- [ ] **0.5** **Display firmware: remove unused code to save flash/RAM/PSRAM:** Audit the **waveshare7b_display** build. Battery/inverter/charger/CAN/RS485/contactor/precharge are already excluded via `build_src_filter`, so no battery definition files are in flash. Check for any other unused code that is still compiled: e.g. `index_html.cpp` / `index_html.h` (web UI for display uses `webserver_display.cpp` with its own HTML; index_html is for full BE). Consider excluding unused webserver or communication modules from the display env only (e.g. `can_replay_html`, `can_logging_html`, `advanced_battery_html`, `settings_html` if not used by display, or `equipmentstopbutton`). Goal: free flash and RAM so adding more display content (dual Solis, etc.) doesn’t run out of memory or PSRAM and cause crashes. Keep OTA and all display-used paths.
- [ ] **0.6** **OTA and branding — “Battery Monitor” for display firmware only:** Keep the OTA function. In **this remote display firmware only** (env `waveshare7b_display`), rename branding from “Battery Emulator” to “Battery Monitor” so the .90 BE boards stay separate and clearly “Battery Emulator.” (1) **On-screen:** In `display.cpp` (inside `#ifdef HW_WAVESHARE7B_DISPLAY_ONLY`), change the main title text from “Battery Emulator” to “Battery Monitor” and the settings-panel version string from “Firmware: Battery Emulator” to “Firmware: Battery Monitor”. (2) **Web UI:** Already “Battery Monitor” in `webserver_display.cpp` (title and h1). (3) OTA menu link can stay as “OTA”; if the ElegantOTA `/update` page shows an app name, ensure it says “Battery Monitor” for this build (e.g. via lib config or wrapper if available). Do not change full Battery Emulator firmware (.90 boards).

---

## Stage 1 — MQTT topic scheme and BE firmware

- [ ] **1.1** Document MQTT topic scheme using last octet: `BE/<last_octet>/info`, `BE/<last_octet>/spec_data`, `BE/<last_octet>/events`, and Solis/inverter payloads under the same prefix (e.g. `BE/90/...`, `BE/<X>/...`). Publish scheme in repo (e.g. `docs/mqtt-topic-scheme-multi-be.md`).
- [ ] **1.2** Add board identity to BE firmware (e.g. NVM “device ID” or configurable last octet / board ID). All BE MQTT publishes use this ID in the topic path (e.g. `BE/90/...` for .90, `BE/<X>/...` for second board).
- [ ] **1.3** Ensure each BE board only polls its assigned Solis (90 → .16, X → .15) and publishes that inverter’s data on its own namespaced topics. No cross-talk.
- [ ] **1.4** Build and deploy BE firmware for `10.10.53.90` (inverter 1). Prepare firmware/image for `10.10.53.X` (inverter 2) using the new topic scheme so it’s ready when the device comes online.

---

## Stage 2 — Display: data model and subscriptions

- [ ] **2.1** Update display MQTT client to subscribe to both BE boards: e.g. `BE/90/#` and `BE/<X>/#` (or configurable second octet). Maintain separate state for “Solis 1 + contactor 1” (90/.16) and “Solis 2 + contactor 2” (X/.15).
- [ ] **2.2** Extend `SolarData` (or equivalent) so the display holds two Solis datasets (e.g. `solis1_*`, `solis2_*`) and two contactor states, plus existing Solark and Envoy data.
- [ ] **2.3** Implement “master SOC %” as **average of all three systems** (Solis 1 SOC + Solis 2 SOC + Solark SOC) / 3 (when a system is offline, use 0 or exclude from average — product choice). Prefer server publishing this value over MQTT so the display only shows it; if computed on device, keep it to a trivial average of three floats. Expose for main SOC display and web UI.

---

## Stage 3 — Screen layout and sizing (no scrolling)

- [ ] **3.1** Create a layout spec/wireframe: two Solis blocks side-by-side (left/right), each with the same 8 cards, logo, model number, SOC bar, and contactor LED; Solark row below; Envoy row below Solark. All within 1024×600; font/size adjustments allowed so nothing overflows.
- [ ] **3.2** Implement layout: Solis 1 (left) and Solis 2 (right). For each side: duplicate the same 8 cards as current Solis screen, plus logo, model number, SOC bar, and contactor LED. Solis 2 shows 0s (and optional “offline”) until data from `BE/<X>/#` is present.
- [ ] **3.3** Resize and position elements so the full layout fits on the 7" screen without scrolling (adjust card sizes, fonts, spacing, and section positions as needed).
- [ ] **3.4** Duplicate logos and model number for the second inverter block so both sides look the same visually.

---

## Stage 4 — Solark and Envoy repositioning

- [ ] **4.1** Move Solark section to below the two Solis blocks. Keep the same Solark data items; change layout to a landscape/Envoy-style row so the 8 items fit in one compact row (or similar) for better fit.
- [ ] **4.2** Move Envoy section below the Solark block. No change to Envoy data or topics; position only.

---

## Stage 5 — Master SOC and polish

- [ ] **5.1** Wire the main SOC display (and SOC bar) to the **master SOC %** (average of all three systems from task 2.3).
- [ ] **5.2** **Web UI:** Update dashboard and `/solar` to show dual Solis (Solis 1 & 2), Solark, Envoy, and master SOC (average of three). Match layout and data to the 7" screen.
- [ ] **5.3** Test with inverter 2 offline (all 0s for Solis 2) and with both online; confirm no topic cross-talk, correct per-board data, and no crashes.
- [ ] **5.4** Document final IP map and topic map: which BE is inverter 1 vs 2, and `BE/90` vs `BE/<X>` usage for future reference.

---

## Summary

| Stage | Focus |
|-------|--------|
| 0 | Stability, remove unused legacy solar labels, web UI parity, **document data objects & MQTT mappings**, **audit/remove unused code in display build** (save flash/RAM/PSRAM), **OTA kept + rename to “Battery Monitor” for display firmware only** |
| 1 | MQTT topic scheme (last octet), BE firmware identity and per-board publish, Solis polling per BE |
| 2 | Display subscribes to both BEs, two Solis datasets + contactors, master SOC = average of three |
| 3 | Two Solis blocks (8 cards + logo + model + SOC bar + contactor each), fit without scrolling |
| 4 | Solark below Solis (Envoy-style layout), Envoy below Solark |
| 5 | Master SOC on main screen and web UI, web UI reflects same layout/data, testing, documentation |

**Master SOC:** Average of Solis 1 SOC, Solis 2 SOC, and Solark SOC (three systems). **Later (optional):** Per-system kW capacity, remaining capacity, remaining hours — only after a stable working system.

---

## Stage 6 — Tesla-1 / Tesla-2 tabs and top bar (7" display + web UI parity)

**Scope:** 7" display (LVGL) and web UI — same layout and behaviour on both.

- [ ] **6.1** **Tabs:** Rename current Tesla tab to **Tesla-1**. Add new tab **Tesla-2** with the same cards/layout as Tesla-1 (for the second Solis battery emulator board; second board not assigned an IP yet — Tesla-2 can show placeholders/"--" until configured).
- [ ] **6.2** **Top row (all tabs):** Move **Settings** and **Reboot** to the main top bar so they are visible on every tab. Layout: tabs then buttons — **Tesla-1 | Tesla-2 | Solar | Cells | Alerts | [Settings] [Reboot]** (Settings beside Alerts, Reboot beside Settings).
- [ ] **6.3** **Remove buttons:** Remove **AP off** button and **Contactors** button from the UI (both 7" and web). **Keep** contactor LED status on the main/Solar screen (green/grey for closed/open).
- [ ] **6.4** **Web UI parity:** Apply the same tab order, Tesla-1/Tesla-2 content, top-bar Settings + Reboot, and button removals in the web UI so it matches the 7" display.

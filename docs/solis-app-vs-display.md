# Solis S6 app vs 7-inch display — separate workstreams

Two different pieces of work are easy to mix up. Keep them on **separate branches** (and for the display, a separate worktree).

---

## 1. Solis S6 app (server)

**What it is:** Python/FastAPI app that runs on a **server** (e.g. 10.10.53.92). Polls Solis inverter(s) via Modbus, subscribes to Solark via MQTT, publishes Solis data to MQTT, serves the **main web dashboard** (Solis/Solark cards, controls, curtailment, settings). This is the **server dashboard** — not to be confused with the Waveshare display device’s web UI (see below).

**Repo:** This repo (`tesla-model-y-stationary-power-plant`)  
**Branch:** `feature/solis-s6-app`  
**Path:** `scripts/solis_s6_app/` (config.py, main.py, solis_modbus.py, mqtt_publish.py, templates/, etc.)

**When working on the Solis app:**  
`git checkout feature/solis-s6-app` in **this** workspace.

---

## 2. 7-inch display (firmware)

**What it is:** Embedded firmware for the **Waveshare 7"** (1024×600). Subscribes to MQTT (Solis, Solark, Envoy, BE), draws LVGL UI on the screen, and serves its **own web UI** from the display device (e.g. `webserver_display.cpp`) — the **Waveshare web UI**, which mirrors the 7" screen. Not the same as the Solis S6 app (server) dashboard. **No Modbus** on the display; it only displays data. “Battery Monitor” branding for this build.

**Repo:** Battery-Emulator (separate worktree)  
**Worktree path:** `/Users/chad/intent/workspaces/bizarre-basilisk/battery-emulator-tesla-rewrite-stable-15ef950`  
**Branch:** `feature/dual-solis-display-S3-1024`  
**Code:** e.g. `Software/src/devboard/display/display.cpp`, `mqtt_display_bridge.cpp`, `webserver_display.cpp`

**When working on the 7" display:**  
Open the **display worktree** in Cursor and use branch `feature/dual-solis-display-S3-1024` there.

**Display-only docs in this repo (reference only):** `display-settings-top-bar-changes.md`, `display-web-ui-parity.md` — describe display firmware; implement those changes in the display worktree, not here.

---

## Two different web UIs — do not confuse

| Web UI | Where it runs | Purpose |
|--------|----------------|--------|
| **Solis S6 app dashboard** | **Server** (e.g. 10.10.53.92) | Main dashboard: Solis/Solark data, controls, curtailment, settings. Full-featured. |
| **Waveshare display web UI** | **Display device** (the 7" unit, e.g. 10.10.53.110) | Lightweight UI served by the display firmware; mirrors what’s on the 7" screen. Different codebase (`webserver_display.cpp` etc.). |

---

## How they relate

- The **Solis S6 app** (server) publishes Solis data to MQTT. The **7" display** (and BE boards) **subscribe** to MQTT. So the app feeds the display; they are not the same codebase.
- **docs/dual-solis-solar-layout-tasks.md** describes the **overall** dual-Solis + Solark + Envoy layout. It mixes:
  - **Server / app** tasks (e.g. “all processing on remote server (e.g. solis_s6 app)”)
  - **Display** tasks (Stages 0–6: display firmware, MQTT subscriptions, layout, web UI parity)
  - **BE firmware** tasks (per-board MQTT, Solis polling)
- For **agent/work sessions:** say which you mean: “work on **Solis app**” → this repo, `feature/solis-s6-app`; “work on **7-inch display**” → display worktree, `feature/dual-solis-display-S3-1024`.

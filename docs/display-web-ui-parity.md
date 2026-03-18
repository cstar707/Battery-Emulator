# Web UI parity with 7" display

**Goal:** The web UI is a **duplicate of the 7" display** — same layout (Tesla-1, Tesla-2, Solar, Cells, Alerts, Settings/Reboot in the top bar), same data everywhere. What you see on the screen is what you see on the web.

**Principle:** The web UI served by the battery monitor (e.g. 10.10.53.110) must show the **same data** as the 7" display on the same device. One data source feeds both.

**Implication:** On the device that runs both the 7" (LVGL) and the web server:

- The 7" and the web must consume the **same** in-memory data (e.g. `SolarData` / solis1_*, solis2_*, etc.).
- Any API or server-rendered page that drives the web UI must read from that same structure — not from a different topic or a different code path that omits fields (e.g. Solis #1 battery voltage, current, temp, SOC).
- If the 7" shows correct Solis #1 battery but the web shows zeros, the bug is in the display firmware: the web path is not using the same data as the 7", or the structure is not fully filled for the web/API.

**Where to fix:** In the **battery monitor display firmware** (the build that runs on .110: 7" + web). That firmware typically lives in the worktree that has:

- `mqtt_display_bridge.cpp` (or equivalent) — fills display data from MQTT (and any BE/battery source).
- Display-specific webserver (e.g. `webserver_display.cpp`) and/or `/api/data` (or similar) that serves the web UI.

Ensure every field shown on the 7" (including Solis #1 battery V, I, temp, SOC) is written into the shared data structure and that the web/API reads **only** from that structure so the web UI is a direct parity view of the 7" display.

**Data flow (intended):** MQTT broker ← BE .90 (battery) + Solis app (Solis/Modbus). Battery monitor .110 **subscribes** to the broker; one pipeline fills the display data; both 7" and web render from that.

**Lightweight:** The display device does no Modbus, no polling, no heavy aggregation. It only subscribes to the MQTT broker and displays what it receives. All processing lives on the server side (solis app, BE boards); the display stays stable and light.

**All topics through the broker:** By routing all data through the MQTT broker, the same stream can later feed Home Assistant, an InfluxDB (or similar) for time series, or other tools — one pipeline, multiple subscribers. Use whatever is most practical (e.g. HA MQTT integration, or a small service that subscribes and writes to Influx).

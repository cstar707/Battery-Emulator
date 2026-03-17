# Workspace and Branch Setup

This repo uses **multiple git worktrees** to keep different feature branches isolated. Use the correct workspace for each type of work.

**See also:** `docs/solis-app-vs-display.md` — Solis S6 **app** (server) vs **7" display** (firmware) are separate; different branches and (for display) a different worktree.

## Workspaces

| Work | Workspace path | Branch |
|------|----------------|--------|
| **7" display / Solar (Waveshare)** | `/Users/chad/intent/workspaces/bizarre-basilisk/battery-emulator-tesla-rewrite-stable-15ef950` | `feature/dual-solis-display-S3-1024` |
| **Solis S6 app (server)** | This repo (CloudDocs path below) | `feature/solis-s6-app` |
| **Velar battery / vehicle context** | `/Users/chad/Library/Mobile Documents/com~apple~CloudDocs/cursor/tesla-model-y-stationary-power-plant` | `codex/velar-v10-recovery-point` |

## Quick reference

- **7" display** (LVGL, MQTT subscribe, Solar tab, web UI): Open the **display worktree** in Cursor; branch `feature/dual-solis-display-S3-1024`.
- **Solis S6 app** (server dashboard, Modbus poll, MQTT publish): **This** repo, branch `feature/solis-s6-app`.
- **Velar work** (LAND-ROVER-VELAR-PHEV-BATTERY, vehicle context, OTA): This repo, branch `codex/velar-v10-recovery-point`.

## Opening the display worktree in Cursor

**File → Open Folder** →  
`/Users/chad/intent/workspaces/bizarre-basilisk/battery-emulator-tesla-rewrite-stable-15ef950`

## Build & flash (display)

```bash
cd /Users/chad/intent/workspaces/bizarre-basilisk/battery-emulator-tesla-rewrite-stable-15ef950
pio run -e waveshare7b_330
pio run -e waveshare7b_330 -t upload --upload-port /dev/cu.usbmodem*
```

## Preserved files

Display-related docs and scripts have been copied to the display worktree:
- `docs/display-web-ui-parity.md`
- `docs/display-settings-top-bar-changes.md`
- `docs/dual-solis-solar-layout-tasks.md`
- `docs/solis-s6-app-solark-mqtt-contract.md`
- `scripts/solis_s6_app/config.py`, `main.py`

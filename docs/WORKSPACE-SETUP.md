# Workspace and Branch Setup

This is the **display worktree** for Waveshare 7" and Solar/dual-Solis work.

| Work | Workspace path | Branch |
|------|----------------|--------|
| **Display / Solar / 7" Waveshare** | **This folder** | `feature/dual-solis-display-S3-1024` |
| **Velar battery / vehicle context** | `~/Library/Mobile Documents/.../tesla-model-y-stationary-power-plant` | `codex/velar-v10-recovery-point` |

## Build & flash

```bash
pio run -e waveshare7b_330
pio run -e waveshare7b_330 -t upload --upload-port /dev/cu.usbmodem*
```

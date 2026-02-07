# Building when the project is on a network drive (e.g. TrueNAS SMB)

If your project lives on a network mount (e.g. `/Volumes/development` on macOS), PlatformIO may fail with **Permission denied** when the ESP32 platform tries to run tools (like `uv`) from a venv inside that mount—many network filesystems don't allow executing binaries.

## Fix: use a venv on the local disk

Use a **Python 3.12** virtualenv in your **home directory** (local disk) and run PlatformIO from there, pointing at the project on the network.

### One-time setup (this computer)

1. **Python 3.12** (e.g. `brew install python@3.12`).
2. **Create venv in home** (not on the network drive):
   ```bash
   /opt/homebrew/opt/python@3.12/bin/python3.12 -m venv ~/.venv-battery-emulator-pio
   ~/.venv-battery-emulator-pio/bin/python -m pip install --upgrade pip platformio
   ```

### Build / upload

From anywhere, run (adjust `-d` to your project path and `-e` to your env):

```bash
~/.venv-battery-emulator-pio/bin/python -m platformio run -e lilygo_330 -d /Volumes/development/projects/Battery-Emulator/Battery-Emulator
```

**Upload** (board in download mode, same `-d` and `-e`):

```bash
~/.venv-battery-emulator-pio/bin/python -m platformio run -e lilygo_330 -t upload -d /Volumes/development/projects/Battery-Emulator/Battery-Emulator
```

Examples for other envs:

- **Waveshare 7B:** `-e waveshare7b_330`
- **LilyGo 2-CAN:** `-e lilygo_2CAN_330`

Build output and `.pio/` still live on the network drive; only the Python/PlatformIO venv runs from local disk, so the platform can execute its tools without permission errors.

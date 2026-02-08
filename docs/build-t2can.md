# Building and flashing for LilyGo T_2CAN (t-2can branch)

This branch is for the **LilyGo T_2CAN** board (dual CAN: native + MCP2515). Use it from the **repo root** (where `platformio.ini` and `Software/` live).

## On this machine or after clone

1. **Clone and checkout** (if on another computer):
   ```bash
   git clone -b t-2can git@github.com:cstar707/Battery-Emulator.git
   cd Battery-Emulator
   ```
   Or if you already have the repo:
   ```bash
   git fetch origin
   git checkout t-2can
   ```

2. **Build** (from repo root):
   ```bash
   pio run -e lilygo_2CAN_330
   ```

3. **Flash** (board connected via USB):
   ```bash
   pio run -e lilygo_2CAN_330 -t upload
   ```

4. **Serial monitor** (115200 baud; trigger reset to see boot log):
   ```bash
   pio run -e lilygo_2CAN_330 -t monitor
   ```
   Or use PlatformIO Serial Monitor from the IDE.

## Important

- Run all `pio` commands from the **repository root** (the directory that contains `platformio.ini`). There is no `Battery-Emulator/` subfolder on this branch.
- First boot: AP **BatteryEmulator** (password `123456789`), IP **192.168.4.1**. USB logging is on by default so you get serial output; you can disable it in the web UI later.
- PSRAM init error in serial is expected (board has OPI PSRAM, build uses QSPI); the firmware does not use PSRAM.

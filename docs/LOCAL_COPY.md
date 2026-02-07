# Working from a local copy (this computer)

A full copy of the project is on your **local disk** so you can build and flash without network-drive permission issues.

## Path

```
~/development/projects/Battery-Emulator
```

Firmware and PlatformIO project (where you run build/upload):

```
~/development/projects/Battery-Emulator/Battery-Emulator
```

## Build and upload (local copy)

Use the **home-directory venv** (Python 3.12 + PlatformIO). No `-d` needed when you `cd` into the project.

```bash
cd ~/development/projects/Battery-Emulator/Battery-Emulator

# Build
~/.venv-battery-emulator-pio/bin/python -m platformio run -e lilygo_330

# Upload (board in download mode)
~/.venv-battery-emulator-pio/bin/python -m platformio run -e lilygo_330 -t upload
```

Other envs: `-e waveshare7b_330`, `-e lilygo_2CAN_330`, etc.

## Syncing with the network copy

If you still use the project on `/Volumes/development`, you can sync changes either way with `rsync`. From the network copy to local:

```bash
rsync -a --exclude='.pio' --exclude='.venv-pio312' --exclude='.pio-venv' \
  /Volumes/development/projects/Battery-Emulator/ \
  ~/development/projects/Battery-Emulator/
```

Consider making the local copy your main workspace and pushing/pulling from Git from there; the network path can then be an extra backup or share.

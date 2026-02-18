# Local config and secrets – never commit to GitHub

**All credentials and server-specific configuration must stay local.** Nothing that identifies your MQTT broker, WiFi, web passwords, or other secrets may be committed to the repository.

## Where credentials live

- **On the device:** WiFi (SSID/password), MQTT (broker host, port, user, password), web UI (username/password), and related settings are configured in the board’s **Settings** and stored in **NVM** (non-volatile memory). The firmware does not hardcode these; they are set via the web UI (or future provisioning) and stay on the device.
- **In local files only:** If you use local override files (e.g. `USER_SECRETS.h`, `secrets.yaml` for ESPHome reference, `.env`), keep them only on your machine and ensure they are in **`.gitignore`** so they are never pushed to GitHub.

## Never commit

- **Server addresses:** MQTT broker hostname or IP, Home Assistant URL, or any other server IP/hostname that points to your network.
- **Passwords:** MQTT password, web UI password, WiFi password, OTA password, API keys.
- **Usernames:** If they are account-specific (e.g. MQTT user, web login).
- **Secrets files:** Any file that contains the above (e.g. `secrets.yaml`, `.env`, `USER_SECRETS.h` with real values).

Documentation may use **example** values (e.g. `192.168.1.10`, `homeassistant.local`, “your HA IP”) as placeholders. Do not replace those with your real addresses or paste real credentials into the repo.

## .gitignore

The repo’s `.gitignore` is set up to exclude common secret and local-config file names so they are not accidentally committed. If you add new local files that hold credentials or server info, add a matching pattern to `.gitignore`. See the root **`.gitignore`** for the current list.

## Summary

| What                | Where it belongs              | Never put in repo      |
|---------------------|-------------------------------|------------------------|
| MQTT server/port    | Device Settings (NVM)         | Real hostname or IP    |
| MQTT user/password  | Device Settings (NVM)        | Real credentials       |
| WiFi SSID/password  | Device Settings (NVM)         | Real SSID/password     |
| Web UI user/password| Device Settings (NVM)         | Real credentials       |
| Local override files| Your machine only, in .gitignore | Any file with real creds |

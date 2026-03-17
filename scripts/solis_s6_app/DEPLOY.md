# Solis S6 app — deploy to server

The app runs on the **server** (e.g. 10.10.53.92). To see local changes on the dashboard you push the app files to the server and restart the service.

## Workflow

1. **Edit** code locally (this repo, branch `feature/solis-s6-app`).
2. **Deploy** app files to the server (e.g. rsync into `solar-monitoring/solis_s6_app/`).
3. **Restart** the service on the server (e.g. `sudo systemctl restart solis-s6-ui`).
4. **Check** the dashboard at `http://10.10.53.92:3007/` (or `:3008` if `SOLIS_APP_PORT=3008` is set on the server).

## Example deploy commands

From your machine (adjust paths and host if needed):

```bash
# Sync app directory to server
rsync -av --exclude .venv --exclude __pycache__ \
  scripts/solis_s6_app/ \
  10.10.53.92:solar-monitoring/solis_s6_app/

# Restart the app service
ssh 10.10.53.92 "sudo systemctl restart solis-s6-ui"
```

Then open **http://10.10.53.92:3007/** (or :3008) in a browser to use the dashboard.

## Port

Default port is **3007** (`SOLIS_APP_PORT` in config / env). The server may run on 3008 if configured there.

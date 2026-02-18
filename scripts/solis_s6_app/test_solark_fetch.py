#!/usr/bin/env python3
"""One-off test: can we read SOC from the Solark board? Run from app dir: python3 test_solark_fetch.py"""
import json
import os
import sys
from base64 import b64encode
from pathlib import Path
from urllib.request import Request, urlopen
from urllib.error import URLError, HTTPError

# Load .env so SOLARK_* are available
BASE = Path(__file__).resolve().parent
env_file = BASE / ".env"
if env_file.exists():
    with open(env_file) as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith("#") and "=" in line:
                k, v = line.split("=", 1)
                k, v = k.strip(), v.strip().strip('"').strip("'")
                if k and k not in os.environ:
                    os.environ.setdefault(k, v)

try:
    _s = json.load(open(BASE / "settings.json")) if (BASE / "settings.json").exists() else {}
except Exception:
    _s = {}
host = (os.environ.get("SOLARK1_HOST", "") or _s.get("solark1_host", "") or "").strip()
if not host:
    print("No Solark host. Set solark1_host in Settings or SOLARK1_HOST in .env")
    sys.exit(1)

port = int(os.environ.get("SOLARK1_HTTP_PORT", "80").strip() or "80")
url = f"http://{host}:{port}/solark_data"
user = (os.environ.get("SOLARK_HTTP_USER") or "").strip() or None
password = (os.environ.get("SOLARK_HTTP_PASSWORD") or "").strip() or None

print(f"GET {url}")
if user:
    print(f"  Auth: Basic {user}:****")
else:
    print("  Auth: none (set SOLARK_HTTP_USER and SOLARK_HTTP_PASSWORD if board requires login)")

req = Request(url)
if user and password:
    creds = b64encode(f"{user}:{password}".encode()).decode()
    req.add_header("Authorization", f"Basic {creds}")

try:
    with urlopen(req, timeout=10) as r:
        raw = r.read().decode()
        data = json.loads(raw)
    soc_pptt = data.get("battery_soc_pptt")
    soc = (soc_pptt / 100.0) if soc_pptt is not None else None
    print(f"  HTTP {r.status}")
    if soc is not None:
        print(f"  Solark SOC: {soc:.1f}%")
    print("  Full JSON:", json.dumps(data, indent=2)[:500])
except HTTPError as e:
    print(f"  HTTP {e.code}: {e.reason}")
    if e.code == 401:
        print("  -> Board requires login. Set SOLARK_HTTP_USER and SOLARK_HTTP_PASSWORD in .env to match the board's web username/password.")
    elif e.code == 404:
        print("  -> /solark_data not found. Ensure the device at this IP is running firmware that exposes GET /solark_data (Battery-Emulator with Solark RS485).")
except URLError as e:
    print(f"  Error: {e.reason}")
except Exception as e:
    print(f"  Error: {e}")

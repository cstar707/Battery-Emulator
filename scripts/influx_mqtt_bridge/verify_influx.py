#!/usr/bin/env python3
"""
Verify InfluxDB pipeline: check recent points on each measurement.
Expects: solis ~5-10s, Envoy ~30s. Reports "Last seen" per source.
"""
from __future__ import annotations

import os
import sys
import time

INFLUX_URL = os.environ.get("INFLUX_URL", "http://localhost:8086")
INFLUX_TOKEN = os.environ.get("INFLUX_TOKEN", "")
INFLUX_BUCKET = os.environ.get("INFLUX_BUCKET", "solar")
INFLUX_ORG = os.environ.get("INFLUX_ORG", "")

def main():
    if not INFLUX_TOKEN:
        print("INFLUX_TOKEN required")
        sys.exit(1)
    try:
        from influxdb_client import InfluxDBClient
    except ImportError:
        print("pip install influxdb-client")
        sys.exit(1)
    now = time.time()
    measurements = ["solis", "solark", "envoy_aggregate", "envoy_inverter", "battery_emulator"]
    expected_max_age = {"solis": 15, "solark": 15, "envoy_aggregate": 60, "envoy_inverter": 60, "battery_emulator": 15}
    print("InfluxDB verification (bucket=%s)" % INFLUX_BUCKET)
    print("-" * 50)
    client = InfluxDBClient(url=INFLUX_URL, token=INFLUX_TOKEN, org=INFLUX_ORG)
    query_api = client.query_api()
    all_ok = True
    for m in measurements:
        flux = f'from(bucket:"{INFLUX_BUCKET}") |> range(start: -1h) |> filter(fn: (r) => r["_measurement"] == "{m}") |> last()'
        try:
            tables = query_api.query(flux)
            last_ts = None
            for table in tables:
                for record in table.records:
                    t = record.get_time()
                    if t:
                        last_ts = t.timestamp()
                        break
            if last_ts is not None:
                age = now - last_ts
                ok = age < expected_max_age.get(m, 120)
                if not ok:
                    all_ok = False
                status = "OK" if ok else "STALE"
                print("%-20s %6.1fs ago  [%s]" % (m, age, status))
            else:
                print("%-20s no data" % m)
                all_ok = False
        except Exception as e:
            print("%-20s ERROR: %s" % (m, e))
            all_ok = False
    client.close()
    print("-" * 50)
    sys.exit(0 if all_ok else 1)

if __name__ == "__main__":
    main()

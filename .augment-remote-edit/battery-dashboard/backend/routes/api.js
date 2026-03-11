// ─────────────────────────────────────────────────────────
//  routes/api.js  –  REST API endpoints
//
//  Cell monitor endpoints:
//    GET /api/cells/latest
//    GET /api/cells/at?t=ISO
//    GET /api/ticks?hours=24
//
//  Sensor / dashboard endpoints:
//    GET /api/sensors              → list of sensor definitions
//    GET /api/sensors/latest       → latest value for all sensors
//    GET /api/sensors/:id/history  → time series (?hours=24&points=500)
//
//  Utility:
//    GET /api/config
//    GET /api/debug
// ─────────────────────────────────────────────────────────

const express    = require('express');
const config     = require('../config');
const sensorDefs = require('../sensors');

const router = express.Router();

const db = config.app.useMockData
  ? require('../services/mockdata')
  : require('../services/influxdb');

const sensorData = config.app.useMockData
  ? require('../services/mocksensors')
  : require('../services/sensordata');

const DEFAULT_BATTERY = typeof db.getDefaultBattery === 'function' ? db.getDefaultBattery() : 'tesla';
const VALID_BATTERIES = new Set(['tesla', 'ruxiu']);
function resolveBattery(req, res) {
  const battery = String(req.query.battery || DEFAULT_BATTERY).toLowerCase();
  if (!VALID_BATTERIES.has(battery)) {
    res.status(400).json({ ok: false, error: 'Invalid battery. Use ?battery=tesla or ?battery=ruxiu' });
    return null;
  }
  return battery;
}

// ── Cell monitor ──────────────────────────────────────────

router.get('/cells/latest', async (req, res) => {
  const battery = resolveBattery(req, res);
  if (!battery) return;
  try { res.json({ ok: true, ...(await db.getLatestCells(battery)) }); }
  catch (err) { res.status(500).json({ ok: false, error: err.message }); }
});

router.get('/cells/at', async (req, res) => {
  const battery = resolveBattery(req, res);
  if (!battery) return;
  const ts = req.query.t;
  if (!ts || isNaN(Date.parse(ts)))
    return res.status(400).json({ ok: false, error: 'Invalid or missing ?t= timestamp' });
  try { res.json({ ok: true, ...(await db.getCellsAt(ts, battery)) }); }
  catch (err) { res.status(500).json({ ok: false, error: err.message }); }
});

router.get('/battery-sources', async (req, res) => {
  try {
    const defaultBattery = typeof db.getDefaultBattery === 'function' ? db.getDefaultBattery() : DEFAULT_BATTERY;
    const sources = typeof db.getBatterySources === 'function'
      ? await db.getBatterySources()
      : {};
    res.json({ ok: true, defaultBattery, sources });
  } catch (err) {
    res.status(500).json({ ok: false, error: err.message });
  }
});

router.get('/ticks', async (req, res) => {
  const hours = Math.min(parseInt(req.query.hours || '24'), config.app.historyHours);
  try { res.json({ ok: true, timestamps: await db.getDataTimestamps(hours) }); }
  catch (err) { res.status(500).json({ ok: false, error: err.message }); }
});

// ── Sensors / dashboard ───────────────────────────────────

// List all configured sensors (metadata only, no data)
router.get('/sensors', (req, res) => {
  res.json({ ok: true, sensors: sensorDefs });
});

// Latest value for every sensor
router.get('/sensors/latest', async (req, res) => {
  try { res.json({ ok: true, values: await sensorData.getAllLatest() }); }
  catch (err) { res.status(500).json({ ok: false, error: err.message }); }
});

// Time-series history for a single sensor
router.get('/sensors/:id/history', async (req, res) => {
  const { id }     = req.params;
  const hours      = Math.min(parseInt(req.query.hours  || '24'), 168); // max 7 days
  const maxPoints  = Math.min(parseInt(req.query.points || '500'), 2000);
  try {
    const data = await sensorData.getSensorHistory(id, hours, maxPoints);
    res.json({ ok: true, sensorId: id, hours, data });
  } catch (err) {
    res.status(err.message.startsWith('Unknown') ? 404 : 500)
       .json({ ok: false, error: err.message });
  }
});

// ── Grid status (Envoy data: MQTT first, fallback to HTTP API) ────────────

function fetchEnvoyFromHttp() {
  const http = require('http');
  const url = (config.envoy?.apiUrl || 'http://localhost:3002').replace(/\/$/, '');
  const target = `${url}/api/envoy/debug`;
  return new Promise((resolve, reject) => {
    http.get(target, (resp) => {
      let body = '';
      resp.on('data', (chunk) => { body += chunk; });
      resp.on('end', () => {
        try { resolve(JSON.parse(body)); } catch (e) { reject(e); }
      });
    }).on('error', reject);
  });
}

router.get('/grid/envoy', async (req, res) => {
  const cache = require('../services/mqttcache');
  try {
    const mq1 = cache.getEnvoy('envoy1');
    const mq2 = cache.getEnvoy('envoy2');
    let envoy1, envoy2;
    if (mq1 && mq2) {
      envoy1 = mq1;
      envoy2 = mq2;
    } else {
      const data = await fetchEnvoyFromHttp();
      envoy1 = data.envoy1 || {};
      envoy2 = data.envoy2 || {};
    }
    res.json({
      ok: true,
      source: (mq1 && mq2) ? 'mqtt' : 'http',
      envoy1: {
        production: envoy1.production ?? 0,
        active_inverters: envoy1.active_inverters ?? 0,
        energy_today_kwh: extractEnergyToday(envoy1),
      },
      envoy2: {
        production: envoy2.production ?? 0,
        active_inverters: envoy2.active_inverters ?? 0,
        energy_today_kwh: extractEnergyToday(envoy2),
      },
    });
  } catch (err) {
    res.status(500).json({ ok: false, error: err.message });
  }
});

function extractEnergyToday(envoy) {
  const raw = envoy.raw_data || (envoy.raw_envoy_data && envoy.raw_envoy_data.raw_data) || {};
  const prod = raw.production || {};
  const prodArr = prod.production || [];
  for (const item of prodArr) {
    if (item.type === 'inverters' && (item.whToday != null || item.whtoday != null)) {
      const wh = item.whToday ?? item.whtoday ?? 0;
      return wh / 1000;
    }
  }
  if (prod.wattHoursToday != null) return prod.wattHoursToday / 1000;
  return null;
}

// Full inverter list from raw_data (HTTP) or raw_envoy_data.raw_data (MQTT)
function getInverters(envoy) {
  const raw = envoy.raw_data || (envoy.raw_envoy_data && envoy.raw_envoy_data.raw_data) || {};
  const inv = raw.inverters || envoy.inverters;
  if (Array.isArray(inv)) return inv;
  if (inv && Array.isArray(inv.inverters)) return inv.inverters;
  return [];
}

// Rolling integration for daily kWh from inverter watts (single dt for both envoys)
const dailyWhBuffer = { envoy1: 0, envoy2: 0, lastTs: null, lastDay: null, lastSum1: 0, lastSum2: 0 };
function integrateDailyWh(sum1, sum2, hasInverters1, hasInverters2) {
  const now = Date.now();
  const today = new Date().toDateString();
  if (dailyWhBuffer.lastDay !== today) {
    dailyWhBuffer.envoy1 = 0;
    dailyWhBuffer.envoy2 = 0;
    dailyWhBuffer.lastDay = today;
  }
  const s1 = hasInverters1 && sum1 >= 0 ? sum1 : dailyWhBuffer.lastSum1;
  const s2 = hasInverters2 && sum2 >= 0 ? sum2 : dailyWhBuffer.lastSum2;
  if (dailyWhBuffer.lastTs != null) {
    const dtHours = Math.min(0.1, Math.max(0, (now - dailyWhBuffer.lastTs) / 3600000));  // cap dt at 6 min
    dailyWhBuffer.envoy1 += s1 * dtHours;
    dailyWhBuffer.envoy2 += s2 * dtHours;
  }
  dailyWhBuffer.lastTs = now;
  if (hasInverters1 && sum1 >= 0) dailyWhBuffer.lastSum1 = sum1;
  if (hasInverters2 && sum2 >= 0) dailyWhBuffer.lastSum2 = sum2;
  return { daily1: dailyWhBuffer.envoy1 / 1000, daily2: dailyWhBuffer.envoy2 / 1000 };
}

router.get('/grid/envoy-inverters', async (req, res) => {
  const cache = require('../services/mqttcache');
  try {
    const mq1 = cache.getEnvoy('envoy1');
    const mq2 = cache.getEnvoy('envoy2');
    let envoy1, envoy2;
    if (mq1 && mq2) {
      envoy1 = mq1;
      envoy2 = mq2;
    } else {
      const data = await fetchEnvoyFromHttp();
      envoy1 = data.envoy1 || {};
      envoy2 = data.envoy2 || {};
    }
    const inv1 = getInverters(envoy1);
    const inv2 = getInverters(envoy2);
    const sum1 = inv1.reduce((s, i) => s + (i.lastReportWatts || 0), 0);
    const sum2 = inv2.reduce((s, i) => s + (i.lastReportWatts || 0), 0);
    const { daily1, daily2 } = integrateDailyWh(sum1, sum2, inv1.length > 0, inv2.length > 0);

    res.json({
      ok: true,
      source: (mq1 && mq2) ? 'mqtt' : 'http',
      envoy1: {
        name: envoy1.name || 'Envoy #1',
        inverters: inv1,
        production_sum_w: sum1,
        daily_kwh: daily1,
        active_count: inv1.filter(i => (i.lastReportWatts || 0) > 0).length,
        total_count: inv1.length,
      },
      envoy2: {
        name: envoy2.name || 'Envoy #2',
        inverters: inv2,
        production_sum_w: sum2,
        daily_kwh: daily2,
        active_count: inv2.filter(i => (i.lastReportWatts || 0) > 0).length,
        total_count: inv2.length,
      },
      total_production_w: sum1 + sum2,
      total_daily_kwh: daily1 + daily2,
    });
  } catch (err) {
    res.status(500).json({ ok: false, error: err.message });
  }
});

// ── Envoy InfluxDB endpoints ──────────────────────────────
//  Uses micro_inverter measurement: tags (envoy_id, serial), fields (watts, max_watts)

function influxEnvoyQuery(flux) {
  const { InfluxDB } = require('@influxdata/influxdb-client');
  const client   = new InfluxDB({ url: config.influxdb.url, token: config.influxdb.token });
  const queryApi = client.getQueryApi(config.influxdb.org);
  return new Promise((resolve, reject) => {
    const rows = [];
    queryApi.queryRows(flux, {
      next(row, meta) { rows.push(meta.toObject(row)); },
      error: reject,
      complete() { resolve(rows); },
    });
  });
}

// GET /api/envoy/data
// Returns latest watts + daily kWh per inverter (from InfluxDB)
// Also merges any fresher live data from the MQTT/HTTP cache
router.get('/envoy/data', async (req, res) => {
  try {
    const bucket = config.influxdb.bucket;

    // Latest watts per inverter (last report within 15 min)
    const liveRows = await influxEnvoyQuery(`
      from(bucket: "${bucket}")
        |> range(start: -15m)
        |> filter(fn: (r) => r._measurement == "micro_inverter" and r._field == "watts")
        |> last()
        |> keep(columns: ["serial", "envoy_id", "_value", "_time"])
    `);

    // Max watts per inverter
    const maxRows = await influxEnvoyQuery(`
      from(bucket: "${bucket}")
        |> range(start: -30d)
        |> filter(fn: (r) => r._measurement == "micro_inverter" and r._field == "max_watts")
        |> last()
        |> keep(columns: ["serial", "envoy_id", "_value"])
    `);

    // Daily kWh per inverter (integral of watts since local midnight, result in Wh)
    const now = new Date();
    const localMidnight    = new Date(now.getFullYear(), now.getMonth(), now.getDate(), 0, 0, 0);
    const prevMidnight     = new Date(localMidnight - 86400000);  // midnight yesterday
    const hoursSinceMidnight  = Math.max(1, Math.ceil((now - localMidnight) / 3600000));
    const hoursInYesterday    = 24;  // full previous day

    const [dailyRows, yestRows] = await Promise.all([
      influxEnvoyQuery(`
        from(bucket: "${bucket}")
          |> range(start: -${hoursSinceMidnight}h)
          |> filter(fn: (r) => r._measurement == "micro_inverter" and r._field == "watts")
          |> integral(unit: 1h)
          |> keep(columns: ["serial", "envoy_id", "_value"])
      `),
      influxEnvoyQuery(`
        from(bucket: "${bucket}")
          |> range(start: ${prevMidnight.toISOString()}, stop: ${localMidnight.toISOString()})
          |> filter(fn: (r) => r._measurement == "micro_inverter" and r._field == "watts")
          |> integral(unit: 1h)
          |> keep(columns: ["serial", "envoy_id", "_value"])
      `),
    ]);

    // Build maps
    const liveMap    = {};  // serial -> { watts, time, envoy_id }
    const maxMap     = {};  // serial -> max_watts
    const dailyMap   = {};  // serial -> today Wh
    const yestMap    = {};  // serial -> yesterday Wh
    const envoyIdMap = {};  // serial -> envoy_id (from any source)

    for (const r of liveRows)  { liveMap[r.serial]  = { watts: r._value, time: r._time, envoy_id: r.envoy_id }; envoyIdMap[r.serial] = r.envoy_id; }
    for (const r of maxRows)   { maxMap[r.serial]   = r._value; envoyIdMap[r.serial] = r.envoy_id; }
    for (const r of dailyRows) { dailyMap[r.serial] = r._value; envoyIdMap[r.serial] = r.envoy_id; }
    for (const r of yestRows)  { yestMap[r.serial]  = r._value; envoyIdMap[r.serial] = r.envoy_id; }

    // Also check cache for any inverter data not yet in InfluxDB
    const cache = require('../services/mqttcache');
    const mergeCache = (envoyId) => {
      const cached = cache.getEnvoy(envoyId);
      if (!cached) return;
      const inv = cached.inverters || cached.raw_envoy_data?.raw_data?.inverters || [];
      for (const i of inv) {
        const s = i.serialNumber || i.serial;
        if (!s) continue;
        if (!liveMap[s] || (i.lastReportWatts != null && !liveMap[s])) {
          liveMap[s] = { watts: i.lastReportWatts ?? 0, envoy_id: envoyId, time: null };
        }
      }
    };
    mergeCache('envoy1');
    mergeCache('envoy2');

    // Build unified inverter list
    const allSerials = new Set([
      ...Object.keys(liveMap),
      ...Object.keys(maxMap),
      ...Object.keys(dailyMap),
    ]);

    const inverters = [];
    for (const serial of allSerials) {
      const live = liveMap[serial] || {};
      inverters.push({
        serial,
        envoy_id:      live.envoy_id || envoyIdMap[serial] || 'unknown',
        watts:         live.watts    ?? 0,
        max_watts:     maxMap[serial]  ?? 0,
        daily_kwh:     (dailyMap[serial] ?? 0) / 1000,
        yesterday_kwh: (yestMap[serial]  ?? 0) / 1000,
        last_time:     live.time || null,
      });
    }

    // Group totals
    const sumField = (arr, field) => arr.reduce((s, inv) => s + (inv[field] || 0), 0);
    const e1 = inverters.filter(i => i.envoy_id === 'envoy1');
    const e2 = inverters.filter(i => i.envoy_id === 'envoy2');

    res.json({
      ok: true,
      inverters,
      envoy1_daily_kwh:     sumField(e1, 'daily_kwh'),
      envoy2_daily_kwh:     sumField(e2, 'daily_kwh'),
      total_daily_kwh:      sumField(inverters, 'daily_kwh'),
      envoy1_yesterday_kwh: sumField(e1, 'yesterday_kwh'),
      envoy2_yesterday_kwh: sumField(e2, 'yesterday_kwh'),
      total_yesterday_kwh:  sumField(inverters, 'yesterday_kwh'),
    });
  } catch (err) {
    res.status(500).json({ ok: false, error: err.message });
  }
});

// GET /api/envoy/history?serials=xxx,yyy&hours=12&points=200
// Returns time-series watts (sum of provided serials) for charting
router.get('/envoy/history', async (req, res) => {
  try {
    const bucket  = config.influxdb.bucket;
    const hours   = Math.min(parseInt(req.query.hours  || '12'), 48);
    const points  = Math.min(parseInt(req.query.points || '200'), 500);
    const serials = (req.query.serials || '').split(',').map(s => s.trim()).filter(Boolean);

    if (!serials.length) return res.json({ ok: true, data: [] });

    const serialFilter = serials.map(s => `r.serial == "${s}"`).join(' or ');
    const windowSec    = Math.max(60, Math.round((hours * 3600) / points));

    const rows = await influxEnvoyQuery(`
      from(bucket: "${bucket}")
        |> range(start: -${hours}h)
        |> filter(fn: (r) => r._measurement == "micro_inverter" and r._field == "watts")
        |> filter(fn: (r) => ${serialFilter})
        |> aggregateWindow(every: ${windowSec}s, fn: mean, createEmpty: false)
        |> group()
        |> aggregateWindow(every: ${windowSec}s, fn: sum, createEmpty: false)
        |> keep(columns: ["_time", "_value"])
    `);

    const data = rows.map(r => ({ time: r._time, value: r._value || 0 }));
    res.json({ ok: true, hours, points: data.length, data });
  } catch (err) {
    res.status(500).json({ ok: false, error: err.message });
  }
});

// ── Utility ───────────────────────────────────────────────

router.get('/config', async (req, res) => {
  try {
    const defaultBattery = typeof db.getDefaultBattery === 'function' ? db.getDefaultBattery() : DEFAULT_BATTERY;
    const batterySources = typeof db.getBatterySources === 'function'
      ? await db.getBatterySources()
      : {};
    const defaultSource = batterySources[defaultBattery] || null;
    res.json({
      ok:                 true,
      cellCount:          config.battery.cellCount,
      liveRefreshSeconds: config.app.liveRefreshSeconds,
      historyHours:       config.app.historyHours,
      defaultBattery,
      batterySourcesEndpoint: '/api/battery-sources',
      batterySources,
      isMockData: !!defaultSource?.isMock,
      hasMockBatterySources: Object.values(batterySources).some((source) => !!source?.isMock),
    });
  } catch (err) {
    res.status(500).json({ ok: false, error: err.message });
  }
});

router.get('/debug', async (req, res) => {
  if (config.app.useMockData)
    return res.json({ ok: true, message: 'Mock mode active' });
  try {
    const influxdb = require('../services/influxdb');
    res.json({ ok: true, ...(await influxdb.debugSchema()) });
  } catch (err) { res.status(500).json({ ok: false, error: err.message }); }
});

module.exports = router;

// ── GET /api/debug/solark  ─────────────────────────────────
// Shows raw solarkCache values so you can see what MQTT is delivering
router.get('/debug/solark', (req, res) => {
  const cache = require('../services/mqttcache');
  const now = Date.now();
  const entries = Object.entries(cache.solarkCache).map(([k, v]) => ({
    key: k, value: v.value, age_s: Math.round((now - v.ts) / 1000),
  }));
  entries.sort((a, b) => a.key.localeCompare(b.key));
  res.json({ ok: true, count: entries.length, entries });
});

// ── GET /api/debug/sensor?id=soc ─────────────────────────
// Shows raw InfluxDB rows for any sensor entity_id
router.get('/debug/sensor', async (req, res) => {
  const id = req.query.id;
  if (!id) return res.status(400).json({ ok: false, error: 'Missing ?id=' });
  try {
    const sensors = require('../sensors');
    const sensorDef = sensors.find(s => s.id === id);
    const entityId  = sensorDef ? sensorDef.entityId : id;
    const sensorData = require('../services/sensordata');
    const rows = await sensorData.debugSensor(entityId);
    res.json({ ok: true, entityId, rows });
  } catch (err) {
    res.status(500).json({ ok: false, error: err.message });
  }
});

// ── GET /api/debug/search?q=contactor ──────────────────────
// Search for entity_ids matching a string across ALL measurements
router.get('/debug/search', async (req, res) => {
  const q = (req.query.q || '').toLowerCase();
  if (!q) return res.status(400).json({ ok: false, error: 'Missing ?q=' });
  try {
    const { InfluxDB } = require('@influxdata/influxdb-client');
    const config = require('../config');
    const client = new InfluxDB({ url: config.influxdb.url, token: config.influxdb.token });
    const queryApi = client.getQueryApi(config.influxdb.org);

    const flux = `
      from(bucket: "${config.influxdb.bucket}")
        |> range(start: -24h)
        |> keep(columns: ["entity_id", "_measurement", "_field"])
        |> distinct(column: "entity_id")
    `;

    const rows = await new Promise((resolve, reject) => {
      const r = [];
      queryApi.queryRows(flux, {
        next(row, meta) { r.push(meta.toObject(row)); },
        error: reject,
        complete() { resolve(r); },
      });
    });

    // Filter to matching entity_ids and deduplicate
    const seen = new Set();
    const matches = [];
    for (const row of rows) {
      const eid = row.entity_id || row._value || '';
      if (eid.toLowerCase().includes(q) && !seen.has(eid)) {
        seen.add(eid);
        matches.push({ entity_id: eid, measurement: row._measurement });
      }
    }
    res.json({ ok: true, query: q, matches });
  } catch (err) {
    res.status(500).json({ ok: false, error: err.message });
  }
});
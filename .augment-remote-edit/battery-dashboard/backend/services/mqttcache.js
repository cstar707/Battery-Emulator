// ─────────────────────────────────────────────────────────
//  services/mqttcache.js
//
//  Subscribes to MQTT topics and caches the latest values.
//  Also polls the Solis API periodically.
//  Provides: getLatest(key), getAllSolark(), getAllSolis()
// ─────────────────────────────────────────────────────────

const mqtt   = require('mqtt');
const http   = require('http');
const config = require('../config');

// In-memory caches
const solarkCache = {};   // key: topic suffix, value: { value, ts }
const solisCache  = {};   // key: sensor id, value: { value, ts }
const cellCaches  = { tesla: {}, ruxiu: {} }; // per-battery cell cache
const teslaBmsCache = {}; // key: e.g. temperature_min, temperature_max (from BE/info)
const envoyCache  = { envoy1: null, envoy2: null }; // { data, ts } per envoy

// Track last time each key was written by the JSON topic (authoritative source)
const solarkJsonTs = {};  // key -> timestamp when JSON topic last wrote it

let mqttConnected = false;
let solisLastPoll = 0;
let dynamicSubscriptions = new Set();

function getCellSource(battery) {
  return (config.batterySources && config.batterySources[battery]) || null;
}

function getCellCache(battery = 'tesla') {
  if (!cellCaches[battery]) cellCaches[battery] = {};
  return cellCaches[battery];
}

function uniqueTopics(topics) {
  return [...new Set(
    topics
      .filter(Boolean)
      .map((topic) => String(topic).trim())
      .filter(Boolean)
  )];
}

function getTeslaInfoTopics() {
  return uniqueTopics([config.mqtt.topicTeslaBe, config.mqtt.compatTeslaInfoTopic]);
}

function getTeslaSpecTopics() {
  return uniqueTopics([getCellSource('tesla')?.specTopic, config.mqtt.topicBeSpec, config.mqtt.compatTeslaSpecTopic]);
}

function getTeslaBalancingTopics() {
  return uniqueTopics([getCellSource('tesla')?.balancingTopic, config.mqtt.topicBeBalancing, config.mqtt.compatTeslaBalancingTopic]);
}

function subscribeIfPresent(topic) {
  if (topic) client.subscribe(topic, { qos: 0 });
}

function subscribeDynamicTopic(topic) {
  if (!topic || !mqttConnected) return;
  if (dynamicSubscriptions.has(topic)) return;
  dynamicSubscriptions.add(topic);
  client.subscribe(topic, { qos: 0 });
}

function refreshDynamicSubscriptions() {
  if (!mqttConnected) return;
  uniqueTopics([
    config.mqtt.topicSolarkJson,
    ...getTeslaInfoTopics(),
    ...getTeslaSpecTopics(),
    ...getTeslaBalancingTopics(),
    config.mqtt.compatTopicEnvoy1Power,
    config.mqtt.compatTopicEnvoy2Power,
  ]).forEach(subscribeDynamicTopic);

  for (const battery of ['tesla', 'ruxiu']) {
    const source = getCellSource(battery);
    if (source?.type !== 'mqtt') continue;
    [source.infoTopic, source.specTopic, source.balancingTopic]
      .filter(Boolean)
      .forEach(subscribeDynamicTopic);
  }
}

function upsertCell(battery, cellNumber, partial) {
  const cache = getCellCache(battery);
  const current = cache[cellNumber] || { voltage_mv: 0, balancing: false, ts: Date.now() };
  cache[cellNumber] = { ...current, ...partial, ts: partial.ts || Date.now() };
}

function applyCellPayload(battery, rawValue) {
  try {
    const data = JSON.parse(rawValue);
    if (Array.isArray(data)) {
      const ts = Date.now();
      for (const cell of data) {
        if (cell && cell.cell != null) {
          upsertCell(battery, cell.cell, {
            voltage_mv: Math.round(cell.mv || cell.voltage_mv || 0),
            balancing: !!cell.balancing,
            ts,
          });
        }
      }
      return true;
    }
    if (data && Array.isArray(data.cell_voltages)) {
      const ts = Date.now();
      data.cell_voltages.forEach((value, index) => {
        if (typeof value === 'number') {
          upsertCell(battery, index + 1, { voltage_mv: Math.round(value * 1000), ts });
        }
      });
      return true;
    }
  } catch {
    const parsed = rawValue.split(',').map((mv) => parseInt(mv.trim(), 10)).filter((mv) => !Number.isNaN(mv));
    if (parsed.length) {
      const ts = Date.now();
      parsed.forEach((mv, index) => upsertCell(battery, index + 1, { voltage_mv: mv, ts }));
      return true;
    }
  }
  return false;
}

function applyBalancingPayload(battery, rawValue) {
  try {
    const data = JSON.parse(rawValue);
    if (!Array.isArray(data.cell_balancing)) return false;
    const ts = Date.now();
    data.cell_balancing.forEach((balancing, index) => {
      upsertCell(battery, index + 1, { balancing: !!balancing, ts });
    });
    return true;
  } catch {
    return false;
  }
}

function mergeEnvoyData(existing, patch) {
  if (existing && typeof existing === 'object' && !Array.isArray(existing)) {
    return { ...existing, ...patch };
  }
  return { ...patch };
}

function applyEnvoyPayload(envoyId, rawValue, now) {
  try {
    const data = JSON.parse(rawValue);
    if (data && typeof data === 'object' && !Array.isArray(data)) {
      envoyCache[envoyId] = { data, ts: now };
      return true;
    }
    if (typeof data === 'number') {
      envoyCache[envoyId] = {
        data: mergeEnvoyData(envoyCache[envoyId]?.data, {
          production: data,
          active_power: data,
        }),
        ts: now,
      };
      return true;
    }
  } catch {}
  return false;
}

function applyEnvoyActivePower(envoyId, rawValue, topic, now) {
  let watts = Number(rawValue);
  if (!Number.isFinite(watts)) {
    try {
      const data = JSON.parse(rawValue);
      watts = Number(data?.production ?? data?.active_power ?? data?.value ?? data);
    } catch {}
  }
  if (!Number.isFinite(watts)) return false;
  envoyCache[envoyId] = {
    data: mergeEnvoyData(envoyCache[envoyId]?.data, {
      production: watts,
      active_power: watts,
      source: 'mqtt-active-power',
      power_topic: topic,
    }),
    ts: now,
  };
  return true;
}

// ── MQTT ──────────────────────────────────────────────────

const client = mqtt.connect(config.mqtt.url, {
  username:      config.mqtt.username,
  password:      config.mqtt.password,
  reconnectPeriod: 5000,
  connectTimeout:  10000,
});

client.on('connect', () => {
  mqttConnected = true;
  dynamicSubscriptions = new Set();
  uniqueTopics([
    config.mqtt.topicSolark,
    config.mqtt.topicSolarkJson,
    ...getTeslaInfoTopics(),
    ...getTeslaSpecTopics(),
    ...getTeslaBalancingTopics(),
    config.mqtt.topicCells,
    config.mqtt.topicEnvoy1,
    config.mqtt.topicEnvoy2,
    config.mqtt.compatTopicEnvoy1Power,
    config.mqtt.compatTopicEnvoy2Power,
  ]).forEach(subscribeIfPresent);
  refreshDynamicSubscriptions();
  console.log('[mqtt] connected, subscribed to solark + BE compatibility + envoy topics');
});

client.on('error',      (e) => console.warn('[mqtt] error:', e.message));
client.on('close',      ()  => { mqttConnected = false; });
client.on('reconnect',  ()  => console.log('[mqtt] reconnecting...'));

client.on('message', (topic, payload) => {
  const val = payload.toString().trim();
  const now = Date.now();

  // BE/info — Battery Emulator summary (SOC, temps, voltages, power)
  if (getTeslaInfoTopics().includes(topic)) {
    try {
      const data = JSON.parse(val);
      const keys = [
        'temperature_min', 'temperature_max', 'battery_voltage', 'battery_current',
        'SOC', 'SOC_real', 'stat_batt_power', 'cell_max_voltage', 'cell_min_voltage',
        'cell_voltage_delta', 'remaining_capacity', 'total_capacity', 'state_of_health',
        'max_charge_power', 'max_discharge_power',
      ];
      for (const k of keys) {
        const v = data[k];
        if (v != null && typeof v === 'number') teslaBmsCache[k] = { value: v, ts: now };
      }
      if (data.bms_status)      teslaBmsCache['bms_status']      = { value: data.bms_status,      ts: now };
      if (data.emulator_status) teslaBmsCache['emulator_status'] = { value: data.emulator_status, ts: now };
    } catch {}
    return;
  }

  const teslaCellSource = getCellSource('tesla');
  if (teslaCellSource?.type === 'mqtt' && getTeslaSpecTopics().includes(topic)) {
    if (applyCellPayload('tesla', val)) return;
  }

  if (teslaCellSource?.type === 'mqtt' && getTeslaBalancingTopics().includes(topic)) {
    if (applyBalancingPayload('tesla', val)) return;
  }

  const ruxiuCellSource = getCellSource('ruxiu');
  if (ruxiuCellSource?.type === 'mqtt' && ruxiuCellSource.specTopic && topic === ruxiuCellSource.specTopic) {
    if (applyCellPayload('ruxiu', val)) return;
  }

  if (ruxiuCellSource?.type === 'mqtt' && ruxiuCellSource.balancingTopic && topic === ruxiuCellSource.balancingTopic) {
    if (applyBalancingPayload('ruxiu', val)) return;
  }

  // solar/solark — full JSON from Battery-Emulator or unified server
  if (config.mqtt.topicSolarkJson && topic === config.mqtt.topicSolarkJson) {
    try {
      const data = JSON.parse(val);
      const _n = (v) => (v != null && v !== '' ? Number(v) : null);
      const _w = (k) => _n(data[k]) ?? (typeof data[k]?.value === 'number' ? data[k].value : null);

      // Power (W)
      if (data.battery_soc_pptt != null) solarkCache['battery_soc'] = { value: data.battery_soc_pptt / 100, ts: now };
      if (data.battery_voltage_dV != null) solarkCache['battery_voltage'] = { value: data.battery_voltage_dV / 10, ts: now };
      if (data.battery_current_dA != null) solarkCache['battery_current'] = { value: data.battery_current_dA / 10, ts: now };
      const battW = _w('total_battery_power_W') ?? _w('total_battery_power') ?? _w('battery_power_W') ?? _w('battery_power');
      if (battW != null) solarkCache['battery_power'] = { value: battW, ts: now };
      const gridW = _w('grid_power_W') ?? _w('grid_power');
      if (gridW != null) solarkCache['grid_power'] = { value: gridW, ts: now };
      const loadW = _w('load_power_W') ?? _w('load_power');
      if (loadW != null) solarkCache['load_power'] = { value: loadW, ts: now };
      const pvW = _w('pv_power_W') ?? _w('pv_power') ?? _w('solar_power') ?? _w('total_solar_power');
      if (pvW != null) solarkCache['total_solar_power'] = { value: pvW, ts: now };
      const gridCtW = _w('grid_ct_power_W') ?? _w('grid_ct_power') ?? _w('total_grid_ct_power');
      if (gridCtW != null) solarkCache['grid_ct_power'] = { value: gridCtW, ts: now };
      const invW = _w('inverter_power_W') ?? _w('inverter_power') ?? _w('total_inverter_power');
      if (invW != null) solarkCache['inverter_power'] = { value: invW, ts: now };
      const invA = _n(data.inverter_current_dA) != null ? data.inverter_current_dA / 10 : _n(data.inverter_current) ?? _n(data.total_inverter_current);
      if (invA != null) solarkCache['inverter_current'] = { value: invA, ts: now };
      const battA = _n(data.battery_current_dA) != null ? data.battery_current_dA / 10 : _n(data.total_battery_current) ?? _n(data.battery_current);
      if (battA != null) solarkCache['total_battery_current'] = { value: battA, ts: now };

      // Energy (kWh) — today (JSON topic is authoritative; record ts so sensor topics don't overwrite)
      const dayPv = _n(data.day_pv_energy_kWh) ?? _n(data.total_day_pv_energy) ?? _n(data.day_pv_energy);
      if (dayPv != null) { solarkCache['day_pv_energy'] = { value: dayPv, ts: now }; solarkJsonTs['day_pv_energy'] = now; }
      const dayBattCh = _n(data.day_batt_charge_kWh) ?? _n(data.total_day_battery_charge) ?? _n(data.day_battery_charge);
      if (dayBattCh != null) { solarkCache['day_battery_charge'] = { value: dayBattCh, ts: now }; solarkJsonTs['day_battery_charge'] = now; }
      const dayBattDis = _n(data.day_batt_discharge_kWh) ?? _n(data.day_batt_dis_kWh) ?? _n(data.total_day_battery_discharge) ?? _n(data.day_battery_discharge);
      if (dayBattDis != null) { solarkCache['day_battery_discharge'] = { value: dayBattDis, ts: now }; solarkJsonTs['day_battery_discharge'] = now; }
      const dayLoad = _n(data.day_load_energy_kWh) ?? _n(data.total_day_load_energy) ?? _n(data.day_load_energy);
      if (dayLoad != null) { solarkCache['day_load_energy'] = { value: dayLoad, ts: now }; solarkJsonTs['day_load_energy'] = now; }

      // Energy (kWh) — lifetime
      const totBattCh = _n(data.total_batt_charge_kWh) ?? _n(data.total_battery_charge);
      if (totBattCh != null) solarkCache['total_battery_charge'] = { value: totBattCh, ts: now };
      const totBattDis = _n(data.total_batt_dis_kWh) ?? _n(data.total_batt_discharge_kWh) ?? _n(data.total_battery_discharge);
      if (totBattDis != null) solarkCache['total_battery_discharge'] = { value: totBattDis, ts: now };
      const totLoad = _n(data.total_load_energy_kWh) ?? _n(data.total_load_energy);
      if (totLoad != null) solarkCache['total_load_energy'] = { value: totLoad, ts: now };
      const totPv = _n(data.total_pv_energy_kWh) ?? _n(data.total_pv_energy);
      if (totPv != null) solarkCache['total_pv_energy'] = { value: totPv, ts: now };
      const totGridExp = _n(data.total_grid_export_kWh) ?? _n(data.total_grid_export);
      if (totGridExp != null) solarkCache['total_grid_export'] = { value: totGridExp, ts: now };
      const totGridImp = _n(data.total_grid_import_kWh) ?? _n(data.total_grid_import);
      if (totGridImp != null) solarkCache['total_grid_import'] = { value: totGridImp, ts: now };

      if (data.battery_temperature != null) {
        solarkCache['battery_temperature'] = { value: data.battery_temperature, ts: now };
      } else if (Array.isArray(data.raw_registers) && data.raw_registers[15] != null) {
        const t = (data.raw_registers[15] - 1000) * 0.1;
        solarkCache['battery_temperature'] = { value: t, ts: now };
      }
    } catch {}
    return;
  }

  // solar/solark/sensors/<key> — map SunSynk entity names to our canonical keys
  const solarkPrefix = 'solar/solark/sensors/';
  const sunsynkToKey = {
    sunsynk_total_battery_charge: 'total_battery_charge', sunsynk_total_battery_current: 'total_battery_current',
    sunsynk_total_battery_discharge: 'total_battery_discharge', sunsynk_total_battery_power: 'battery_power',
    sunsynk_total_day_battery_charge: 'day_battery_charge', sunsynk_total_day_battery_discharge: 'day_battery_discharge',
    sunsynk_total_day_load_energy: 'day_load_energy', sunsynk_total_day_pv_energy: 'day_pv_energy',
    sunsynk_total_grid_ct_power: 'grid_ct_power', sunsynk_total_grid_export: 'total_grid_export',
    sunsynk_total_grid_import: 'total_grid_import', sunsynk_total_inverter_current: 'inverter_current',
    sunsynk_total_inverter_power: 'inverter_power', sunsynk_total_load_energy: 'total_load_energy',
    sunsynk_total_pv_energy: 'total_pv_energy', sunsynk_total_solar_power: 'total_solar_power',
    sunsynk_battery_soc: 'battery_soc', sunsynk_battery_voltage: 'battery_voltage',
    sunsynk_grid_power: 'grid_power', sunsynk_load_power: 'load_power',
  };
  if (topic.startsWith(solarkPrefix)) {
    const key = topic.slice(solarkPrefix.length);
    let v = parseFloat(val);
    if (isNaN(v) && (val.startsWith('{') || val.startsWith('['))) {
      try { const o = JSON.parse(val); v = Number(o.value ?? o.state ?? o); } catch {}
    }
    if (typeof v !== 'number' || isNaN(v)) v = val;
    const canonical = sunsynkToKey[key] || key.replace(/^sunsynk_/, '');
    // If JSON topic is configured and wrote this canonical key recently, skip sensor topic overwrite
    const JSON_AUTHORITY_MS = 30000;
    const jsonWroteRecently = solarkJsonTs[canonical] && (Date.now() - solarkJsonTs[canonical]) < JSON_AUTHORITY_MS;
    if (!jsonWroteRecently) {
      solarkCache[key] = { value: v, ts: Date.now() };
      if (canonical !== key) solarkCache[canonical] = { value: v, ts: Date.now() };
    }
    return;
  }

  // Envoy MQTT compatibility — accept full retained JSON plus active-power aliases
  if (config.mqtt.topicEnvoy1 && topic === config.mqtt.topicEnvoy1) {
    if (applyEnvoyPayload('envoy1', val, now)) return;
  }
  if (config.mqtt.topicEnvoy2 && topic === config.mqtt.topicEnvoy2) {
    if (applyEnvoyPayload('envoy2', val, now)) return;
  }
  if (config.mqtt.compatTopicEnvoy1Power && topic === config.mqtt.compatTopicEnvoy1Power) {
    if (applyEnvoyActivePower('envoy1', val, topic, now)) return;
  }
  if (config.mqtt.compatTopicEnvoy2Power && topic === config.mqtt.compatTopicEnvoy2Power) {
    if (applyEnvoyActivePower('envoy2', val, topic, now)) return;
  }

  // Legacy: solar/bms/cells  (JSON array [{cell:1,mv:3320},…] or CSV mV)
  // Preserve compatibility by treating the legacy feed as Tesla when it differs
  // from the configured Tesla per-cell topic.
  if (topic === config.mqtt.topicCells && topic !== getCellSource('tesla')?.specTopic && topic !== getCellSource('ruxiu')?.specTopic) {
    if (applyCellPayload('tesla', val)) return;
  }
});

// ── Solis API poller ──────────────────────────────────────

function pollSolis() {
  const url = config.solis.apiUrl + '/api/sensors';
  http.get(url, (res) => {
    let body = '';
    res.on('data', d => body += d);
    res.on('end', () => {
      try {
        const data = JSON.parse(body);
        const now  = Date.now();
        const map  = {
          solis_pv_power:       data.pv_power_W,
          solis_battery_soc:    data.battery_soc_pct,
          solis_battery_voltage: data.battery_voltage_V,
          solis_battery_current: data.battery_current_A,
          solis_battery_power:  data.battery_power_W,
          solis_grid_power:     data.grid_power_W,
          solis_load_power:     data.load_power_W,
          solis_inverter_temp:  data.inverter_temp_C,
          solis_today_pv_kwh:   data.energy_today_pv_kWh,
          solis_today_grid_import_kwh: data.energy_today_grid_import_kWh,
          solis_today_grid_export_kwh: data.energy_today_grid_export_kWh,
          solis_battery_voltage: data.battery_voltage_V,
          solis_battery_current: data.battery_current_A,
          solis_battery_power: data.battery_power_W,
          solis_load_power: data.load_power_W,
          solis_today_pv_kwh: data.energy_today_pv_kWh,
          solis_today_grid_export_kwh: data.energy_today_grid_export_kWh,
        };
        for (const [k, v] of Object.entries(map)) {
          if (v != null) solisCache[k] = { value: v, ts: now };
        }
      } catch {}
    });
  }).on('error', () => {});
}

setInterval(refreshDynamicSubscriptions, 10000);

// Poll Solis every 6 seconds
setInterval(pollSolis, 6000);
pollSolis();

// ── Envoy HTTP poller (background, so API requests return instantly) ──────────
// Polls /api/envoy/debug on ENVOY_API_URL every 30s; stores into envoyCache
// so getEnvoy() returns fresh data without blocking the request handler.

function pollEnvoyHttp() {
  const base = (config.envoy?.apiUrl || 'http://localhost:3004').replace(/\/$/, '');
  const url  = `${base}/api/envoy/debug`;
  const mod  = url.startsWith('https') ? require('https') : http;
  mod.get(url, { timeout: 25000 }, (res) => {
    let body = '';
    res.on('data', d => body += d);
    res.on('end', () => {
      try {
        const d   = JSON.parse(body);
        const now = Date.now();
        if (d.envoy1) envoyCache.envoy1 = { data: d.envoy1, ts: now };
        if (d.envoy2) envoyCache.envoy2 = { data: d.envoy2, ts: now };
      } catch {}
    });
  }).on('error', () => {}).on('timeout', function() { this.destroy(); });
}

// Start polling after a short delay (let the server finish starting up first)
setTimeout(() => {
  pollEnvoyHttp();
  setInterval(pollEnvoyHttp, 30000);
}, 3000);

// ── Public API ────────────────────────────────────────────

function getSolark(key)     { return solarkCache[key]     || null; }
function getSolis(key)      { return solisCache[key]      || null; }
function getTeslaBms(key)   { return teslaBmsCache[key]   || null; }
function getCells(battery = 'tesla') { return getCellCache(battery); }
function getEnvoy(id)       { const c = envoyCache[id]; return c && (Date.now() - c.ts) < 120000 ? c.data : null; }
function isConnected()      { return mqttConnected; }

module.exports = { getSolark, getSolis, getTeslaBms, getCells, getEnvoy, isConnected, solarkCache, solisCache, teslaBmsCache, envoyCache, cellCaches };

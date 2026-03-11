// ─────────────────────────────────────────────────────────────────────────────
//  services/influxdb.js
//
//  Provides per-battery cell data from MQTT cache (live) and InfluxDB (Tesla
//  history fallback). Tesla preserves the current BE live behavior; Ruxiu can
//  run as an explicit mock source until a dedicated live source is configured.
// ─────────────────────────────────────────────────────────────────────────────

const { InfluxDB } = require('@influxdata/influxdb-client');
const config = require('../config');
const cache  = require('./mqttcache');

const CELL_COUNT = config.battery.cellCount;
const BATTERY_DISPLAY_NAMES = {
  tesla: 'Tesla Battery',
  ruxiu: 'Ruxiu Batteries',
};
const VALID_BATTERIES = new Set(['tesla', 'ruxiu']);
const SOURCE_STALE_MS = 30_000;

let _queryApi = null;
function getQueryApi() {
  if (_queryApi) return _queryApi;
  if (!config.influxdb.token || !config.influxdb.url) return null;
  const client = new InfluxDB({ url: config.influxdb.url, token: config.influxdb.token });
  _queryApi = client.getQueryApi(config.influxdb.org);
  return _queryApi;
}

async function influxQuery(flux) {
  const api = getQueryApi();
  if (!api) throw new Error('InfluxDB not configured');
  return new Promise((resolve, reject) => {
    const rows = [];
    api.queryRows(flux, {
      next:     (row, meta) => rows.push(meta.toObject(row)),
      error:    reject,
      complete: () => resolve(rows),
    });
  });
}

function getDefaultBattery() {
  const configured = String(config.battery?.defaultSourceBattery || 'tesla').toLowerCase();
  return VALID_BATTERIES.has(configured) ? configured : 'tesla';
}

function normalizeBattery(battery = getDefaultBattery()) {
  const normalized = String(battery || getDefaultBattery()).toLowerCase();
  return VALID_BATTERIES.has(normalized) ? normalized : getDefaultBattery();
}

function getBatterySourceConfig(battery = getDefaultBattery()) {
  const normalized = normalizeBattery(battery);
  const configured = (config.batterySources && config.batterySources[normalized]) || {};
  return {
    battery: normalized,
    displayName: BATTERY_DISPLAY_NAMES[normalized] || normalized,
    enabled: configured.enabled !== false,
    type: configured.type || (normalized === 'tesla' ? 'mqtt' : 'mock'),
    specTopic: configured.specTopic || null,
    balancingTopic: configured.balancingTopic || null,
    infoTopic: configured.infoTopic || null,
    host: configured.host || null,
    port: configured.port || null,
    basePath: configured.basePath || '/',
  };
}

function buildEndpoints(battery) {
  const normalized = normalizeBattery(battery);
  return {
    latest: `/api/cells/latest?battery=${normalized}`,
    at: `/api/cells/at?t={timestamp}&battery=${normalized}`,
    contract: '/api/battery-sources',
  };
}

function buildEndpointDescriptor(sourceConfig) {
  if (sourceConfig.type === 'mqtt') {
    return {
      kind: 'mqtt',
      infoTopic: sourceConfig.infoTopic,
      specTopic: sourceConfig.specTopic,
      balancingTopic: sourceConfig.balancingTopic,
    };
  }
  if (sourceConfig.type === 'direct_http') {
    return {
      kind: 'direct_http',
      host: sourceConfig.host,
      port: sourceConfig.port,
      basePath: sourceConfig.basePath || '/',
    };
  }
  return { kind: sourceConfig.type || 'unknown' };
}

function buildHistoryDescriptor(sourceConfig) {
  const endpoints = buildEndpoints(sourceConfig.battery);
  if (!sourceConfig.enabled) {
    return {
      available: false,
      backedByStoredData: false,
      mode: 'disabled',
      endpoint: endpoints.at,
      note: 'Source is disabled.',
    };
  }
  if (sourceConfig.type === 'mock') {
    return {
      available: true,
      backedByStoredData: false,
      mode: 'synthetic-mock',
      endpoint: endpoints.at,
      note: 'Returns synthetic mock cells at the requested timestamp; no stored historical feed is configured.',
    };
  }
  if (sourceConfig.type === 'direct_http') {
    return {
      available: false,
      backedByStoredData: false,
      mode: 'not-configured',
      endpoint: endpoints.at,
      note: 'Direct HTTP source settings are stored, but the live adapter is not implemented on :3008 yet.',
    };
  }
  if (sourceConfig.battery === 'tesla') {
    return {
      available: true,
      backedByStoredData: true,
      mode: 'recorded-influxdb',
      endpoint: endpoints.at,
      note: 'Historical Tesla cell requests use recorded InfluxDB data when available.',
    };
  }
  return {
    available: false,
    backedByStoredData: false,
    mode: 'not-configured',
    endpoint: endpoints.at,
    note: 'Historical cells are not configured for this source.',
  };
}

function deriveStatus(sourceConfig, details = {}) {
  if (!sourceConfig.enabled) return 'disabled';
  if (details.error) return 'error';
  if (sourceConfig.type === 'mock') return details.phase === 'historical' ? 'mock-history' : 'mock';
  if (sourceConfig.type === 'direct_http') return 'error';
  if (details.freshnessMs != null && details.freshnessMs > SOURCE_STALE_MS) return 'stale';
  if (details.source === 'mqtt' && details.isLive) return 'live';
  if (details.source === 'influxdb' && details.phase === 'historical') return 'historical';
  if (details.source === 'influxdb') return 'fallback';
  if (details.source === 'none' || !details.source) return 'unavailable';
  return details.source;
}

function buildSourceContract(battery, details = {}) {
  const sourceConfig = getBatterySourceConfig(battery);
  const endpoints = buildEndpoints(sourceConfig.battery);
  const endpoint = buildEndpointDescriptor(sourceConfig);
  const error = details.error || null;
  const status = deriveStatus(sourceConfig, details);
  const freshAt = details.freshAt ?? null;
  const freshnessMs = details.freshnessMs ?? null;
  const history = buildHistoryDescriptor(sourceConfig);

  return {
    battery: sourceConfig.battery,
    displayName: sourceConfig.displayName,
    enabled: sourceConfig.enabled,
    sourceType: sourceConfig.type,
    type: sourceConfig.type,
    isDefault: sourceConfig.battery === getDefaultBattery(),
    status,
    error,
    lastError: error,
    isLive: !!details.isLive,
    isMock: !!details.isMock,
    activeSource: details.source || (sourceConfig.enabled ? sourceConfig.type : 'none'),
    freshAt,
    freshnessMs,
    freshness: {
      freshAt,
      freshnessMs,
    },
    endpoints,
    endpoint,
    inputs: {
      mode: sourceConfig.type,
      specTopic: sourceConfig.type === 'mqtt' ? sourceConfig.specTopic : null,
      balancingTopic: sourceConfig.type === 'mqtt' ? sourceConfig.balancingTopic : null,
      infoTopic: sourceConfig.type === 'mqtt' ? sourceConfig.infoTopic : null,
      host: sourceConfig.type === 'direct_http' ? sourceConfig.host : null,
      port: sourceConfig.type === 'direct_http' ? sourceConfig.port : null,
      basePath: sourceConfig.type === 'direct_http' ? (sourceConfig.basePath || '/') : null,
    },
    history,
  };
}

function buildMockCells(isoTimestamp = new Date().toISOString()) {
  const now = Date.parse(isoTimestamp) || Date.now();
  const phase = now / (5 * 60 * 1000);
  return Array.from({ length: CELL_COUNT }, (_, index) => {
    const cell = index + 1;
    const waveform = Math.sin((phase + cell) / 6) * 12;
    const spread = Math.sin(cell / 8) * 18;
    const ripple = Math.cos((phase / 3) + (cell / 11)) * 5;
    return {
      cell,
      voltage_mv: Math.round(3325 + waveform + spread + ripple),
      balancing: false,
    };
  });
}

function liveCellSnapshot(battery = getDefaultBattery()) {
  const raw = cache.getCells(normalizeBattery(battery));
  const entries = Object.entries(raw || {});
  if (!entries.length) return null;

  let freshestTs = 0;
  const cells = entries
    .map(([n, v]) => {
      freshestTs = Math.max(freshestTs, v.ts || 0);
      return {
        cell: parseInt(n, 10),
        voltage_mv: v.voltage_mv,
        balancing: !!v.balancing,
      };
    })
    .filter((cell) => !Number.isNaN(cell.cell))
    .sort((a, b) => a.cell - b.cell);

  return {
    cells,
    freshAt: freshestTs ? new Date(freshestTs).toISOString() : new Date().toISOString(),
    freshnessMs: freshestTs ? Math.max(0, Date.now() - freshestTs) : null,
  };
}

async function getLatestCells(battery = getDefaultBattery()) {
  const normalized = normalizeBattery(battery);
  const sourceConfig = getBatterySourceConfig(normalized);
  const nowIso = new Date().toISOString();

  if (!sourceConfig.enabled) {
    const sourceMeta = buildSourceContract(normalized, {
      phase: 'latest',
      source: 'none',
      isLive: false,
      isMock: false,
      freshAt: null,
      freshnessMs: null,
      error: 'Source is disabled.',
    });
    return {
      battery: normalized,
      cells: [],
      timestamp: nowIso,
      isLive: false,
      isMock: false,
      source: 'none',
      sourceMeta,
    };
  }

  if (sourceConfig.type === 'mock') {
    const sourceMeta = buildSourceContract(normalized, {
      phase: 'latest',
      source: 'mock',
      isLive: true,
      isMock: true,
      freshAt: nowIso,
      freshnessMs: 0,
    });
    return {
      battery: normalized,
      cells: buildMockCells(nowIso),
      timestamp: nowIso,
      isLive: true,
      isMock: true,
      source: 'mock',
      sourceMeta,
    };
  }

  if (sourceConfig.type === 'direct_http') {
    const target = `${sourceConfig.host || '(not set)'}:${sourceConfig.port || 80}${sourceConfig.basePath || '/'}`;
    const sourceMeta = buildSourceContract(normalized, {
      phase: 'latest',
      source: 'direct_http',
      isLive: false,
      isMock: false,
      freshAt: null,
      freshnessMs: null,
      error: `Direct HTTP source configured for ${normalized} at ${target}, but no live adapter is implemented on :3008 yet.`,
    });
    return {
      battery: normalized,
      cells: [],
      timestamp: nowIso,
      isLive: false,
      isMock: false,
      source: 'direct_http',
      sourceMeta,
    };
  }

  const live = liveCellSnapshot(normalized);
  if (live && live.cells.length > 0) {
    const sourceMeta = buildSourceContract(normalized, {
      phase: 'latest',
      source: 'mqtt',
      isLive: true,
      isMock: false,
      freshAt: live.freshAt,
      freshnessMs: live.freshnessMs,
    });
    return {
      battery: normalized,
      cells: live.cells,
      timestamp: live.freshAt,
      isLive: true,
      isMock: false,
      source: 'mqtt',
      sourceMeta,
    };
  }

  if (normalized === 'tesla') {
    try {
      const cells = await queryCellsFlux(
        `from(bucket: "${config.influxdb.bucket}")
           |> range(start: -1h)
           |> filter(fn: (r) => r._measurement == "bms_cell" and r._field == "voltage_mv")
           |> last()`
      );
      if (cells.length > 0) {
        const sourceMeta = buildSourceContract(normalized, {
          phase: 'latest',
          source: 'influxdb',
          isLive: false,
          isMock: false,
          freshAt: null,
          freshnessMs: null,
        });
        return {
          battery: normalized,
          cells,
          timestamp: nowIso,
          isLive: false,
          isMock: false,
          source: 'influxdb',
          sourceMeta,
        };
      }
    } catch (e) {
      console.warn('[influxdb] getLatestCells fallback failed:', e.message);
    }
  }

  const sourceMeta = buildSourceContract(normalized, {
    phase: 'latest',
    source: 'none',
    isLive: false,
    isMock: false,
    freshAt: null,
    freshnessMs: null,
    error: `No live or historical cell data currently available for ${normalized}.`,
  });
  return {
    battery: normalized,
    cells: [],
    timestamp: nowIso,
    isLive: false,
    isMock: false,
    source: 'none',
    sourceMeta,
  };
}

async function getCellsAt(isoTimestamp, battery = getDefaultBattery()) {
  const normalized = normalizeBattery(battery);
  const sourceConfig = getBatterySourceConfig(normalized);

  if (sourceConfig.type === 'mock') {
    const sourceMeta = buildSourceContract(normalized, {
      phase: 'historical',
      source: 'mock',
      isLive: false,
      isMock: true,
      freshAt: isoTimestamp,
      freshnessMs: null,
    });
    return {
      battery: normalized,
      cells: buildMockCells(isoTimestamp),
      timestamp: isoTimestamp,
      isLive: false,
      isMock: true,
      source: 'mock',
      sourceMeta,
    };
  }

  if (normalized !== 'tesla') {
    return getLatestCells(normalized);
  }

  const t   = new Date(isoTimestamp);
  const lo  = new Date(t.getTime() - 45_000).toISOString();
  const hi  = new Date(t.getTime() + 45_000).toISOString();

  try {
    const cells = await queryCellsFlux(
      `from(bucket: "${config.influxdb.bucket}")
         |> range(start: ${lo}, stop: ${hi})
         |> filter(fn: (r) => r._measurement == "bms_cell" and r._field == "voltage_mv")
         |> last()`
    );
    if (cells.length > 0) {
      const sourceMeta = buildSourceContract(normalized, {
        phase: 'historical',
        source: 'influxdb',
        isLive: false,
        isMock: false,
        freshAt: isoTimestamp,
        freshnessMs: null,
      });
      return {
        battery: normalized,
        cells,
        timestamp: isoTimestamp,
        isLive: false,
        isMock: false,
        source: 'influxdb',
        sourceMeta,
      };
    }
  } catch (e) {
    console.warn('[influxdb] getCellsAt failed:', e.message);
  }

  return getLatestCells(normalized);
}

async function getDataTimestamps(hours = 24) {
  try {
    const rows = await influxQuery(
      `from(bucket: "${config.influxdb.bucket}")
         |> range(start: -${hours}h)
         |> filter(fn: (r) => r._measurement == "bms_cell" and r._field == "voltage_mv" and r.cell == "1")
         |> keep(columns: ["_time"])
         |> sort(columns: ["_time"])`
    );
    if (rows.length > 0) return rows.map((row) => new Date(row._time).toISOString());
  } catch (e) {
    console.warn('[influxdb] getDataTimestamps failed:', e.message);
  }

  const now   = Date.now();
  const ticks = [];
  const step  = 5 * 60 * 1000;
  for (let t = now - hours * 3600 * 1000; t <= now; t += step) ticks.push(new Date(t).toISOString());
  return ticks;
}

async function queryCellsFlux(flux) {
  const rows  = await influxQuery(flux);
  const byCell = {};
  for (const row of rows) {
    const cn = parseInt(row.cell, 10);
    if (!Number.isNaN(cn)) byCell[cn] = Math.round(Number(row._value));
  }
  return Object.entries(byCell)
    .map(([cn, mv]) => ({ cell: parseInt(cn, 10), voltage_mv: mv, balancing: false }))
    .sort((a, b) => a.cell - b.cell);
}

async function getBatterySourceContract(battery = getDefaultBattery()) {
  const latest = await getLatestCells(battery);
  return latest.sourceMeta;
}

async function getBatterySources() {
  const batteries = Array.from(VALID_BATTERIES);
  const contracts = await Promise.all(batteries.map((battery) => getBatterySourceContract(battery)));
  return contracts.reduce((acc, contract) => {
    acc[contract.battery] = contract;
    return acc;
  }, {});
}

async function debugSchema() {
  return {
    mode: 'influxdb+mqtt',
    cellCount: CELL_COUNT,
    defaultBattery: getDefaultBattery(),
    mqttCells: {
      tesla: Object.keys(cache.getCells('tesla')).length,
      ruxiu: Object.keys(cache.getCells('ruxiu')).length,
    },
    batterySources: await getBatterySources(),
    influxUrl: config.influxdb.url,
    influxOrg: config.influxdb.org,
    influxBucket: config.influxdb.bucket,
    hasToken: !!config.influxdb.token,
  };
}

module.exports = {
  getLatestCells,
  getCellsAt,
  getDataTimestamps,
  getBatterySourceContract,
  getBatterySources,
  getDefaultBattery,
  debugSchema,
};

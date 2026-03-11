// ─────────────────────────────────────────────────────────
//  js/dashboard.js  –  Battery-aware dashboard tabs
// ─────────────────────────────────────────────────────────

(async function Dashboard() {

  const tsDisplay  = document.getElementById('timestamp-display');
  const errorBadge = document.getElementById('error-badge');

  const BATTERIES = ['tesla', 'ruxiu'];
  const STATUS_LABELS = {
    live: 'LIVE',
    mock: 'MOCK',
    'mock-history': 'MOCK HISTORY',
    stale: 'STALE',
    error: 'ERROR',
    disabled: 'DISABLED',
    unavailable: 'UNAVAILABLE',
    fallback: 'FALLBACK',
    historical: 'HISTORY',
  };

  let appConfig = { liveRefreshSeconds: 5, defaultBattery: 'tesla' };
  let sensors   = [];
  let batterySources = {};
  const charts  = {};

  const RUXIU_CHART_IDS = [
    'soc', 'battery_power', 'voltage', 'battery_temp', 'total_battery_current',
    'solar_power', 'load_power', 'grid_power', 'grid_ct_power', 'inverter_power', 'inverter_current',
    'day_battery_charge', 'day_battery_discharge', 'day_load_energy', 'day_pv_energy',
    'total_battery_charge', 'total_battery_discharge', 'total_load_energy', 'total_pv_energy',
    'total_grid_export', 'total_grid_import',
  ];
  const TESLA_CHART_IDS = [
    'tesla_soc', 'tesla_voltage', 'tesla_power', 'tesla_battery_temp_min', 'tesla_battery_temp_max',
    'tesla_pv', 'tesla_grid', 'tesla_load', 'tesla_inv_temp', 'tesla_today_pv', 'tesla_grid_import',
  ];
  const BATTERY_VIEWS = {
    tesla: { chartIds: TESLA_CHART_IDS, canvasId: 'mini-bar-canvas-tesla' },
    ruxiu: { chartIds: RUXIU_CHART_IDS, canvasId: 'mini-bar-canvas-ruxiu' },
  };

  try {
    appConfig = { ...appConfig, ...await API.getServerConfig() };
    appConfig.defaultBattery = appConfig.defaultBattery || 'tesla';
  } catch {}

  try {
    sensors = await API.getSensors();
  } catch (e) {
    console.error('Cannot load sensors:', e);
    return;
  }

  buildChartCards(TESLA_CHART_IDS, document.getElementById('chart-grid-tesla'));
  buildChartCards(RUXIU_CHART_IDS, document.getElementById('chart-grid-ruxiu'));

  requestAnimationFrame(() => {
    [...RUXIU_CHART_IDS, ...TESLA_CHART_IDS].forEach((id) => {
      const canvas = document.getElementById('canvas-' + id);
      if (canvas && !charts[id]) charts[id] = new LineChart(canvas, { mini: true });
    });
  });

  await refresh();
  setInterval(refresh, appConfig.liveRefreshSeconds * 1000);

  function buildChartCards(ids, container) {
    if (!container) return;
    for (const id of ids) {
      const s = sensors.find((x) => x.id === id);
      if (!s) continue;
      const card = mk('div', 'chart-card');
      card.style.setProperty('--sensor-color', s.color);
      card.innerHTML = `
        <div class="card-header">
          <span class="card-title" style="color:${s.color}">${s.label}</span>
          <span class="card-current-val" id="card-val-${s.id}">—</span>
        </div>
        <div class="card-canvas-wrap" id="wrap-${s.id}">
          <canvas id="canvas-${s.id}"></canvas>
        </div>
        <div class="card-footer">
          <span class="card-unit">${s.unit}</span>
          <span class="card-range">LIVE</span>
        </div>
      `;
      card.addEventListener('click', () => window.location.href = '/detail.html?sensor=' + s.id);
      container.appendChild(card);
    }
  }

  async function refresh() {
    let sensorOk = false;
    let sourceOk = false;

    const latest = await API.getAllLatest().then((data) => {
      sensorOk = true;
      return data;
    }).catch(() => ({}));
    updateValues(latest);

    const now = Object.values(latest).find((v) => v?.time)?.time;
    if (now) tsDisplay.textContent = fmtTime(now);

    const sourcePayload = await API.getBatterySources().then((payload) => {
      sourceOk = true;
      return payload;
    }).catch(() => null);

    if (sourcePayload?.sources) {
      batterySources = sourcePayload.sources;
      appConfig.defaultBattery = sourcePayload.defaultBattery || appConfig.defaultBattery;
      applyBatterySourceUI();
    }

    const activeTab = document.querySelector('.battery-tab.active')?.dataset?.tab || appConfig.defaultBattery || 'tesla';
    const sparkIds  = activeTab === 'ruxiu' ? RUXIU_CHART_IDS : TESLA_CHART_IDS;
    for (const id of sparkIds) {
      const s = sensors.find((x) => x.id === id);
      if (!s) continue;
      API.getSensorHistory(id, 24, 150)
        .then((res) => renderSparkline(s, res.data))
        .catch(() => {});
    }

    const cellResults = await Promise.allSettled(BATTERIES.map(async (battery) => {
      const payload = await API.getLatestCells(battery);
      const source = payload.sourceMeta || batterySources[battery] || null;
      if (payload.cells?.length) renderMiniBar(BATTERY_VIEWS[battery].canvasId, payload.cells);
      else clearMiniBar(BATTERY_VIEWS[battery].canvasId);
      updateBatteryCard(battery, source, payload.cells?.length || 0, null);
      return payload;
    }));

    cellResults.forEach((result, index) => {
      if (result.status === 'fulfilled') return;
      const battery = BATTERIES[index];
      clearMiniBar(BATTERY_VIEWS[battery].canvasId);
      updateBatteryCard(battery, batterySources[battery] || null, 0, result.reason);
    });

    if (sensorOk || sourceOk || cellResults.some((r) => r.status === 'fulfilled')) errorBadge.classList.add('hidden');
    else errorBadge.classList.remove('hidden');
  }

  function applyBatterySourceUI() {
    for (const battery of BATTERIES) {
      const source = batterySources[battery] || null;
      document.getElementById('battery-label-' + battery).textContent = source?.displayName || prettyBatteryName(battery);
      document.getElementById('sysinfo-' + battery).textContent = buildSourceSummary(source);
      document.getElementById('history-note-' + battery).textContent = source?.history?.note || 'History contract unavailable.';
      setStatusBadge(document.getElementById('tab-badge-' + battery), source, 'source-state-badge');
      setStatusBadge(document.getElementById('source-badge-' + battery), source, 'source-state-badge');
    }
  }

  function updateBatteryCard(battery, source, cellCount, err) {
    const messageEl = document.getElementById('source-message-' + battery);
    if (!messageEl) return;
    const message = buildSourceMessage(source, cellCount, err);
    const stateClass = 'is-' + normalizeStatus(source?.status || (err ? 'error' : 'unavailable'));
    messageEl.className = `source-message ${stateClass}${message ? '' : ' hidden'}`;
    messageEl.textContent = message;
    if (!message) messageEl.classList.add('hidden');
  }

  function buildSourceSummary(source) {
    if (!source) return 'Source contract unavailable.';
    const parts = [prettyStatus(source.status)];
    if (source.type === 'mqtt') parts.push('MQTT-backed');
    else if (source.type === 'mock') parts.push('Explicit mock source');
    else if (source.type === 'direct_http') parts.push('Direct HTTP configured');
    else if (source.type) parts.push(source.type);
    if (source.endpoint?.kind === 'mqtt' && source.endpoint.specTopic) parts.push(source.endpoint.specTopic);
    if (source.freshnessMs != null && !source.isMock) parts.push(formatFreshness(source.freshnessMs));
    if (source.lastError) parts.push(source.lastError);
    return parts.join(' · ');
  }

  function buildSourceMessage(source, cellCount, err) {
    if (err) return `Battery tab refresh failed: ${err.message || err}`;
    if (!source) return 'Battery source state is unavailable.';

    if (source.status === 'live') {
      return cellCount > 0 ? '' : 'Live source selected, but no cells are currently available for this battery.';
    }
    if (source.status === 'mock' || source.status === 'mock-history') {
      return `${source.displayName || prettyBatteryName(source.battery)} is using explicit mock cells on :3008. Do not treat this tab as live persisted history.`;
    }
    if (source.status === 'stale') {
      return source.lastError || `Live source is stale (${formatFreshness(source.freshnessMs)}).`;
    }
    if (source.status === 'fallback' || source.status === 'historical') {
      return source.history?.note || 'Showing fallback or historical data only.';
    }
    if (source.status === 'disabled') {
      return source.lastError || 'This battery source is disabled.';
    }
    if (source.status === 'error' || source.status === 'unavailable') {
      return source.lastError || `No ${source.displayName || prettyBatteryName(source.battery)} cells are currently available.`;
    }
    return cellCount > 0 ? '' : 'No cells are currently available for this battery tab.';
  }

  function renderSparkline(sensor, data) {
    if (!data?.length) return;
    const canvas = document.getElementById('canvas-' + sensor.id);
    if (!canvas) return;
    if (!charts[sensor.id]) charts[sensor.id] = new LineChart(canvas, { mini: true });
    charts[sensor.id].setData([{ label: sensor.label, color: sensor.color, unit: sensor.unit, data }]);
  }

  function renderMiniBar(canvasId, cells) {
    if (!cells?.length) return;
    const canvas = document.getElementById(canvasId);
    if (!canvas) return;
    const wrap = canvas.parentElement;
    const dpr  = window.devicePixelRatio || 1;
    const w = wrap.clientWidth, h = wrap.clientHeight;
    if (w < 2 || h < 2) return;
    canvas.width = w * dpr;
    canvas.height = h * dpr;
    canvas.style.width = w + 'px';
    canvas.style.height = h + 'px';
    const ctx = canvas.getContext('2d');
    ctx.scale(dpr, dpr);
    ctx.clearRect(0, 0, w, h);
    const sorted   = cells.slice().sort((a, b) => a.cell - b.cell);
    const voltages = sorted.map((c) => c.voltage_mv);
    const minV = Math.min(...voltages);
    const maxV = Math.max(...voltages);
    const range = maxV - minV || 1;
    const { highWarn = 3580, highCritical = 3625, lowWarn = 3000, lowCritical = 2900 } = (Config.lfp || {});
    const barW = w / sorted.length;
    const GAP = Math.max(0.5, barW * 0.1);
    sorted.forEach((c, i) => {
      const mv = c.voltage_mv;
      const ratio = (mv - minV) / range;
      const barH = 2 + ratio * (h - 4);
      const x = i * barW + GAP / 2;
      const bw = barW - GAP;
      let color;
      if (mv === maxV || mv === minV) color = 'rgba(200,220,255,0.8)';
      else if (mv >= highCritical || mv <= lowCritical) color = '#ff3b5c';
      else if (mv >= highWarn || mv <= lowWarn) color = '#f59e0b';
      else {
        const t = ratio;
        color = `rgb(${20 + Math.round(t * 10)},${65 + Math.round(t * 45)},${110 + Math.round(t * 60)})`;
      }
      ctx.fillStyle = color;
      ctx.beginPath();
      if (ctx.roundRect) ctx.roundRect(x, h - barH, bw, barH, [1, 1, 0, 0]);
      else ctx.rect(x, h - barH, bw, barH);
      ctx.fill();
    });
  }

  function clearMiniBar(canvasId) {
    const canvas = document.getElementById(canvasId);
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, canvas.width || 0, canvas.height || 0);
  }

  function updateValues(latest) {
    for (const s of sensors) {
      const entry = latest[s.id];
      const val   = entry?.value ?? null;
      const el    = document.getElementById('card-val-' + s.id);
      if (!el) continue;
      el.textContent = val != null ? fmtVal(val, s) : '—';
    }
  }

  new ResizeObserver(() => {
    const active = document.querySelector('.battery-tab.active')?.dataset?.tab || appConfig.defaultBattery || 'tesla';
    const view = BATTERY_VIEWS[active] || BATTERY_VIEWS.tesla;
    API.getLatestCells(active)
      .then((r) => r.cells?.length ? renderMiniBar(view.canvasId, r.cells) : clearMiniBar(view.canvasId))
      .catch(() => clearMiniBar(view.canvasId));
  }).observe(document.body);

  function setStatusBadge(el, source, baseClass = 'source-state-badge') {
    if (!el) return;
    const status = normalizeStatus(source?.status || 'unavailable');
    el.className = `${baseClass} is-${status}`;
    el.textContent = prettyStatus(status);
  }

  function prettyStatus(status) {
    return STATUS_LABELS[normalizeStatus(status)] || String(status || 'UNKNOWN').toUpperCase();
  }

  function normalizeStatus(status) {
    return String(status || 'unavailable').toLowerCase().replace(/[^a-z-]+/g, '-') || 'unavailable';
  }

  function prettyBatteryName(battery) {
    return battery === 'ruxiu' ? 'Ruxiu Batteries' : 'Tesla Battery';
  }

  function formatFreshness(ms) {
    if (ms == null) return 'freshness unknown';
    if (ms < 1000) return 'fresh now';
    if (ms < 60000) return `${Math.round(ms / 1000)}s ago`;
    return `${Math.round(ms / 60000)}m ago`;
  }

  function mk(tag, cls) {
    const e = document.createElement(tag);
    if (cls) e.className = cls;
    return e;
  }

  function fmtVal(v, s) {
    return Number(v).toFixed(s.decimals ?? 1) + (s.unit ? '\u00A0' + s.unit : '');
  }

  function fmtTime(iso) {
    return new Date(iso).toLocaleTimeString('en-CA', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
  }

})();

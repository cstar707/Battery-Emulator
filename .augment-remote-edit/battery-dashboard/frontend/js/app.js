// ─────────────────────────────────────────────────────────
//  js/app.js  –  Battery-aware cell monitor logic
// ─────────────────────────────────────────────────────────

(async function App() {

  const timestampDisplay = document.getElementById('timestamp-display');
  const errorBadge       = document.getElementById('error-badge');
  const mockBadge        = document.getElementById('mock-badge');
  const liveIndicator    = document.getElementById('live-indicator');
  const liveLabel        = liveIndicator.querySelector('.live-label');
  const batteryTitle     = document.getElementById('battery-title');
  const batteryMeta      = document.getElementById('battery-meta');
  const sourceBadge      = document.getElementById('source-badge');
  const sourceMessage    = document.getElementById('source-message');
  const historyNote      = document.getElementById('history-note');
  const oldestLabel      = document.getElementById('slider-label-oldest');
  const centerLabel      = document.getElementById('slider-label-center');

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

  let liveInterval = null;
  let batterySource = null;
  let batterySources = {};
  const selectedBattery = (() => {
    const battery = new URLSearchParams(window.location.search).get('battery');
    return battery === 'ruxiu' ? 'ruxiu' : 'tesla';
  })();

  try {
    const serverConfig = await API.getServerConfig();
    Config.cellCount          = serverConfig.cellCount;
    Config.liveRefreshSeconds = serverConfig.liveRefreshSeconds;
    Config.historyHours       = serverConfig.historyHours;
  } catch (err) {
    console.warn('[App] Could not load server config:', err.message);
  }

  try {
    const sourcePayload = await API.getBatterySources();
    batterySources = sourcePayload?.sources || {};
    batterySource = batterySources[selectedBattery] || null;
    updateSourceChrome(batterySource, 'live');
  } catch (err) {
    console.warn('[App] Could not load battery source contract:', err.message);
  }

  function buildFullCellList(incomingCells) {
    const incoming = new Map((incomingCells || []).map((c) => [c.cell, c.voltage_mv]));
    const lastKnown = CellGrid.getLastKnown();
    const full = [];

    for (let i = 1; i <= Config.cellCount; i++) {
      if (incoming.has(i)) full.push({ cell: i, voltage_mv: incoming.get(i), stale: false });
      else if (lastKnown.has(i)) full.push({ cell: i, voltage_mv: lastKnown.get(i), stale: true });
    }
    return full;
  }

  function renderData(payload, mode) {
    const { cells, timestamp, isMock, sourceMeta } = payload;
    if (sourceMeta?.battery) batterySources[sourceMeta.battery] = sourceMeta;
    batterySource = sourceMeta || batterySource;
    updateSourceChrome(batterySource, mode);
    CellGrid.render(cells);
    BarChart.render(buildFullCellList(cells));
    timestampDisplay.textContent = formatTime(timestamp);
    mockBadge.classList.toggle('hidden', !isMock);
    errorBadge.classList.add('hidden');
  }

  function updateSourceChrome(source, mode) {
    const effective = source || { battery: selectedBattery, displayName: prettyBatteryName(selectedBattery), status: 'unavailable' };
    const status = normalizeStatus(effective.status);
    document.title = `${effective.displayName || prettyBatteryName(selectedBattery)} – Cell Monitor`;
    batteryTitle.textContent = effective.displayName || prettyBatteryName(selectedBattery);
    batteryMeta.textContent = buildSourceSummary(effective);
    setStatusBadge(sourceBadge, effective);
    const teslaSource = selectedBattery === 'tesla' ? effective : batterySources.tesla;
    const ruxiuSource = selectedBattery === 'ruxiu' ? effective : batterySources.ruxiu;
    setStatusBadge(document.getElementById('tab-badge-tesla'), teslaSource, 'source-state-badge');
    setStatusBadge(document.getElementById('tab-badge-ruxiu'), ruxiuSource, 'source-state-badge');
    document.getElementById('battery-tab-tesla').classList.toggle('is-active', selectedBattery === 'tesla');
    document.getElementById('battery-tab-ruxiu').classList.toggle('is-active', selectedBattery === 'ruxiu');
    sourceMessage.textContent = buildSourceMessage(effective, mode);
    sourceMessage.className = `source-message is-${status}${sourceMessage.textContent ? '' : ' hidden'}`;
    historyNote.textContent = buildHistoryNote(effective, mode);
    oldestLabel.textContent = selectedBattery === 'ruxiu' ? 'mock timeline' : `–${Config.historyHours}h`;
    centerLabel.textContent = selectedBattery === 'ruxiu' ? '◀ synthetic preview' : '◀ scroll back in time';
    liveLabel.textContent = TimeSlider.isLive ? 'LIVE' : 'PAUSED';
    mockBadge.textContent = selectedBattery === 'ruxiu' ? 'MOCK SOURCE' : 'MOCK DATA';
    mockBadge.classList.toggle('hidden', !(effective.isMock || status === 'mock' || status === 'mock-history'));
  }

  function buildSourceSummary(source) {
    const parts = [prettyStatus(source.status)];
    if (source.type === 'mqtt') parts.push('MQTT-backed');
    else if (source.type === 'mock') parts.push('Explicit mock source');
    else if (source.type === 'direct_http') parts.push('Direct HTTP configured');
    if (source.endpoint?.kind === 'mqtt' && source.endpoint.specTopic) parts.push(source.endpoint.specTopic);
    if (source.freshnessMs != null && !source.isMock) parts.push(formatFreshness(source.freshnessMs));
    if (source.lastError) parts.push(source.lastError);
    return parts.join(' · ');
  }

  function buildSourceMessage(source, mode) {
    if (source.status === 'live') return '';
    if (source.status === 'mock' || source.status === 'mock-history') {
      return `${source.displayName || prettyBatteryName(selectedBattery)} is explicit mock data on :3008. ${mode === 'historical' ? 'This historical view is synthetic, not persisted.' : 'Historical scrubbing does not represent persisted live history.'}`;
    }
    if (source.status === 'stale') return source.lastError || `Source is stale (${formatFreshness(source.freshnessMs)}).`;
    if (source.status === 'fallback' || source.status === 'historical') return source.history?.note || 'Showing fallback or historical data only.';
    if (source.status === 'disabled') return source.lastError || 'This battery source is disabled.';
    if (source.status === 'error' || source.status === 'unavailable') return source.lastError || 'Battery source is currently unavailable.';
    return '';
  }

  function buildHistoryNote(source, mode) {
    if (!source?.history) return 'History contract unavailable.';
    if (mode === 'historical' && source.history.backedByStoredData) {
      return `Showing recorded history. ${source.history.note}`;
    }
    if (mode === 'historical' && !source.history.backedByStoredData) {
      return `Showing synthetic mock cells at the selected timestamp only. ${source.history.note}`;
    }
    return source.history.note;
  }

  function showError(err) {
    console.error('[App] Data fetch error:', err.message);
    errorBadge.textContent = `⚠ ${err.message || 'SOURCE ISSUE'}`;
    errorBadge.classList.remove('hidden');
    sourceMessage.textContent = `Battery tab refresh failed: ${err.message || err}`;
    sourceMessage.className = 'source-message is-error';
  }

  function formatTime(iso) {
    if (!iso) return '—';
    return new Date(iso).toLocaleTimeString('da-DK', {
      hour: '2-digit', minute: '2-digit', second: '2-digit',
    });
  }

  async function fetchLive() {
    try {
      renderData(await API.getLatestCells(selectedBattery), 'live');
    } catch (err) {
      showError(err);
    }
  }

  function startLive() {
    stopLive();
    fetchLive();
    liveInterval = setInterval(fetchLive, Config.liveRefreshSeconds * 1000);
    liveIndicator.classList.remove('paused');
    liveLabel.textContent = 'LIVE';
  }

  function stopLive() {
    if (liveInterval) {
      clearInterval(liveInterval);
      liveInterval = null;
    }
    liveLabel.textContent = 'PAUSED';
  }

  async function fetchHistorical(isoTimestamp) {
    try {
      renderData(await API.getCellsAt(isoTimestamp, selectedBattery), 'historical');
    } catch (err) {
      showError(err);
    }
  }

  TimeSlider.init({
    onLive()         { startLive(); },
    onHistorical(ts) { stopLive(); fetchHistorical(ts); },
  });

  startLive();

  document.addEventListener('visibilitychange', () => {
    if (document.hidden) stopLive();
    else if (TimeSlider.isLive) startLive();
  });

  function setStatusBadge(el, source, baseClass = 'source-state-badge') {
    if (!el) return;
    const status = normalizeStatus(source?.status || 'unavailable');
    el.className = `${baseClass} is-${status}`;
    el.textContent = prettyStatus(status);
  }

  function prettyBatteryName(battery) {
    return battery === 'ruxiu' ? 'Ruxiu Batteries' : 'Tesla Battery';
  }

  function prettyStatus(status) {
    return STATUS_LABELS[normalizeStatus(status)] || String(status || 'UNKNOWN').toUpperCase();
  }

  function normalizeStatus(status) {
    return String(status || 'unavailable').toLowerCase().replace(/[^a-z-]+/g, '-') || 'unavailable';
  }

  function formatFreshness(ms) {
    if (ms == null) return 'freshness unknown';
    if (ms < 1000) return 'fresh now';
    if (ms < 60000) return `${Math.round(ms / 1000)}s ago`;
    return `${Math.round(ms / 60000)}m ago`;
  }

})();
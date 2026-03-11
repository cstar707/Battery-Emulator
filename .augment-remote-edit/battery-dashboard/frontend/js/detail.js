// ─────────────────────────────────────────────────────────
//  js/detail.js  –  Full-screen sensor chart with overlays
// ─────────────────────────────────────────────────────────

(async function Detail() {

  const titleEl    = document.getElementById('detail-title');
  const unitEl     = document.getElementById('detail-unit');
  const chipsEl    = document.getElementById('overlay-chips');
  const legendEl   = document.getElementById('detail-legend');
  const tooltip    = document.getElementById('detail-tooltip');
  const tooltipT   = document.getElementById('tooltip-time');
  const tooltipV   = document.getElementById('tooltip-values');
  const errorBadge = document.getElementById('error-badge');

  // ── State ───────────────────────────────────────────────
  const params     = new URLSearchParams(window.location.search);
  const primaryId  = params.get('sensor');
  let   hours      = 6;
  let   sensors    = [];
  let   overlayIds = new Set();
  let   chart      = null;
  let   seriesData = {};   // sensorId → [{time,value}]

  if (!primaryId) { window.location.href = '/'; return; }

  // ── Init ────────────────────────────────────────────────
  try {
    sensors = await API.getSensors();
  } catch (e) {
    errorBadge.classList.remove('hidden');
    return;
  }

  const primary = sensors.find(s => s.id === primaryId);
  if (!primary) { window.location.href = '/'; return; }

  titleEl.textContent = primary.label;
  unitEl.textContent  = primary.unit;
  document.title      = `${primary.label} – Battery Dashboard`;

  buildOverlayChips();
  initChart();
  bindTimeRangeButtons();
  await loadAndRender();

  // ── Chart ───────────────────────────────────────────────

  function initChart() {
    const canvas = document.getElementById('detail-canvas');
    chart = new LineChart(canvas, {
      padLeft:   60,
      padRight:  20,
      padTop:    20,
      padBottom: 30,
      showGrid:  true,
      showYAxis: true,
      showXAxis: true,
      showHover: true,
      onHover: (values) => {
        if (!values) { tooltip.style.opacity = '0'; return; }
        tooltip.style.opacity = '1';
        tooltipT.textContent  = new Date(values[0].time).toLocaleString('da-DK');
        tooltipV.innerHTML    = values.map(v =>
          `<span style="color:${v.color}">▪ ${v.label}: <b>${formatVal(v.value, findSensor(v.label))}</b></span>`
        ).join('');
      },
    });
  }

  // ── Load data ───────────────────────────────────────────

  async function loadAndRender() {
    // Load primary + overlays in parallel
    const toLoad = [primaryId, ...overlayIds];
    await Promise.all(toLoad.map(async (id) => {
      try {
        const res       = await API.getSensorHistory(id, hours, 600);
        seriesData[id]  = res.data;
      } catch { seriesData[id] = []; }
    }));

    renderChart();
    renderLegend();
  }

  function renderChart() {
    const series = [primaryId, ...overlayIds].map(id => {
      const s = sensors.find(x => x.id === id);
      return { label: s.label, color: s.color, unit: s.unit, data: seriesData[id] || [] };
    });
    chart.setData(series);
  }

  // ── Overlay chips ───────────────────────────────────────

  function buildOverlayChips() {
    chipsEl.innerHTML = '';
    for (const s of sensors) {
      if (s.id === primaryId) continue;
      const chip = document.createElement('button');
      chip.className    = 'overlay-chip';
      chip.dataset.id   = s.id;
      chip.style.setProperty('--chip-color', s.color);
      chip.textContent  = s.label;
      chip.addEventListener('click', () => toggleOverlay(s.id, chip));
      chipsEl.appendChild(chip);
    }
  }

  async function toggleOverlay(id, chipEl) {
    if (overlayIds.has(id)) {
      overlayIds.delete(id);
      chipEl.classList.remove('active');
    } else {
      overlayIds.add(id);
      chipEl.classList.add('active');
      if (!seriesData[id]) {
        const res     = await API.getSensorHistory(id, hours, 600);
        seriesData[id] = res.data;
      }
    }
    renderChart();
    renderLegend();
  }

  // ── Legend ──────────────────────────────────────────────

  function renderLegend() {
    const ids = [primaryId, ...overlayIds];
    legendEl.innerHTML = ids.map(id => {
      const s = sensors.find(x => x.id === id);
      return `<div class="legend-item">
        <span class="legend-dot" style="background:${s.color}"></span>
        <span class="legend-label">${s.label}</span>
        <span class="legend-unit" style="color:${s.color}">${s.unit}</span>
      </div>`;
    }).join('');
  }

  // ── Time range buttons ──────────────────────────────────

  function bindTimeRangeButtons() {
    document.getElementById('time-range-btns').addEventListener('click', async (e) => {
      const btn = e.target.closest('.range-btn');
      if (!btn) return;
      document.querySelectorAll('.range-btn').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      hours      = parseInt(btn.dataset.hours);
      seriesData = {};  // clear cache – different range
      await loadAndRender();
    });
  }

  // ── Helpers ─────────────────────────────────────────────

  function findSensor(label) {
    return sensors.find(s => s.label === label) || {};
  }

  function formatVal(v, sensor) {
    if (v == null) return '—';
    if (sensor?.isBinary) return v ? 'CLOSED' : 'OPEN';
    const d = sensor?.decimals ?? 1;
    return v.toFixed(d) + (sensor?.unit ? ' ' + sensor.unit : '');
  }

})();
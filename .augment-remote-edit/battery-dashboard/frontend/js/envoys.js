// ─────────────────────────────────────────────────────────
//  envoys.js  –  Enphase Micro-Inverter Dashboard
//  Data sources:
//    live watts  → /api/envoy/data   (Envoy cache + InfluxDB)
//    daily kWh   → /api/envoy/data   (InfluxDB integral since today midnight)
//    history     → /api/envoy/history (InfluxDB time-series)
// ─────────────────────────────────────────────────────────

// ── Inverter grouping ─────────────────────────────────────
const HOUSE_SERIALS = new Set(['121138031474','121138032402','121138031483']);
const SHED_FROM_ENVOY2 = new Set(['542442025779']);

// All Envoy-2 serials except the one moved to Shed
const TRAILER_SERIALS = new Set([
  '202219071122','202219064522','202219067773','202219067168',
  '202219030726','202219066411','542442025750','542441003578',
  '542442025808','542442025914','542442025916','542442025408',
  '542442025748',
]);

// ── Group colours ─────────────────────────────────────────
const GROUP_COLOR = { house:'#34d399', shed:'#60a5fa', trailer:'#f87171' };

// ── State ─────────────────────────────────────────────────
const sectionCharts = {};   // group -> LineChart instance
const sparkCharts   = {};   // serial -> LineChart instance
const sparkHistory  = {};   // serial -> [{x,y}]
const sectionHistory = {};  // group  -> [{x,y}]
let lastInverters   = [];   // previous render list for sparkline updates

// ── Helpers ───────────────────────────────────────────────
function fmtW(w)   { return w == null ? '--' : Math.round(w).toLocaleString(); }
function fmtKwh(k) { return k == null ? '--' : (Math.round(k * 10) / 10).toFixed(1); }
function now()     { return Date.now(); }

function classifySerial(serial, envoyId) {
  if (HOUSE_SERIALS.has(serial))    return 'house';
  if (SHED_FROM_ENVOY2.has(serial)) return 'shed';
  if (envoyId === 'envoy1')         return 'shed';
  return 'trailer';
}

function groupInverters(inverters) {
  const groups = { house:[], shed:[], trailer:[] };
  for (const inv of inverters) {
    const g = classifySerial(inv.serial, inv.envoy_id);
    groups[g].push(inv);
  }
  return groups;
}

// ── KPI update ────────────────────────────────────────────
function updateKPIs(groups) {
  const sum = (arr, f) => arr.reduce((s, x) => s + (x[f] || 0), 0);

  const allInv = [...groups.house, ...groups.shed, ...groups.trailer];
  document.getElementById('sum-live').textContent  = fmtW(sum(allInv,'watts'));
  document.getElementById('sum-daily').textContent = fmtKwh(sum(allInv,'daily_kwh'));

  for (const [g, arr] of Object.entries(groups)) {
    const liveW  = sum(arr,'watts');
    const daily  = sum(arr,'daily_kwh');
    const yday   = sum(arr,'yesterday_kwh');
    const active = arr.filter(i => (i.watts || 0) > 0).length;

    document.getElementById(`${g}-daily`).textContent     = fmtKwh(daily);
    document.getElementById(`${g}-live`).textContent      = fmtW(liveW) + ' W';
    document.getElementById(`${g}-daily-sub`).textContent = fmtKwh(daily) + ' kWh';
    document.getElementById(`${g}-active`).textContent    = `${active}/${arr.length}`;
    const ydayEl = document.getElementById(`${g}-yday`);
    if (ydayEl) ydayEl.textContent = fmtKwh(yday);
  }
}

function updateYesterdayKPIs(apiData) {
  const s = (id, v) => { const el = document.getElementById(id); if (el) el.textContent = v; };
  s('sum-yday', fmtKwh(apiData?.total_yesterday_kwh   ?? null));
  s('e1-yday',  fmtKwh(apiData?.envoy1_yesterday_kwh  ?? null));
}

// ── Section (group) charts ────────────────────────────────
function initSectionCharts() {
  for (const [g, color] of Object.entries(GROUP_COLOR)) {
    const canvas = document.getElementById(`chart-${g}`);
    if (!canvas || sectionCharts[g]) continue;
    sectionCharts[g] = new LineChart(canvas, {
      color,
      fill:       true,
      yMin:       0,
      labelFormat: v => `${Math.round(v)} W`,
      compact:    true,
    });
    sectionHistory[g] = [];
  }
}

function updateSectionChartPoint(group, watts) {
  const t = now();
  const h = sectionHistory[group] || (sectionHistory[group] = []);
  h.push({ x: t, y: watts });
  // Keep only last 6 hours
  const cutoff = t - 6 * 3600 * 1000;
  while (h.length > 1 && h[0].x < cutoff) h.shift();
  if (sectionCharts[group]) sectionCharts[group].setData(h);
}

// ── Inverter sparkline helpers ─────────────────────────────
function updateSparkline(serial, watts, color) {
  const t = now();
  const h = sparkHistory[serial] || (sparkHistory[serial] = []);
  h.push({ x: t, y: watts });
  const cutoff = t - 3600 * 1000; // keep 1 hour
  while (h.length > 1 && h[0].x < cutoff) h.shift();
  if (sparkCharts[serial]) sparkCharts[serial].setData(h);
}

// ── Render inverter card ───────────────────────────────────
function renderCard(inv, idx, group) {
  const serial = inv.serial;
  const watts  = inv.watts || 0;
  const color  = GROUP_COLOR[group];
  const online = watts > 0;
  const sn4    = serial.slice(-8);
  const label  = `INV ${String(idx + 1).padStart(2, '0')}`;

  const div = document.createElement('div');
  div.className = `inv-card${online ? '' : ' offline'}`;
  div.style.setProperty('--group-color', color);
  div.dataset.serial = serial;

  div.innerHTML = `
    <div class="inv-top">
      <div class="inv-label-wrap">
        <span class="inv-num">${label}</span>
        <span class="inv-sn">${sn4}</span>
      </div>
      <div class="inv-status-dot"></div>
    </div>
    <div class="inv-watts-row">
      <span class="inv-watts">${fmtW(watts)}</span>
      <span class="inv-watts-u">W</span>
    </div>
    <div class="inv-meta">
      <span class="inv-daily" style="color:${color}">${fmtKwh(inv.daily_kwh)} kWh</span>
      <span class="inv-yday">yday ${fmtKwh(inv.yesterday_kwh)} kWh</span>
    </div>
    <div class="inv-spark"><canvas id="spark-${serial}"></canvas></div>
  `;
  return div;
}

// ── Render all groups ──────────────────────────────────────
function renderGroups(groups) {
  for (const [g, arr] of Object.entries(groups)) {
    const container = document.getElementById(`grid-${g}`);
    if (!container) continue;

    // Check if we need a full re-render (different number of cards)
    const existing = container.querySelectorAll('.inv-card');
    if (existing.length !== arr.length) {
      container.innerHTML = '';
      if (!arr.length) {
        container.innerHTML = '<div class="no-data-notice">No inverter data</div>';
        continue;
      }
      arr.forEach((inv, i) => {
        const card = renderCard(inv, i, g);
        container.appendChild(card);
        // Init sparkline
        const canvas = card.querySelector(`#spark-${inv.serial}`);
        if (canvas && !sparkCharts[inv.serial]) {
          sparkCharts[inv.serial] = new LineChart(canvas, {
            color:      GROUP_COLOR[g],
            fill:       true,
            yMin:       0,
            compact:    true,
            showAxes:   false,
          });
        }
      });
    } else {
      // Update existing cards in place
      arr.forEach((inv, i) => {
        const card = existing[i];
        if (!card) return;
        const watts = inv.watts || 0;
        card.className = `inv-card${watts > 0 ? '' : ' offline'}`;
        const wEl = card.querySelector('.inv-watts');
        if (wEl) wEl.textContent = fmtW(watts);
        const dEl = card.querySelector('.inv-daily');
        if (dEl) dEl.textContent = fmtKwh(inv.daily_kwh) + ' kWh';
        const yEl = card.querySelector('.inv-yday');
        if (yEl) yEl.textContent = 'yday ' + fmtKwh(inv.yesterday_kwh) + ' kWh';
      });
    }

    // Update sparklines + section chart
    let groupW = 0;
    arr.forEach(inv => {
      const w = inv.watts || 0;
      groupW += w;
      updateSparkline(inv.serial, w, GROUP_COLOR[g]);
    });
    updateSectionChartPoint(g, groupW);
  }
}

// ── Load InfluxDB history for section charts ───────────────
async function loadSectionHistory(groups) {
  for (const [g, arr] of Object.entries(groups)) {
    if (!arr.length) continue;
    const serials = arr.map(i => i.serial);
    try {
      const resp = await API.getEnvoyHistory(serials, 12, 200);
      if (resp.ok && resp.data.length) {
        const pts = resp.data.map(d => ({ x: new Date(d.time).getTime(), y: d.value || 0 }));
        sectionHistory[g] = pts;
        if (sectionCharts[g]) sectionCharts[g].setData(pts);
      }
    } catch (e) {
      console.warn(`[envoys] history fetch for ${g}:`, e.message);
    }
  }
}

// ── Main data fetch + render loop ─────────────────────────
let firstRender = true;

async function refresh() {
  try {
    const data = await API.getEnvoyData();
    if (!data.ok || !data.inverters) return;

    const groups = groupInverters(data.inverters);
    updateKPIs(groups);
    updateYesterdayKPIs(data);
    renderGroups(groups);

    if (firstRender) {
      firstRender = false;
      // Load InfluxDB history for the section charts (once on startup)
      loadSectionHistory(groups);
    }

    const ts = document.getElementById('timestamp-display');
    if (ts) ts.textContent = new Date().toLocaleTimeString();
  } catch (err) {
    console.error('[envoys] refresh error:', err);
  }
}

// ── Init ──────────────────────────────────────────────────
document.addEventListener('DOMContentLoaded', async () => {
  initSectionCharts();
  await refresh();
  setInterval(refresh, 10000);   // refresh live data every 10 s
  setInterval(() => {
    // Reload section history every 5 min
    API.getEnvoyData().then(d => {
      if (d.ok) loadSectionHistory(groupInverters(d.inverters));
    }).catch(() => {});
  }, 300000);
});

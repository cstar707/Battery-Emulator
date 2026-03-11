// ─────────────────────────────────────────────────────────
//  js/grid-status.js  –  Grid Status page
//  Cards: Solark Battery %, Tesla-Y Battery %, Total Day PV Energy
//  Values are buffered to prevent jumping/zero flicker on transient API gaps
// ─────────────────────────────────────────────────────────

(async function GridStatus() {

  const tsDisplay = document.getElementById('timestamp-display');
  let appConfig = { liveRefreshSeconds: 5 };

  // Buffer: hold last valid value to avoid flicker when API returns null/0 briefly
  const BUFFER_MS = 60000;  // keep last value for 60s on gap
  const ZERO_HOLD_CYCLES = 2;  // require 2 consecutive zeros before showing 0 (avoids glitches)
  const buffer = {};  // key -> { value, ts, zeroCount }
  let zeroHoldCounters = {};
  // Monotonic: cumulative daily values (Solis, Solark, Envoy) should never decrease
  const maxSeen = {};  // key -> max value seen today
  let lastDate = new Date().toDateString();  // key -> count of consecutive zeros seen

  try { appConfig = { ...appConfig, ...await API.getServerConfig() }; } catch {}

  await refresh();
  setInterval(refresh, appConfig.liveRefreshSeconds * 1000);

  async function refresh() {
    try {
      const [latest, envoyData, envoyInflux] = await Promise.all([
        API.getAllLatest(),
        API.getGridEnvoy().catch(() => null),
        API.getEnvoyData().catch(() => null),
      ]);
      updateCards(latest, envoyData, envoyInflux);

      const now = Object.values(latest).find(v => v?.time)?.time;
      if (now) tsDisplay.textContent = fmtTime(now);
    } catch (e) {
      console.error('Grid status refresh error:', e);
    }
  }

  function buffered(key, raw, isZeroSensitive = true, isCumulative = false) {
    const valid = raw != null && typeof raw === 'number' && !isNaN(raw);
    const now = Date.now();
    const today = new Date().toDateString();
    if (today !== lastDate) { lastDate = today; Object.keys(maxSeen).forEach(k => delete maxSeen[k]); }

    if (valid) {
      if (raw === 0 && isZeroSensitive) {
        zeroHoldCounters[key] = (zeroHoldCounters[key] || 0) + 1;
        if (zeroHoldCounters[key] < ZERO_HOLD_CYCLES) {
          const prev = buffer[key];
          if (prev) return prev.value;
          return 0;
        }
      } else {
        zeroHoldCounters[key] = 0;
      }
      let out = raw;
      if (isCumulative && raw >= 0) {
        const m = maxSeen[key];
        if (m != null && raw < m - 0.05) out = m;  // reject decreases (allow 0.05 tolerance)
        else if (m == null || raw > m) maxSeen[key] = raw;
      }
      buffer[key] = { value: out, ts: now };
      return out;
    }

    zeroHoldCounters[key] = 0;
    const prev = buffer[key];
    if (prev && (now - prev.ts) < BUFFER_MS) return prev.value;
    return null;
  }

  function updateCards(latest, envoyData, envoyInflux) {
    const solarkRaw = latest.soc?.value;
    const teslaRaw = latest.tesla_soc?.value;
    const solisPv = buffered('solisPv', latest.tesla_today_pv?.value, true, true);
    const solarkPvVal = latest.day_pv_energy?.value;
    const solarkPv = buffered('solarkPv', solarkPvVal, true, true);
    const tubuchiPv = 15;

    // Envoy daily kWh — prefer InfluxDB totals (accurate since midnight, survives restarts)
    // Fall back to live gateway data if InfluxDB unavailable
    let envoy1Raw = null, envoy2Raw = null;
    let envoy1Yday = null, envoy2Yday = null;
    if (envoyInflux?.ok) {
      envoy1Raw  = envoyInflux.envoy1_daily_kwh     ?? null;
      envoy2Raw  = envoyInflux.envoy2_daily_kwh     ?? null;
      envoy1Yday = envoyInflux.envoy1_yesterday_kwh ?? null;
      envoy2Yday = envoyInflux.envoy2_yesterday_kwh ?? null;
    } else if (envoyData?.ok) {
      envoy1Raw = envoyData.envoy1?.energy_today_kwh ?? null;
      envoy2Raw = envoyData.envoy2?.energy_today_kwh ?? null;
    }
    const envoy1Pv = buffered('envoy1Pv', envoy1Raw, true, true);
    const envoy2Pv = buffered('envoy2Pv', envoy2Raw, true, true);

    const solarkSoc = buffered('solarkSoc', solarkRaw);
    const teslaSoc = buffered('teslaSoc', teslaRaw);

    setEl('solark-soc', solarkSoc != null ? solarkSoc.toFixed(1) : '—');
    setEl('tesla-soc', teslaSoc != null ? teslaSoc.toFixed(1) : '—');
    setEl('pv-solis', fmtKwh(solisPv));
    setEl('pv-solark', fmtKwh(solarkPv));
    setEl('pv-tubuchi', fmtKwh(tubuchiPv));
    setEl('pv-envoy1',      fmtKwh(envoy1Pv));
    setEl('pv-envoy1-yday', fmtKwh(envoy1Yday));
    setEl('pv-envoy2',      fmtKwh(envoy2Pv));
    setEl('pv-envoy2-yday', fmtKwh(envoy2Yday));

    let total = 0;
    if (typeof solisPv   === 'number') total += solisPv;
    if (typeof solarkPv  === 'number') total += solarkPv;
    if (typeof tubuchiPv === 'number') total += tubuchiPv;
    if (typeof envoy1Pv  === 'number') total += envoy1Pv;
    if (typeof envoy2Pv  === 'number') total += envoy2Pv;

    setEl('total-day-pv', total > 0 ? fmtKwh(total) : '—');
  }

  function setEl(id, text) {
    const el = document.getElementById(id);
    if (el) el.textContent = text;
  }

  function fmtKwh(v) {
    if (v == null || typeof v !== 'number' || isNaN(v)) return '—';
    return v.toFixed(1) + ' kWh';
  }

  function fmtTime(iso) {
    return new Date(iso).toLocaleTimeString('en-CA', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
  }

})();

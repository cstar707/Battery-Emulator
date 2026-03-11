// ─────────────────────────────────────────────────────────
//  js/api.js  –  All communication with the backend
// ─────────────────────────────────────────────────────────

const API = (() => {

  async function get(path) {
    const res  = await fetch(path);
    const data = await res.json();
    if (!data.ok) throw new Error(data.error || 'Server error');
    return data;
  }

  function withBattery(path, battery) {
    if (!battery) return path;
    const joiner = path.includes('?') ? '&' : '?';
    return `${path}${joiner}battery=${encodeURIComponent(battery)}`;
  }

  return {
    // ── App config ──────────────────────────────────────
    async getServerConfig() {
      return get('/api/config');
    },
    async getBatterySources() {
      return get('/api/battery-sources');
    },

    // ── Cell monitor ────────────────────────────────────
    async getLatestCells(battery) {
      return get(withBattery('/api/cells/latest', battery));
    },
    async getCellsAt(isoTimestamp, battery) {
      return get(withBattery(`/api/cells/at?t=${encodeURIComponent(isoTimestamp)}`, battery));
    },
    async getDataTimestamps(hours) {
      const res = await get(`/api/ticks?hours=${hours}`);
      return res.timestamps;
    },

    // ── Sensor dashboard ────────────────────────────────
    async getSensors() {
      const res = await get('/api/sensors');
      return res.sensors;
    },
    async getAllLatest() {
      const res = await get('/api/sensors/latest');
      return res.values;
    },
    async getSensorHistory(id, hours = 24, points = 500) {
      return get(`/api/sensors/${encodeURIComponent(id)}/history?hours=${hours}&points=${points}`);
    },

    async getGridEnvoy() {
      return get('/api/grid/envoy');
    },
    async getGridEnvoyInverters() {
      return get('/api/grid/envoy-inverters');
    },
    async getEnvoyData() {
      return get('/api/envoy/data');
    },
    async getEnvoyHistory(serials, hours = 12, points = 200) {
      return get(`/api/envoy/history?serials=${serials.join(',')}&hours=${hours}&points=${points}`);
    },
  };

})();

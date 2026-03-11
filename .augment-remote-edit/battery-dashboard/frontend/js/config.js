// ─────────────────────────────────────────────────────────
//  js/config.js  –  Frontend tuneable settings
// ─────────────────────────────────────────────────────────

const Config = {
  // Overwritten at startup by /api/config
  cellCount:          108,
  liveRefreshSeconds: 5,
  historyHours:       24,

  // ── LFP voltage thresholds (mV) ───────────────────────
  lfp: {
    // High side  (max cell voltage is 3650 mV)
    highWarn:     3580,   // → subtle amber glow starts here
    highCritical: 3625,   // → bright red
    highMax:      3650,   // absolute max (used for interpolation)

    // Low side   (min cell voltage is ~2800 mV)
    lowWarn:      3000,   // → subtle amber glow starts here
    lowCritical:  2900,   // → bright red
    lowMin:       2800,   // absolute min (used for interpolation)
  },

  // ── Bar chart horizontal reference lines ──────────────
  // Only drawn when the voltage is within the visible range.
  // Edit freely – add/remove lines here, nowhere else.
  chartReferenceLines: [
    { mv: 3650, label: '3.65V', color: 'rgba(255, 59,  92,  0.8)' },
    { mv: 3580, label: '3.58V', color: 'rgba(245, 158, 11,  0.6)' },
    { mv: 3000, label: '3.00V', color: 'rgba(245, 158, 11,  0.6)' },
    { mv: 2900, label: '2.90V', color: 'rgba(255, 59,  92,  0.8)' },
  ],

  // Slider resolution
  sliderSteps: 2000,
};
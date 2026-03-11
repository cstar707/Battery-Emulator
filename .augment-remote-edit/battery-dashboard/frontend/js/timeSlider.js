// ─────────────────────────────────────────────────────────
//  js/timeSlider.js  –  Time scrubbing slider
//
//  Fires data updates LIVE while dragging (no debounce).
//  Rightmost position = LIVE mode with auto-refresh.
// ─────────────────────────────────────────────────────────

const TimeSlider = (() => {

  const slider        = document.getElementById('time-slider');
  const timeDisplay   = document.getElementById('slider-time-display');
  const liveIndicator = document.getElementById('live-indicator');
  const oldestLabel   = document.getElementById('slider-label-oldest');

  let onLive       = null;
  let onHistorical = null;

  // ── Init ──────────────────────────────────────────────

  function init({ onLive: liveCallback, onHistorical: histCallback }) {
    onLive       = liveCallback;
    onHistorical = histCallback;

    slider.max   = Config.sliderSteps;
    slider.value = Config.sliderSteps;

    oldestLabel.textContent = `–${Config.historyHours}h`;

    // Fire on every input event – no debounce – for live scrubbing feel
    slider.addEventListener('input', onSliderMove);

    updateFillStyle();
    updateDisplay(true);
  }

  // ── Position → timestamp ──────────────────────────────

  function sliderToTimestamp(value) {
    if (value >= Config.sliderSteps) return null;
    const historyMs = Config.historyHours * 3600 * 1000;
    const fraction  = value / Config.sliderSteps;
    const offsetMs  = (1 - fraction) * historyMs;
    return new Date(Date.now() - offsetMs).toISOString();
  }

  function isLive() {
    return parseInt(slider.value) >= Config.sliderSteps;
  }

  // ── Display ───────────────────────────────────────────

  function updateFillStyle() {
    const pct = (slider.value / slider.max * 100).toFixed(1) + '%';
    slider.style.setProperty('--fill-pct', pct);
  }

  function updateDisplay(live) {
    if (live) {
      timeDisplay.textContent = '● LIVE';
      timeDisplay.classList.add('is-live');
      liveIndicator.classList.remove('paused');
    } else {
      const ts = sliderToTimestamp(parseInt(slider.value));
      timeDisplay.textContent = formatTimestamp(ts);
      timeDisplay.classList.remove('is-live');
      liveIndicator.classList.add('paused');
    }
  }

  function formatTimestamp(iso) {
    if (!iso) return '';
    return new Date(iso).toLocaleString('da-DK', {
      day: '2-digit', month: '2-digit',
      hour: '2-digit', minute: '2-digit', second: '2-digit',
    });
  }

  // ── Event handler ─────────────────────────────────────

  function onSliderMove() {
    updateFillStyle();
    const live = isLive();
    updateDisplay(live);

    // Fire immediately – no debounce
    if (live) {
      onLive && onLive();
    } else {
      const ts = sliderToTimestamp(parseInt(slider.value));
      onHistorical && onHistorical(ts);
    }
  }

  // ── Public API ────────────────────────────────────────

  return {
    init,
    onNewLiveTick() {},
    setLive() {
      slider.value = Config.sliderSteps;
      updateFillStyle();
      updateDisplay(true);
    },
    get isLive() { return isLive(); },
  };

})();
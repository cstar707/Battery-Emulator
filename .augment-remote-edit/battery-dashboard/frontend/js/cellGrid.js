// ─────────────────────────────────────────────────────────
//  js/cellGrid.js  –  Renders the 108-cell voltage grid
//
//  Color logic:
//   - LFP warn (highWarn / lowWarn)     → amber glow
//   - LFP critical (highCrit / lowCrit) → red glow
//   - Highest cell in current dataset   → subtle white/bright border
//   - Lowest cell in current dataset    → subtle white/bright border
//   - Normal                            → default dark card
//
//  Note: highest/lowest get a neutral highlight, NOT the LFP
//  warning colour, unless they also exceed a threshold.
// ─────────────────────────────────────────────────────────

const CellGrid = (() => {

  const grid    = document.getElementById('cell-grid');
  const statMax = document.getElementById('stat-max');
  const statMin = document.getElementById('stat-min');
  const statDev = document.getElementById('stat-dev');
  const statAvg = document.getElementById('stat-avg');

  const cellElements  = new Map();  // cellNum → card DOM element
  const lastKnown     = new Map();  // cellNum → last known voltage_mv (fill-in for missing data)
  let isInitialised   = false;

  // ── LFP colour calculation ────────────────────────────

  /**
   * Returns { bg, border, text } CSS strings based on LFP proximity,
   * or null if voltage is within normal range.
   */
  function lfpColor(mv) {
    const { highWarn, highCritical, highMax, lowWarn, lowCritical, lowMin } = Config.lfp;

    if (mv >= highCritical) {
      const t = Math.min((mv - highCritical) / (highMax - highCritical), 1);
      const g = Math.round(60 * (1 - t));
      return {
        bg:     `rgba(255,${g},20,0.15)`,
        border: `rgba(255,${g},20,0.85)`,
        text:   `rgb(255,${g + 40},40)`,
      };
    }
    if (mv >= highWarn) {
      const t = (mv - highWarn) / (highCritical - highWarn);
      const g = Math.round(158 - t * 60);
      return {
        bg:     `rgba(245,${g},11,0.10)`,
        border: `rgba(245,${g},11,0.65)`,
        text:   `rgb(255,${g + 20},40)`,
      };
    }
    if (mv <= lowCritical) {
      const t = Math.min((lowCritical - mv) / (lowCritical - lowMin), 1);
      const g = Math.round(59 * (1 - t));
      return {
        bg:     `rgba(255,${g},80,0.15)`,
        border: `rgba(255,${g},80,0.85)`,
        text:   `rgb(255,${g + 30},80)`,
      };
    }
    if (mv <= lowWarn) {
      const t = (lowWarn - mv) / (lowWarn - lowCritical);
      const g = Math.round(158 - t * 60);
      return {
        bg:     `rgba(245,${g},11,0.10)`,
        border: `rgba(245,${g},11,0.65)`,
        text:   `rgb(255,${g + 20},40)`,
      };
    }

    return null; // normal range – no special colour
  }

  // ── Build grid ────────────────────────────────────────

  function createCellElement(num) {
    const card = document.createElement('div');
    card.className    = 'cell-card';
    card.dataset.cell = num;

    const label = document.createElement('span');
    label.className   = 'cell-number';
    label.textContent = `C${num}`;

    const voltage = document.createElement('span');
    voltage.className   = 'cell-voltage';
    voltage.textContent = '—';

    card.appendChild(label);
    card.appendChild(voltage);

    card.addEventListener('mouseenter', () => Highlight.setCell(num));
    card.addEventListener('mouseleave', () => Highlight.clear());

    return card;
  }

  function buildGrid(cellCount) {
    grid.innerHTML = '';
    for (let i = 1; i <= cellCount; i++) {
      const card = createCellElement(i);
      cellElements.set(i, card);
      grid.appendChild(card);
    }
    isInitialised = true;

    Highlight.register('grid', (cellNum) => {
      document.querySelectorAll('.cell-card.is-hovered')
        .forEach(el => el.classList.remove('is-hovered'));
      if (cellNum !== null) {
        const card = cellElements.get(cellNum);
        if (card) {
          card.classList.add('is-hovered');
          card.scrollIntoView({ block: 'nearest', behavior: 'smooth' });
        }
      }
    });
  }

  // ── Apply appearance to a single card ─────────────────

  function styleCard(card, mv, isHighest, isLowest) {
    const voltageEl = card.querySelector('.cell-voltage');
    voltageEl.textContent = `${mv}`;

    // Reset everything
    card.style.background  = '';
    card.style.borderColor = '';
    card.style.boxShadow   = '';
    voltageEl.style.color  = '';
    card.classList.remove('is-min', 'is-max', 'is-stale');

    // 1. First apply LFP threshold colours (takes priority)
    const lfp = lfpColor(mv);
    if (lfp) {
      card.style.background  = lfp.bg;
      card.style.borderColor = lfp.border;
      card.style.boxShadow   = `0 0 8px ${lfp.border.replace('0.85', '0.25')}`;
      voltageEl.style.color  = lfp.text;
      // Also add min/max marker for LFP-highlighted extremes
      if (isHighest) card.classList.add('is-max');
      if (isLowest)  card.classList.add('is-min');
      return;
    }

    // 2. Highest/lowest in dataset – neutral highlight only
    if (isHighest) {
      card.classList.add('is-max');   // uses CSS: bright border, no colour fill
    } else if (isLowest) {
      card.classList.add('is-min');   // uses CSS: bright border, no colour fill
    }
  }

  // ── Stats header ──────────────────────────────────────

  function updateStats(minV, maxV, avgV) {
    const dev = maxV - minV;
    statMax.textContent = `${maxV} mV`;
    statMin.textContent = `${minV} mV`;
    statDev.textContent = `${dev} mV`;
    statAvg.textContent = `${Math.round(avgV)} mV`;

    statDev.classList.remove('warn', 'alert');
    if      (dev >= 100) statDev.classList.add('alert');
    else if (dev >= 30)  statDev.classList.add('warn');
  }

  // ── Public API ────────────────────────────────────────

  return {
    /**
     * Render cell grid.
     * Missing cells (not present in `cells` array) are filled
     * from the lastKnown map so all 108 cards always show a value.
     */
    render(cells) {
      if (!isInitialised) buildGrid(Config.cellCount);

      // Build a lookup from the incoming data
      const incoming = new Map();
      for (const c of (cells || [])) {
        incoming.set(c.cell, c.voltage_mv);
        lastKnown.set(c.cell, c.voltage_mv);   // update last-known cache
      }

      // Build a complete dataset: incoming value or last-known fallback
      const full = [];
      for (let i = 1; i <= Config.cellCount; i++) {
        const mv = incoming.has(i) ? incoming.get(i) : lastKnown.get(i);
        if (mv !== undefined) full.push({ cell: i, voltage_mv: mv });
      }

      if (full.length === 0) return;

      const voltages = full.map(c => c.voltage_mv);
      const minV     = Math.min(...voltages);
      const maxV     = Math.max(...voltages);
      const avgV     = voltages.reduce((a, b) => a + b, 0) / voltages.length;

      updateStats(minV, maxV, avgV);

      for (const c of full) {
        const card = cellElements.get(c.cell);
        if (!card) continue;

        const isStale = !incoming.has(c.cell);
        styleCard(card, c.voltage_mv, c.voltage_mv === maxV, c.voltage_mv === minV);

        // Dim stale cells slightly so the user can see they are filled-in
        if (isStale) card.classList.add('is-stale');
      }
    },

    /** Used by barChart to get ordered voltages including stale fill-ins */
    getLastKnown() {
      return lastKnown;
    },
  };

})();
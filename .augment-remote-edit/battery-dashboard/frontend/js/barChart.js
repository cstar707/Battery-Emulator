// ─────────────────────────────────────────────────────────
//  js/barChart.js  –  Canvas bar chart for cell voltages
//
//  Features:
//   - Y-axis with voltage labels (always visible)
//   - Horizontal reference lines at LFP thresholds
//   - LFP-based bar colours (normal → amber → red)
//   - Cross-highlight with cell grid
//   - Stale data bars shown slightly dimmed
// ─────────────────────────────────────────────────────────

const BarChart = (() => {

  const canvas    = document.getElementById('bar-chart');
  const ctx       = canvas.getContext('2d');
  const hoverInfo = document.getElementById('chart-hover-info');
  const hoverLbl  = document.getElementById('chart-hover-label');
  const hoverVal  = document.getElementById('chart-hover-value');

  let lastCells       = [];   // { cell, voltage_mv, stale? }
  let highlightedCell = null;

  // Layout constants
  const Y_AXIS_W   = 46;   // px reserved for Y-axis labels on the left
  const PAD_TOP    = 20;   // px above highest bar (room for top ref-line label)
  const PAD_BOTTOM = 4;    // px below lowest bar

  // ── Resize ────────────────────────────────────────────

  function resizeCanvas() {
    const rect = canvas.parentElement.getBoundingClientRect();
    const dpr  = window.devicePixelRatio || 1;
    canvas.width        = rect.width  * dpr;
    canvas.height       = rect.height * dpr;
    canvas.style.width  = rect.width  + 'px';
    canvas.style.height = rect.height + 'px';
    ctx.scale(dpr, dpr);
    if (lastCells.length) draw();
  }

  new ResizeObserver(() => resizeCanvas()).observe(canvas.parentElement);

  // ── LFP bar colour ─────────────────────────────────────

  function barColor(mv, isHighest, isLowest, isHovered) {
    if (isHovered)  return '#ffffff';

    const { highWarn, highCritical, highMax, lowWarn, lowCritical, lowMin } = Config.lfp;

    if (mv >= highCritical) {
      const t = Math.min((mv - highCritical) / (highMax - highCritical), 1);
      return `rgb(255,${Math.round(60*(1-t))},20)`;
    }
    if (mv >= highWarn) {
      const t = (mv - highWarn) / (highCritical - highWarn);
      return `rgb(245,${Math.round(158-t*60)},11)`;
    }
    if (mv <= lowCritical) {
      const t = Math.min((lowCritical - mv) / (lowCritical - lowMin), 1);
      return `rgb(255,${Math.round(59*(1-t))},80)`;
    }
    if (mv <= lowWarn) {
      const t = (lowWarn - mv) / (lowWarn - lowCritical);
      return `rgb(245,${Math.round(158-t*60)},11)`;
    }

    // Normal range – neutral blue gradient by relative height
    if (isHighest) return 'rgba(180,220,255,0.85)';
    if (isLowest)  return 'rgba(180,220,255,0.85)';

    const voltages = lastCells.map(c => c.voltage_mv);
    const minV     = Math.min(...voltages);
    const maxV     = Math.max(...voltages);
    const ratio    = (mv - minV) / ((maxV - minV) || 1);
    return `rgb(${Math.round(20+ratio*10)},${Math.round(65+ratio*45)},${Math.round(110+ratio*60)})`;
  }

  // ── Y-axis ────────────────────────────────────────────

  function drawYAxis(minV, maxV, w, h) {
    const usableH = h - PAD_TOP - PAD_BOTTOM;

    // Choose a nice step: aim for ~5-8 labels
    const rangeV  = maxV - minV;
    const rawStep = rangeV / 6;
    // Round to nearest 10mV
    const step    = Math.max(10, Math.round(rawStep / 10) * 10);

    // First label at next multiple of step above minV
    const firstV  = Math.ceil(minV / step) * step;

    ctx.save();
    ctx.font      = `400 10px 'JetBrains Mono', monospace`;
    ctx.textAlign = 'right';
    ctx.fillStyle = 'rgba(100,130,160,0.9)';

    for (let v = firstV; v <= maxV; v += step) {
      const ratio = (v - minV) / (rangeV || 1);
      const y     = h - PAD_BOTTOM - ratio * usableH;

      // Tick line
      ctx.strokeStyle = 'rgba(60,90,120,0.35)';
      ctx.lineWidth   = 1;
      ctx.setLineDash([2, 3]);
      ctx.beginPath();
      ctx.moveTo(Y_AXIS_W, y);
      ctx.lineTo(w, y);
      ctx.stroke();

      // Label  (e.g. "3372")
      ctx.setLineDash([]);
      ctx.fillText(`${v}`, Y_AXIS_W - 4, y + 3.5);
    }

    ctx.restore();
  }

  // ── Reference lines ────────────────────────────────────

  function drawReferenceLines(minV, maxV, w, h) {
    const usableH = h - PAD_TOP - PAD_BOTTOM;
    const rangeV  = maxV - minV || 1;

    ctx.save();
    ctx.setLineDash([5, 4]);
    ctx.lineWidth = 1.5;

    for (const line of Config.chartReferenceLines) {
      if (line.mv < minV || line.mv > maxV) continue;
      const ratio = (line.mv - minV) / rangeV;
      const y     = h - PAD_BOTTOM - ratio * usableH;

      ctx.strokeStyle = line.color;
      ctx.beginPath();
      ctx.moveTo(Y_AXIS_W, y);
      ctx.lineTo(w, y);
      ctx.stroke();

      // Label on the right edge
      ctx.setLineDash([]);
      ctx.font      = `500 9px 'JetBrains Mono', monospace`;
      ctx.textAlign = 'right';
      ctx.fillStyle = line.color;
      ctx.fillText(line.label, w - 2, y - 3);
    }

    ctx.restore();
  }

  // ── Main draw ──────────────────────────────────────────

  function draw() {
    if (!lastCells.length) return;

    const sorted   = lastCells.slice().sort((a, b) => a.cell - b.cell);
    const w        = canvas.offsetWidth;
    const h        = canvas.offsetHeight;
    const voltages = sorted.map(c => c.voltage_mv);
    const minV     = Math.min(...voltages);
    const maxV     = Math.max(...voltages);
    const rangeV   = maxV - minV || 1;
    const usableH  = h - PAD_TOP - PAD_BOTTOM;

    // Bar area starts after Y-axis
    const barAreaW = w - Y_AXIS_W;
    const barW     = barAreaW / sorted.length;
    const GAP      = Math.max(0.5, barW * 0.12);

    ctx.clearRect(0, 0, w, h);

    // Y-axis grid lines + labels
    drawYAxis(minV, maxV, w, h);

    // Threshold reference lines (on top of grid, below bars)
    drawReferenceLines(minV, maxV, w, h);

    // Bars
    for (let i = 0; i < sorted.length; i++) {
      const cell      = sorted[i];
      const ratio     = (cell.voltage_mv - minV) / rangeV;
      const barH      = PAD_BOTTOM + ratio * usableH;
      const x         = Y_AXIS_W + i * barW + GAP / 2;
      const y         = h - barH;
      const bw        = barW - GAP;
      const isHovered = cell.cell === highlightedCell;
      const isHighest = cell.voltage_mv === maxV;
      const isLowest  = cell.voltage_mv === minV;

      ctx.globalAlpha = cell.stale ? 0.45 : 1;
      ctx.fillStyle   = barColor(cell.voltage_mv, isHighest, isLowest, isHovered);
      ctx.shadowBlur  = isHovered ? 10 : 0;
      ctx.shadowColor = 'rgba(255,255,255,0.7)';

      ctx.beginPath();
      if (ctx.roundRect) {
        ctx.roundRect(x, y, bw, barH, [2, 2, 0, 0]);
      } else {
        ctx.rect(x, y, bw, barH);
      }
      ctx.fill();
    }

    ctx.globalAlpha = 1;
    ctx.shadowBlur  = 0;
  }

  // ── Hover ──────────────────────────────────────────────

  function cellAtX(clientX) {
    const rect    = canvas.getBoundingClientRect();
    const relX    = clientX - rect.left - Y_AXIS_W;
    if (relX < 0) return null;
    const barAreaW = rect.width - Y_AXIS_W;
    const barW     = barAreaW / lastCells.length;
    const idx      = Math.floor(relX / barW);
    const sorted   = lastCells.slice().sort((a, b) => a.cell - b.cell);
    return sorted[Math.min(idx, sorted.length - 1)] || null;
  }

  canvas.addEventListener('mousemove', (e) => {
    if (!lastCells.length) return;
    const cell = cellAtX(e.clientX);
    if (cell) {
      hoverLbl.textContent = `Cell ${cell.cell}`;
      hoverVal.textContent = `${cell.voltage_mv} mV${cell.stale ? ' ↩' : ''}`;
      hoverInfo.classList.add('visible');
      Highlight.setCell(cell.cell);
    }
  });

  canvas.addEventListener('mouseleave', () => {
    hoverInfo.classList.remove('visible');
    Highlight.clear();
  });

  Highlight.register('chart', (cellNum) => {
    highlightedCell = cellNum;
    draw();
    if (cellNum !== null) {
      const cell = lastCells.find(c => c.cell === cellNum);
      if (cell) {
        hoverLbl.textContent = `Cell ${cell.cell}`;
        hoverVal.textContent = `${cell.voltage_mv} mV${cell.stale ? ' ↩' : ''}`;
        hoverInfo.classList.add('visible');
      }
    } else {
      hoverInfo.classList.remove('visible');
    }
  });

  // ── Public API ─────────────────────────────────────────

  return {
    /**
     * Render bars.
     * Accepts cells with optional { stale: true } for filled-in values.
     */
    render(cells) {
      lastCells = cells;
      draw();
    },
  };

})();
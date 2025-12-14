(() => {
  const weightEl = document.getElementById('weight');
  const statusEl = document.getElementById('status');
  const tareBtn = document.getElementById('tareBtn');
  const clearBtn = document.getElementById('clearBtn');
  const graphCanvas = document.getElementById('weightGraph');
  const gctx = graphCanvas.getContext('2d');

  // data buffer: array of {t: timestamp_ms, v: value}
  const data = [];
  let WINDOW_MS = 5 * 60 * 1000; // default 5 minutes (can be changed by selectors)
  const selectorWrap = document.getElementById('windowSelectors');

  function setStatus(s) {
    statusEl.textContent = s;
    setTimeout(() => { if (statusEl.textContent === s) statusEl.textContent = ''; }, 1500);
  }

  // open websocket to /ws
  let ws;
  function connect() {
    const loc = window.location;
    const protocol = (loc.protocol === 'https:') ? 'wss://' : 'ws://';
    const url = protocol + loc.host + '/ws';
    ws = new WebSocket(url);

    ws.onopen = () => {
      setStatus('WS connected');
    };
    ws.onclose = () => {
      setStatus('WS disconnected — retrying');
      setTimeout(connect, 1500);
    };
    ws.onmessage = (ev) => {
      try {
        const obj = JSON.parse(ev.data);
        const now = Date.now();
        let w = obj.weight;
        if (w === null || w === undefined || isNaN(w)) {
          weightEl.textContent = '-- g';
          w = NaN;
        } else {
          weightEl.textContent = w.toFixed(2) + ' g';
        }
        // push into buffer with timestamp
        data.push({ t: now, v: (isNaN(w) ? NaN : Number(w)) });
        // trim old data
        const cutoff = now - WINDOW_MS;
        while (data.length && data[0].t < cutoff) data.shift();
        drawGraph();
      } catch (e) {
        console.error('Invalid ws message', e, ev.data);
      }
    };
  }
  connect();

  tareBtn.addEventListener('click', () => {
    // try websocket command first
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send('tare');
      setStatus('Tare sent');
      return;
    }
    // fallback to HTTP POST
    fetch('/tare', { method: 'POST' })
      .then(r => r.json())
      .then(j => setStatus('Tare OK'))
      .catch(err => setStatus('Tare failed'));
  });

  clearBtn.addEventListener('click', () => {
    data.length = 0;
    drawGraph();
    setStatus('Data cleared');
  });

  // window selector buttons
  if (selectorWrap) {
    selectorWrap.addEventListener('click', (ev) => {
      const btn = ev.target.closest && ev.target.closest('.winBtn');
      if (!btn) return;
      const ms = Number(btn.getAttribute('data-ms')) || 0;
      if (!ms) return;
      WINDOW_MS = ms;
      // toggle active class
      selectorWrap.querySelectorAll('.winBtn').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      setStatus('Window: ' + (ms >= 60000 ? (ms/60000) + 'm' : (ms/1000) + 's'));
      drawGraph();
    });
  }

  // draw graph on canvas showing last WINDOW_MS
  function drawGraph() {
    const w = graphCanvas.width = Math.floor(graphCanvas.clientWidth * devicePixelRatio);
    const h = graphCanvas.height = Math.floor(graphCanvas.clientHeight * devicePixelRatio);
    gctx.setTransform(devicePixelRatio, 0, 0, devicePixelRatio, 0, 0);
    gctx.clearRect(0, 0, graphCanvas.clientWidth, graphCanvas.clientHeight);
    // background and grid
    gctx.fillStyle = '#fff';
    gctx.fillRect(0, 0, graphCanvas.clientWidth, graphCanvas.clientHeight);
    gctx.strokeStyle = '#eee';
    gctx.lineWidth = 1;
    for (let i = 0; i <= 4; i++) {
      const y = (graphCanvas.clientHeight / 4) * i;
      gctx.beginPath(); gctx.moveTo(0, y); gctx.lineTo(graphCanvas.clientWidth, y); gctx.stroke();
    }
    if (!data.length) return;
    // compute min/max of visible data ignoring NaN
    const now = Date.now();
    const cutoff = now - WINDOW_MS;
    const visible = data.filter(d => d.t >= cutoff && !isNaN(d.v));
    if (!visible.length) return;
    let min = visible[0].v, max = visible[0].v;
    for (const p of visible) { if (p.v < min) min = p.v; if (p.v > max) max = p.v; }
    if (min === max) { min -= 0.5; max += 0.5; }
    // margins
    const pad = 6;
    const plotW = graphCanvas.clientWidth - pad * 2;
    const plotH = graphCanvas.clientHeight - pad * 2;
    // draw line
    gctx.beginPath();
    gctx.strokeStyle = '#0077cc';
    gctx.lineWidth = 2;
    let started = false;
    for (let i = 0; i < data.length; i++) {
      const p = data[i];
      if (isNaN(p.v) || p.t < cutoff) continue;
      const x = pad + ((p.t - cutoff) / WINDOW_MS) * plotW;
      const y = pad + (1 - (p.v - min) / (max - min)) * plotH;
      if (!started) { gctx.moveTo(x, y); started = true; } else { gctx.lineTo(x, y); }
    }
    gctx.stroke();
    // draw min/max labels
    gctx.fillStyle = '#333';
    gctx.font = '12px sans-serif';
    gctx.fillText(max.toFixed(2) + ' g', pad + 4, pad + 12);
    gctx.fillText(min.toFixed(2) + ' g', pad + 4, graphCanvas.clientHeight - pad - 2);
  }

  // redraw periodically to keep canvas crisp
  setInterval(drawGraph, 1000);

})();

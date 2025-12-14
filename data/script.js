(() => {
  const weightEl1 = document.getElementById('weight1');
  const weightEl2 = document.getElementById('weight2');
  const statusEl = document.getElementById('status');
  const tareBtn = document.getElementById('tareBtn');
  const clearBtn = document.getElementById('clearBtn');
  const canvas1 = document.getElementById('weightGraph1');
  const canvas2 = document.getElementById('weightGraph2');

  const ctx1 = canvas1.getContext('2d');
  const ctx2 = canvas2.getContext('2d');

  const data1 = [];
  const data2 = [];
  let WINDOW_MS = 5 * 60 * 1000;
  const selectorWrap = document.getElementById('windowSelectors');

  function setStatus(s) {
    statusEl.textContent = s;
    setTimeout(() => { if (statusEl.textContent === s) statusEl.textContent = ''; }, 1500);
  }

  // websocket
  let ws;
  function connect() {
    const loc = window.location;
    const protocol = (loc.protocol === 'https:') ? 'wss://' : 'ws://';
    const url = protocol + loc.host + '/ws';
    ws = new WebSocket(url);
    ws.onopen = () => setStatus('WS connected');
    ws.onclose = () => { setStatus('WS disconnected — retrying'); setTimeout(connect, 1500); };
    ws.onmessage = (ev) => {
      try {
        const obj = JSON.parse(ev.data);
        const now = Date.now();
        const w1 = obj.weight1;
        const w2 = obj.weight2;
        if (w1 === null || w1 === undefined || isNaN(w1)) {
          weightEl1.textContent = '-- g';
          data1.push({ t: now, v: NaN });
        } else {
          weightEl1.textContent = Number(w1).toFixed(2) + ' g';
          data1.push({ t: now, v: Number(w1) });
        }
        if (w2 === null || w2 === undefined || isNaN(w2)) {
          weightEl2.textContent = '-- g';
          data2.push({ t: now, v: NaN });
        } else {
          weightEl2.textContent = Number(w2).toFixed(2) + ' g';
          data2.push({ t: now, v: Number(w2) });
        }
        // trim
        const cutoff = now - WINDOW_MS;
        while (data1.length && data1[0].t < cutoff) data1.shift();
        while (data2.length && data2[0].t < cutoff) data2.shift();
        drawAll();
      } catch (e) { console.error('Invalid ws message', e, ev.data); }
    };
  }
  connect();

  tareBtn.addEventListener('click', () => {
    if (ws && ws.readyState === WebSocket.OPEN) { ws.send('tare'); setStatus('Tare sent'); return; }
    fetch('/tare', { method: 'POST' }).then(r => r.json()).then(j => setStatus('Tare OK')).catch(() => setStatus('Tare failed'));
  });

  clearBtn.addEventListener('click', () => { data1.length = 0; data2.length = 0; drawAll(); setStatus('Data cleared'); });

  if (selectorWrap) {
    selectorWrap.addEventListener('click', (ev) => {
      const btn = ev.target.closest && ev.target.closest('.winBtn');
      if (!btn) return;
      const ms = Number(btn.getAttribute('data-ms')) || 0;
      if (!ms) return;
      WINDOW_MS = ms;
      selectorWrap.querySelectorAll('.winBtn').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      setStatus('Window: ' + (ms >= 60000 ? (ms/60000) + 'm' : (ms/1000) + 's'));
      drawAll();
    });
  }

  function drawGraph(canvas, ctx, data, color) {
    const w = canvas.width = Math.floor(canvas.clientWidth * devicePixelRatio);
    const h = canvas.height = Math.floor(canvas.clientHeight * devicePixelRatio);
    ctx.setTransform(devicePixelRatio, 0, 0, devicePixelRatio, 0, 0);
    ctx.clearRect(0, 0, canvas.clientWidth, canvas.clientHeight);
    ctx.fillStyle = '#fff'; ctx.fillRect(0, 0, canvas.clientWidth, canvas.clientHeight);
    ctx.strokeStyle = '#eee'; ctx.lineWidth = 1;
    for (let i = 0; i <= 4; i++) { const y = (canvas.clientHeight / 4) * i; ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(canvas.clientWidth, y); ctx.stroke(); }
    if (!data.length) return;
    const now = Date.now(); const cutoff = now - WINDOW_MS;
    const visible = data.filter(d => d.t >= cutoff && !isNaN(d.v));
    if (!visible.length) return;
    let min = visible[0].v, max = visible[0].v; for (const p of visible) { if (p.v < min) min = p.v; if (p.v > max) max = p.v; }
    if (min === max) { min -= 0.5; max += 0.5; }
    const pad = 6; const plotW = canvas.clientWidth - pad * 2; const plotH = canvas.clientHeight - pad * 2;
    ctx.beginPath(); ctx.strokeStyle = color; ctx.lineWidth = 2;
    let started = false;
    for (let i = 0; i < data.length; i++) {
      const p = data[i]; if (isNaN(p.v) || p.t < cutoff) continue;
      const x = pad + ((p.t - cutoff) / WINDOW_MS) * plotW;
      const y = pad + (1 - (p.v - min) / (max - min)) * plotH;
      if (!started) { ctx.moveTo(x, y); started = true; } else { ctx.lineTo(x, y); }
    }
    ctx.stroke();
    ctx.fillStyle = '#333'; ctx.font = '12px sans-serif'; ctx.fillText(max.toFixed(2) + ' g', pad + 4, pad + 12);
    ctx.fillText(min.toFixed(2) + ' g', pad + 4, canvas.clientHeight - pad - 2);
  }

  function drawAll() { drawGraph(canvas1, ctx1, data1, '#0077cc'); drawGraph(canvas2, ctx2, data2, '#cc5500'); }

  setInterval(drawAll, 1000);

})();

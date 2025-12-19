(() => {
  const weightEl1 = document.getElementById('weight1');
  const weightEl2 = document.getElementById('weight2');
  const statusEl = document.getElementById('status');
  const tare1Btn = document.getElementById('tare1Btn');
  const tare2Btn = document.getElementById('tare2Btn');
  const calibrateBtn = document.getElementById('calibrateBtn');
  const calScale = document.getElementById('calScale');
  const calResult = document.getElementById('calResult');
  const calReading1 = document.getElementById('calReading1');
  const calReading2 = document.getElementById('calReading2');
  const tareAllBtn = document.getElementById('tareAllBtn');
  const clearBtn = document.getElementById('clearBtn');
  const canvas1 = document.getElementById('weightGraph1');
  const canvas2 = document.getElementById('weightGraph2');

  const ctx1 = canvas1.getContext('2d');
  const ctx2 = canvas2.getContext('2d');

  // Spec table elements and input (use robust selector to get the inner input)
  const specInputEl = document.querySelector('#specInput input') || document.getElementById('specInput');
  const loadSpecBtn = document.getElementById('loadSpecBtn');
  const specAvg1El = document.getElementById('specAvg1');
  const specAvg2El = document.getElementById('specAvg2');
  const specThs = document.querySelectorAll('#SpecTable th');
  const specTh1 = specThs && specThs[1];
  const specTh2 = specThs && specThs[2];

  let MIN_SPEC = (specInputEl && parseFloat(specInputEl.value)) || (specInputEl && parseFloat(specInputEl.placeholder)) || 550;
  if (loadSpecBtn && specInputEl) loadSpecBtn.addEventListener('click', () => {
    const v = parseFloat(specInputEl.value);
    if (!isNaN(v)) { MIN_SPEC = v; setStatus('Spec set: ' + MIN_SPEC + ' g'); }
    else setStatus('Invalid spec');
    updateSpecTable();
  });

  // Pressing Enter in the spec input should trigger the Set Spec button
  if (specInputEl && loadSpecBtn) {
    specInputEl.addEventListener('keydown', (ev) => {
      if (ev.key === 'Enter') {
        ev.preventDefault();
        loadSpecBtn.click();
      }
    });
  }

  const data1 = [];
  const data2 = [];
  const color1El = document.getElementById('color1');
  const color2El = document.getElementById('color2');
  const box1 = document.getElementById('scaleBox1');
  const box2 = document.getElementById('scaleBox2');
  let WINDOW_MS = 5 * 60 * 1000;
  const selectorWrap = document.getElementById('windowSelectors');

  function setStatus(s) {
    statusEl.textContent = s;
    setTimeout(() => { if (statusEl.textContent === s) statusEl.textContent = ''; }, 1500);
  }

  let ws;
  let isParentMode = false;  // Track if this is a parent node
  
  function connect() {
    const loc = window.location;
    const protocol = (loc.protocol === 'https:') ? 'wss://' : 'ws://';
    const url = protocol + loc.host + '/ws';
    ws = new WebSocket(url);
    ws.onopen = () => setStatus('WS connected');
    // fetch persisted settings once websocket opens
    fetch('/settings').then(r => r.json()).then(j => {
      if (j.name1) { title1.textContent = j.name1; name1.value = j.name1; }
      if (j.name2) { title2.textContent = j.name2; name2.value = j.name2; }
      if (j.color1) { color1El.value = j.color1; color1El.setAttribute('value', j.color1); }
      if (j.color2) { color2El.value = j.color2; color2El.setAttribute('value', j.color2); }
      applyColors();
    }).catch(()=>{});
    ws.onclose = () => { setStatus('WS disconnected — retrying'); setTimeout(connect, 1500); };
    ws.onmessage = (ev) => {
      try {
        const obj = JSON.parse(ev.data);
        // Check if parent or child mode
        if (obj.mode === 'parent') {
          isParentMode = true;
        } else if (obj.mode === 'child') {
          isParentMode = false;
        }
        // handle calibration response
        if (obj.calibrate !== undefined) {
          const which = obj.scale || 0;
          const r = obj.calibrate;
          const text = (isNaN(r) ? 'calibrate failed' : (Number(r).toFixed(3) + ' g'));
          calResult.textContent = `Scale ${which}: ${text}`;
          if (which === 1) calReading1.textContent = text;
          if (which === 2) calReading2.textContent = text;
        }
        const now = Date.now();
        const w1 = obj.weight1 || obj.weight;
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

  // Modular tare function - sends tare request for a specific node
  function sendTareCommand(nodeId) {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send('tare:' + nodeId);
      setStatus('Tare ' + nodeId + ' sent');
      return;
    }
    // Fallback to HTTP endpoint (for child nodes only)
    fetch('/tare?scale=' + nodeId, { method: 'POST' })
      .then(r => r.json())
      .then(j => setStatus('Tare OK'))
      .catch(() => setStatus('Tare failed'));
  }

  // Tare all nodes - send requests to nodes 1-10
  tareAllBtn.addEventListener('click', () => {
    if (ws && ws.readyState === WebSocket.OPEN) {
      // Parent mode: loop through all possible child nodes
      for (let i = 1; i <= 10; i++) {
        ws.send('tare:' + i);
      }
      setStatus('Tare all sent');
      return;
    }
    // Child mode or WS unavailable: use HTTP endpoint
    fetch('/tare', { method: 'POST' })
      .then(r => r.json())
      .then(j => setStatus('Tare OK'))
      .catch(() => setStatus('Tare failed'));
  });

  // per-scale tare buttons
  if (tare1Btn) tare1Btn.addEventListener('click', () => {
    sendTareCommand(1);
  });
  if (tare2Btn) tare2Btn.addEventListener('click', () => {
    sendTareCommand(2);

  // calibration
  if (calibrateBtn) calibrateBtn.addEventListener('click', () => {
    const which = Number(calScale.value) || 1;
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send('calibrate:' + which);
      setStatus('Calibrate sent');
      return;
    }
    setStatus('WS not connected');
  });

  // color handling
  function applyColors() {
    const c1 = (color1El && color1El.value) ? color1El.value : '#0077cc';
    const c2 = (color2El && color2El.value) ? color2El.value : '#cc5500';
    if (box1) box1.style.setProperty('--accent', c1);
    if (box2) box2.style.setProperty('--accent', c2);
    // update spec table header colors to match graphs
    updateSpecHeaderColors(c1, c2);
    updateSpecHeaderText();
    drawAll();
  }
  if (color1El) color1El.addEventListener('input', applyColors);
  if (color2El) color2El.addEventListener('input', applyColors);
  applyColors();

  // edit UI
  const edit1 = document.getElementById('edit1');
  const edit2 = document.getElementById('edit2');
  const name1 = document.getElementById('scaleName1');
  const name2 = document.getElementById('scaleName2');
  const title1 = document.querySelector('#scaleBox1 .scaleTitle');
  const title2 = document.querySelector('#scaleBox2 .scaleTitle');

  function beginEdit(box, which) {
    box.classList.add('editing');
    // focus name input
    if (box.id === 'scaleBox1') name1.focus(); else name2.focus();
  }
  function endEdit(box, save) {
    if (!save) {
      // revert name and color to previous values
      if (box.id === 'scaleBox1') {
        name1.value = title1.textContent;
        color1El.value = color1El.getAttribute('value') || '#0077cc';
      } else {
        name2.value = title2.textContent;
        color2El.value = color2El.getAttribute('value') || '#cc5500';
      }
      applyColors();
    } else {
      // save title
      if (box.id === 'scaleBox1') {
        title1.textContent = name1.value;
        color1El.setAttribute('value', color1El.value);
      } else {
        title2.textContent = name2.value;
        color2El.setAttribute('value', color2El.value);
      }
    }
    box.classList.remove('editing');
    // if saved, update spec table headers to reflect new names
    if (save) updateSpecHeaderText();
    // if saved, persist settings to server
    if (save) {
      const payload = {
        name1: document.getElementById('scaleName1').value,
        name2: document.getElementById('scaleName2').value,
        color1: document.getElementById('color1').value,
        color2: document.getElementById('color2').value
      };
      fetch('/settings', { method: 'POST', body: JSON.stringify(payload), headers: {'Content-Type':'application/json'} })
        .then(r => r.json()).then(j => { if (j.status === 'ok') setStatus('Settings saved'); else setStatus('Save failed'); })
        .catch(() => setStatus('Save failed'));
    }
  }

  if (edit1) edit1.addEventListener('click', () => beginEdit(box1, 1));
  if (edit2) edit2.addEventListener('click', () => beginEdit(box2, 2));

  // add save/cancel buttons dynamically inside each box header when editing
  function createEditControls(box) {
    let ctr = box.querySelector('.editControls');
    if (ctr) return ctr;
    ctr = document.createElement('div'); ctr.className = 'editControls';
    const save = document.createElement('button'); save.className = 'saveBtn';
    save.setAttribute('title','Save');
    save.innerHTML = '<svg viewBox="0 0 24 24"><path d="M9 16.2l-3.5-3.5L4 14.2 9 19l12-12-1.5-1.5z"/></svg>';
    const cancel = document.createElement('button'); cancel.className = 'cancelBtn';
    cancel.setAttribute('title','Cancel');
    cancel.innerHTML = '<svg viewBox="0 0 24 24"><path d="M19 6.4L17.6 5 12 10.6 6.4 5 5 6.4 10.6 12 5 17.6 6.4 19 12 13.4 17.6 19 19 17.6 13.4 12z"/></svg>';
    ctr.appendChild(save); ctr.appendChild(cancel);
    const header = box.querySelector('.scaleHeader'); header.appendChild(ctr);
    save.addEventListener('click', () => endEdit(box, true));
    cancel.addEventListener('click', () => endEdit(box, false));
    return ctr;
  }
  createEditControls(box1); createEditControls(box2);

  clearBtn.addEventListener('click', () => { data1.length = 0; data2.length = 0; drawAll(); setStatus('Data cleared'); });

  const resetBtn = document.getElementById('resetBtn');
  if (resetBtn) resetBtn.addEventListener('click', () => {
    if (!confirm('Reset scale names and colors to defaults?')) return;
    fetch('/settings/reset', { method: 'POST' })
      .then(r => r.json())
      .then(j => {
        if (j.name1) { title1.textContent = j.name1; name1.value = j.name1; }
        if (j.name2) { title2.textContent = j.name2; name2.value = j.name2; }
        if (j.color1) { color1El.value = j.color1; color1El.setAttribute('value', j.color1); }
        if (j.color2) { color2El.value = j.color2; color2El.setAttribute('value', j.color2); }
        applyColors();
        setStatus('Defaults restored');
      }).catch(()=> setStatus('Reset failed'));
  });

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

  function drawAll() { 
    const c1 = (color1El && color1El.value) ? color1El.value : '#0077cc';
    const c2 = (color2El && color2El.value) ? color2El.value : '#cc5500';
    drawGraph(canvas1, ctx1, data1, c1); 
    drawGraph(canvas2, ctx2, data2, c2); 
    updateSpecTable();
  }

  // helper: pick readable text color for a given background hex
  function textColorForBg(hex) {
    if (!hex) return '#000';
    const h = hex.replace('#','');
    const r = parseInt(h.substring(0,2),16);
    const g = parseInt(h.substring(2,4),16);
    const b = parseInt(h.substring(4,6),16);
    const lum = (0.299*r + 0.587*g + 0.114*b) / 255;
    return lum > 0.6 ? '#000' : '#fff';
  }

  function updateSpecHeaderColors(c1, c2) {
    if (specTh1) {
      specTh1.style.background = c1 || '';
      specTh1.style.color = textColorForBg(c1);
    }
    if (specTh2) {
      specTh2.style.background = c2 || '';
      specTh2.style.color = textColorForBg(c2);
    }
  }

  function updateSpecHeaderText() {
    try {
      if (specTh1 && title1) specTh1.textContent = title1.textContent || (name1 && name1.value) || specTh1.textContent;
      if (specTh2 && title2) specTh2.textContent = title2.textContent || (name2 && name2.value) || specTh2.textContent;
    } catch (e) { /* ignore */ }
  }

  // compute min/max for the last 5 seconds and update SpecTable; color cells red if below MIN_SPEC, green if over
  function updateSpecTable() {
    const lastMs = 5000;
    const now = Date.now();
    const cutoff = now - lastMs;
    function compute(data) {
      const visible = data.filter(d => d.t >= cutoff && !isNaN(d.v));
      if (!visible.length) return null;
      let min = visible[0].v, max = visible[0].v, sum = 0;
      for (const p of visible) { if (p.v < min) min = p.v; if (p.v > max) max = p.v; sum += p.v; }
      const avg = sum / visible.length;
      return { min, max, avg };
    }
    const r1 = compute(data1);
    const r2 = compute(data2);
    function apply(avgEl, r) {
      if (!avgEl) return;
      if (!r) {
        avgEl.textContent = '-- g';
        avgEl.style.background = '';
        avgEl.title = '';
        return;
      }
      avgEl.textContent = r.avg.toFixed(2) + ' g';
      avgEl.title = `5s avg: ${r.avg.toFixed(2)} g — min: ${r.min.toFixed(2)} g, max: ${r.max.toFixed(2)} g`;
      const avg = r.avg;
      if (avg >= MIN_SPEC) {
        avgEl.style.background = '#c8e6c9';
      } else {
        const deficit = MIN_SPEC - avg;
        if (deficit <= 10) {
          avgEl.style.background = '#ffe0b2';
        } else {
          avgEl.style.background = '#ffcdd2';
        }
      }
    }
    apply(specAvg1El, r1);
    apply(specAvg2El, r2);
  }

  setInterval(drawAll, 1000);

})();

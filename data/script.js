(() => {
  const statusEl = document.getElementById('status');
  const calibrateBtn = document.getElementById('calibrateBtn');
  const calScale = document.getElementById('calScale');
  const calResult = document.getElementById('calResult');
  const tareAllBtn = document.getElementById('tareAllBtn');
  const clearBtn = document.getElementById('clearBtn');

  // The UI will only display graphs for nodes that send data via ESP-NOW.
  // Local scale boxes (previously shown) have been removed per user request.

  // Dynamic child graphs: id -> { container, canvas, ctx, data[], lastSeen, name, color }
  const dynamicContainer = document.getElementById('dynamicGraphs');
  const childGraphs = new Map();
  const MAX_CHILD_GRAPHS = 4;
  const CHILD_TIMEOUT_MS = 5 * 60 * 1000; // remove after 5 minutes of no data

  // Load persisted child names/colors from localStorage
  const persistedChildNames = JSON.parse(localStorage.getItem('childNames') || '{}');
  const persistedChildColors = JSON.parse(localStorage.getItem('childColors') || '{}');

  function persistChildSettings() {
    const names = {};
    const colors = {};
    childGraphs.forEach((g, id) => { names[id] = g.name || ''; colors[id] = g.color || ''; });
    localStorage.setItem('childNames', JSON.stringify(Object.assign({}, persistedChildNames, names)));
    localStorage.setItem('childColors', JSON.stringify(Object.assign({}, persistedChildColors, colors)));
  }

  function createChildGraph(id, firstValue) {
    // If already exists, return it
    if (childGraphs.has(id)) return childGraphs.get(id);

    // enforce max
    if (childGraphs.size >= MAX_CHILD_GRAPHS) {
      // remove least recently seen to make space
      let oldestId = null; let oldestTime = Infinity;
      childGraphs.forEach((g, k) => { if (g.lastSeen < oldestTime) { oldestId = k; oldestTime = g.lastSeen; } });
      if (oldestId !== null) removeChildGraph(oldestId);
    }

    const card = document.createElement('div'); card.className = 'dynamicGraph';
    const header = document.createElement('div'); header.className = 'dgHeader';
    const titleRow = document.createElement('div'); titleRow.className = 'dgTitleRow';
    const title = document.createElement('div'); title.textContent = 'Node ' + id; title.style.fontWeight = '600';
    const nameInput = document.createElement('input'); nameInput.className = 'dgNameInput';
    nameInput.placeholder = 'Custom name...';
    nameInput.value = persistedChildNames[id] || ('Node ' + id);
    const colorInput = document.createElement('input'); colorInput.type = 'color'; colorInput.className = 'dgColor';
    colorInput.value = persistedChildColors[id] || '#0077cc';
    titleRow.appendChild(title); titleRow.appendChild(nameInput);
    header.appendChild(titleRow);
    const controls = document.createElement('div');
    const tareBtn = document.createElement('button'); tareBtn.className = 'dgTare'; tareBtn.textContent = 'Tare';
    const removeBtn = document.createElement('button'); removeBtn.className = 'dgRemove'; removeBtn.textContent = 'Remove';
    controls.appendChild(colorInput); controls.appendChild(tareBtn); controls.appendChild(removeBtn);
    header.appendChild(controls);
    card.appendChild(header);
    const canvas = document.createElement('canvas'); canvas.className = 'graphCanvas'; canvas.width = 700; canvas.height = 180;
    card.appendChild(canvas);
    dynamicContainer.prepend(card);

    const g = { container: card, canvas, ctx: canvas.getContext('2d'), data: [], lastSeen: Date.now(), name: nameInput.value || ('Node ' + id), color: colorInput.value };
    // initial value
    if (firstValue !== undefined && !isNaN(firstValue)) g.data.push({ t: Date.now(), v: Number(firstValue) });

    // wire inputs
    nameInput.addEventListener('change', () => { g.name = nameInput.value || ('Node ' + id); persistChildSettings(); });
    colorInput.addEventListener('input', () => { g.color = colorInput.value; persistChildSettings(); drawAll(); });
    tareBtn.addEventListener('click', () => sendTareCommand(id));
    removeBtn.addEventListener('click', () => removeChildGraph(id));

    childGraphs.set(String(id), g);
    persistChildSettings();
    return g;
  }

  function removeChildGraph(id) {
    const key = String(id);
    const g = childGraphs.get(key);
    if (!g) return;
    try { g.container.remove(); } catch (e) {}
    childGraphs.delete(key);
    persistChildSettings();
  }

  function processChildren(obj) {
    if (!obj || !obj.children) return;
    const now = Date.now();
    Object.keys(obj.children).forEach(k => {
      const val = obj.children[k];
      const g = createChildGraph(k, val);
      // update data
      if (val === null || val === undefined || isNaN(val)) {
        g.data.push({ t: now, v: NaN });
      } else {
        g.data.push({ t: now, v: Number(val) });
        g.lastSeen = now;
      }
      // trim to window
      const cutoff = now - WINDOW_MS;
      while (g.data.length && g.data[0].t < cutoff) g.data.shift();
    });
  }

  // Periodically remove stale child graphs
  setInterval(() => {
    const now = Date.now();
    const toRemove = [];
    childGraphs.forEach((g, id) => { if (now - g.lastSeen > CHILD_TIMEOUT_MS) toRemove.push(id); });
    toRemove.forEach(id => removeChildGraph(id));
  }, 30 * 1000);


  // Spec table elements and input (use robust selector to get the inner input)
  const specInputEl = document.querySelector('#specInput input') || document.getElementById('specInput');
  const loadSpecBtn = document.getElementById('loadSpecBtn');
  const specTable = document.getElementById('SpecTable');

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
    // no local scales to initialize; dynamic graphs will appear as data arrives
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
        // handle dynamic child nodes (parent mode)
        try { processChildren(obj); } catch (e) { /* ignore */ }
        // trim child graphs and draw
        const now = Date.now();
        childGraphs.forEach((g) => {
          const cutoff = now - WINDOW_MS;
          while (g.data.length && g.data[0].t < cutoff) g.data.shift();
        });
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

  // Tare all nodes - send requests to all connected child nodes
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
    // Colors for child graphs are managed per-graph; just redraw the table/graphs
    drawAll();
  }
  if (color1El) color1El.addEventListener('input', applyColors);
  if (color2El) color2El.addEventListener('input', applyColors);
  applyColors();

  // edit UI (no local scale edit controls since local UI removed)

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
  // edit controls removed for local scales (no local boxes)

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
    // draw dynamic child graphs only
    childGraphs.forEach((g, id) => {
      drawGraph(g.canvas, g.ctx, g.data, g.color || '#0077cc');
      try {
        const titleEl = g.container.querySelector('.dgTitleRow div');
        if (titleEl) titleEl.textContent = (g.name || ('Node ' + id));
      } catch (e) {}
    });
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

  // build/update the spec table dynamically based on active child graphs
  function updateSpecTable() {
    if (!specTable) return;
    // header row: first cell label, then one column per active child (order by lastSeen desc)
    const children = Array.from(childGraphs.entries()).sort((a,b) => b[1].lastSeen - a[1].lastSeen);
    // create table header
    const thead = document.createElement('thead');
    const hrow = document.createElement('tr');
    const thLabel = document.createElement('th'); thLabel.textContent = '5 Sec Avg'; hrow.appendChild(thLabel);
    children.forEach(([id, g]) => {
      const th = document.createElement('th');
      th.textContent = g.name || ('Node ' + id);
      th.style.background = g.color || '';
      th.style.color = textColorForBg(g.color || '#fff');
      hrow.appendChild(th);
    });
    thead.appendChild(hrow);

    // second row: averages
    const tbody = document.createElement('tbody');
    const avgRow = document.createElement('tr');
    const tdLabel = document.createElement('td'); tdLabel.textContent = '';
    avgRow.appendChild(tdLabel);
    const now = Date.now(); const cutoff = now - 5000;
    children.forEach(([id, g]) => {
      const visible = g.data.filter(d => d.t >= cutoff && !isNaN(d.v));
      const td = document.createElement('td');
      if (!visible.length) {
        td.textContent = '-- g';
      } else {
        const sum = visible.reduce((s,p) => s + p.v, 0);
        const avg = sum / visible.length;
        td.textContent = avg.toFixed(2) + ' g';
      }
      avgRow.appendChild(td);
    });
    tbody.appendChild(avgRow);

    // replace table contents
    specTable.innerHTML = '';
    specTable.appendChild(thead);
    specTable.appendChild(tbody);
  }

  // compute min/max for the last 5 seconds and update SpecTable; color cells red if below MIN_SPEC, green if over
  function updateSpecTable() {
    const lastMs = 5000;
    const now = Date.now();
    const cutoff = now - lastMs;
    // No local scales: show placeholders for spec averages
    if (specAvg1El) { specAvg1El.textContent = '-- g'; specAvg1El.style.background = ''; specAvg1El.title = ''; }
    if (specAvg2El) { specAvg2El.textContent = '-- g'; specAvg2El.style.background = ''; specAvg2El.title = ''; }
  }

  setInterval(drawAll, 1000);

})();
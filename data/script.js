(() => {
  const statusEl = document.getElementById('status');
  const calibrateBtn = document.getElementById('calibrateBtn');
  const calScale = document.getElementById('calScale');
  const timeSpan = document.getElementById('timeSpan');
  const tareAllBtn = document.getElementById('tareAllBtn');


  // The UI will only display graphs for nodes that send data via ESP-NOW.

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

  const scaleBoxesContainer = document.getElementById('scaleBoxes');

  function createChildGraph(id, firstValue, serverName) {
    const key = String(id);
    // If already exists, return it
    if (childGraphs.has(key)) return childGraphs.get(key);

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
    const title = document.createElement('div'); title.textContent = serverName || ('Node ' + id); title.style.fontWeight = '600';
    // name input (hidden until editing)
    const nameInput = document.createElement('input'); nameInput.className = 'dgNameInput';
    nameInput.placeholder = 'Custom name...';
    nameInput.style.display = 'none';
    nameInput.value = persistedChildNames[key] || serverName || ('Node ' + id);
    const weightEl = document.createElement('div'); weightEl.className = 'dgWeight'; weightEl.style.marginLeft = '8px'; weightEl.textContent = '-- g';
    const tareBtn = document.createElement('button'); tareBtn.className = 'dgTare'; tareBtn.textContent = 'Tare';

    // color picker (hidden until editing)
    const colorInput = document.createElement('input'); colorInput.type = 'color'; colorInput.className = 'dgColor';
    colorInput.style.display = 'none';
    colorInput.value = persistedChildColors[key] || '#525c63ff';
    titleRow.appendChild(title); titleRow.appendChild(nameInput); titleRow.appendChild(weightEl); titleRow.appendChild(tareBtn);
    header.appendChild(titleRow);
    const controls = document.createElement('div');
    const editBtn = document.createElement('button'); editBtn.className = 'material-icons'; editBtn.innerHTML = 'mode_edit'; editBtn.title = 'Edit';
    const saveBtn = document.createElement('button'); saveBtn.className = 'material-icons'; saveBtn.innerHTML = 'save'; saveBtn.style.display = 'none'; saveBtn.title = 'Save'; saveBtn.style.color = 'green';
    const cancelBtn = document.createElement('button'); cancelBtn.className = 'material-icons'; cancelBtn.innerHTML = 'undo'; cancelBtn.style.display = 'none'; cancelBtn.title = 'Cancel'; cancelBtn.style.color = 'red';
    const removeBtn = document.createElement('button'); removeBtn.className = 'material-icons'; removeBtn.innerHTML = 'remove'; removeBtn.title = 'Remove';
    controls.appendChild(colorInput); controls.appendChild(editBtn); controls.appendChild(saveBtn); controls.appendChild(cancelBtn); controls.appendChild(removeBtn);
    header.appendChild(controls);
    card.appendChild(header);
    const canvas = document.createElement('canvas'); canvas.className = 'graphCanvas'; canvas.width = 700; canvas.height = 180;
    card.appendChild(canvas);
    // resolve container at runtime (in case DOM changed); prefer dynamicGraphs
    try {
      const container = document.getElementById('dynamicGraphs') || document.getElementById('scaleBoxes') || document.body;
      container.prepend(card);
    } catch (err) {
      console.error('Failed to insert dynamic graph card', err);
      document.body.prepend(card);
    }

    const g = { container: card, canvas, ctx: canvas.getContext('2d'), data: [], lastSeen: Date.now(), name: nameInput.value || serverName || ('Node ' + id), color: colorInput.value, weightEl, titleEl: title, nameInput, colorInput, editBtn, saveBtn, cancelBtn };
    // initial value
    if (firstValue !== undefined && !isNaN(firstValue)) g.data.push({ t: Date.now(), v: Number(firstValue) });

    // wire inputs
    // edit flow
    editBtn.addEventListener('click', () => {
      editBtn.style.display = 'none';
      saveBtn.style.display = '';
      cancelBtn.style.display = '';
      // hide remove while editing
      if (removeBtn) removeBtn.style.display = 'none';
      // hide the static title and show the editable name input
      try { if (title) title.style.display = 'none'; } catch (e) {}
      nameInput.style.display = '';
      colorInput.style.display = '';
      nameInput.focus();
    });
    saveBtn.addEventListener('click', () => {
      g.name = nameInput.value || ('Node ' + id);
      g.color = colorInput.value;
      title.textContent = g.name;
      // apply background and readable text color to the card
      try {
        g.container.style.background = g.color || '';
        const textCol = textColorForBg(g.color || '');
        if (g.titleEl) g.titleEl.style.color = textCol;
        if (g.weightEl) g.weightEl.style.color = textCol;
      } catch (e) {}
      editBtn.style.display = '';
      saveBtn.style.display = 'none';
      cancelBtn.style.display = 'none';
      // restore remove visibility
      if (removeBtn) removeBtn.style.display = '';
      // restore title visibility and hide name input
      try { if (title) title.style.display = ''; } catch (e) {}
      nameInput.style.display = 'none';
      colorInput.style.display = 'none';
      persistChildSettings();
      setStatus('Settings saved');
      drawAll();
    });
    cancelBtn.addEventListener('click', () => {
      nameInput.value = g.name;
      colorInput.value = g.color;
      editBtn.style.display = '';
      saveBtn.style.display = 'none';
      cancelBtn.style.display = 'none';
      // restore remove visibility
      if (removeBtn) removeBtn.style.display = '';
      // restore title visibility and hide name input
      try { if (title) title.style.display = ''; } catch (e) {}
      nameInput.style.display = 'none';
      colorInput.style.display = 'none';
    });
    nameInput.addEventListener('change', () => { /* no-op until saved */ });
    colorInput.addEventListener('input', () => { /* preview handled on save/draw */ });
    tareBtn.addEventListener('click', () => sendTareCommand(id));
    removeBtn.addEventListener('click', () => removeChildGraph(id));

    childGraphs.set(key, g);
    // apply initial color to the container/title/weight elements
    try {
      g.container.style.background = g.color || '';
      const textCol = textColorForBg(g.color || '');
      if (g.titleEl) g.titleEl.style.color = textCol;
      if (g.weightEl) g.weightEl.style.color = textCol;
    } catch (e) {}
    persistChildSettings();
    console.log('Created child graph', key, serverName);
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
    // Debug: show which child keys arrived
    // try { console.log('processChildren keys:', Object.keys(obj.children)); } catch (e) {}
    const now = Date.now();
    // Support two formats: array of {id,weight,name} (server) or object map id->{weight,name}
    if (Array.isArray(obj.children)) {
      obj.children.forEach(entry => {
        if (!entry) return;
        const k = String(entry.id || '');
        const val = (entry.weight === undefined) ? NaN : entry.weight;
        const serverName = entry.name;
        const g = createChildGraph(k, val, serverName);
        if (val === null || val === undefined || isNaN(val)) g.data.push({ t: now, v: NaN }); else { g.data.push({ t: now, v: Number(val) }); g.lastSeen = now; }
        const cutoff = now - WINDOW_MS; while (g.data.length && g.data[0].t < cutoff) g.data.shift();
      });
      return;
    }
    Object.keys(obj.children).forEach(k => {
      const entry = obj.children[k];
      let val = undefined;
      let serverName = undefined;
      if (entry !== null && typeof entry === 'object') {
        val = entry.weight;
        serverName = entry.name;
      } else {
        val = entry;
      }
      const g = createChildGraph(k, val, serverName);
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
  // Default time window = 5 minutes
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
    const which = Number((calScale && calScale.value) ? calScale.value : 1) || 1;
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
  

  applyColors();

  // Reset persisted child display settings to defaults
  const resetBtn = document.getElementById('resetBtn');
  if (resetBtn) resetBtn.addEventListener('click', () => {
    if (!confirm('Reset child names and colors to defaults?')) return;
    fetch('/settings/reset', { method: 'POST' })
      .then(r => r.json())
      .then(j => {
        // clear persisted child name/color overrides and reset current graphs
        Object.keys(persistedChildNames).forEach(k => delete persistedChildNames[k]);
        Object.keys(persistedChildColors).forEach(k => delete persistedChildColors[k]);
        localStorage.removeItem('childNames');
        localStorage.removeItem('childColors');
        childGraphs.forEach((g, id) => {
          g.name = 'Node ' + id;
          g.color = '#525c63ff';
        });
        persistChildSettings();
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
      if (timeSpan) timeSpan.value = String(ms);
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
        const titleEl = g.titleEl || g.container.querySelector('.dgTitleRow div');
        if (titleEl) titleEl.textContent = (g.name || ('Node ' + id));
        // update current weight display
        const last = g.data.length ? g.data[g.data.length - 1] : null;
        if (g.weightEl) {
          g.weightEl.textContent = (last && !isNaN(last.v)) ? (last.v.toFixed(2) + ' g') : '-- g';
        }
      } catch (e) {}
    });
    // update the spec table based on active child graphs
    if (typeof updateSpecTableDetailed === 'function') updateSpecTableDetailed(); else updateSpecTable();
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
  function updateSpecTableDetailed() {
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

  // (updateSpecTable is defined earlier to build the table from active child graphs)

  setInterval(drawAll, 2000);  // Reduced from 1000ms to lower client-side redraw load

})();
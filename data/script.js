(() => {
  const weightEl = document.getElementById('weight');
  const statusEl = document.getElementById('status');
  const tareBtn = document.getElementById('tareBtn');

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
        if (obj.weight === null || obj.weight === undefined || isNaN(obj.weight)) {
          weightEl.textContent = '-- g';
        } else {
          weightEl.textContent = obj.weight.toFixed(2) + ' g';
        }
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

})();

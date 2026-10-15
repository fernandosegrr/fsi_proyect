#ifndef PAGINA_H
#define PAGINA_H

const char html[] PROGMEM = R"=====(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <meta charset="UTF-8">
  <meta http-equiv="Cache-Control" content="no-store, no-cache, must-revalidate">
  <meta http-equiv="Pragma" content="no-cache">
  <meta http-equiv="Expires" content="0">
  <title>Control del carro</title>
  <style>
    /* Mismos colores que la app (docs/diseno.md): gris azulado oscuro, un solo acento cian, rojo solo para el PARO. */
    * { box-sizing: border-box; margin: 0; padding: 0; user-select: none; -webkit-touch-callout: none; }

    body {
      background: #0C131A; color: #E7EDF3;
      font-family: system-ui, -apple-system, 'Segoe UI', Roboto, Arial, sans-serif;
      display: flex; flex-direction: column; align-items: center;
      justify-content: center; min-height: 100vh; overflow: hidden;
    }

    .header { text-align: center; margin-bottom: 24px; }
    h1 { font-size: 1.5rem; font-weight: 600; letter-spacing: 0.5px; color: #E7EDF3; }
    .sub { font-size: 0.85rem; color: #9BAEC0; margin-top: 4px; }

    .panel {
      background: #131C25; border: 1px solid #26323F; border-radius: 16px; padding: 24px 16px;
      width: 90%; max-width: 360px;
    }

    .row { display: flex; justify-content: center; gap: 16px; margin: 16px 0; }

    .btn {
      width: 80px; height: 80px; border-radius: 16px; font-size: 28px; cursor: pointer;
      display: flex; align-items: center; justify-content: center; touch-action: none;
      background: #1A2531; border: 1px solid #5B6E80; color: #E7EDF3;
      transition: transform 0.15s, background 0.15s;
    }
    .btn:active { transform: scale(0.97); background: #22C5DD; color: #03232B; border-color: #22C5DD; }

    .btn-stop { background: #D32F2F; border-color: #D32F2F; color: #FFFFFF; }
    .btn-stop:active { background: #B71C1C; border-color: #B71C1C; color: #FFFFFF; }

    .status { text-align: center; margin-top: 24px; font-size: 0.9rem; color: #9BAEC0; }
    #ws-status { color: #FF8A8F; font-weight: 600; }
    .connected #ws-status { color: #4ADE80; }

    /* Indicador de reconexión */
    #reconnect-btn {
      display: none; margin-top: 16px; padding: 12px 24px;
      background: #22C5DD; color: #03232B; border: none; border-radius: 28px;
      font-family: inherit; font-size: 0.95rem; font-weight: 600;
      cursor: pointer;
    }
    #reconnect-btn:active { opacity: 0.7; }
  </style>
</head>
<body>
  <div class="header">
    <h1>CARRO FSI</h1>
    <div class="sub">Control manual</div>
  </div>

  <div class="panel">
    <div class="row">
      <button class="btn" id="btnFwd">▲</button>
    </div>
    <div class="row">
      <button class="btn" id="btnLeft">◀</button>
      <button class="btn btn-stop" id="btnStop">■</button>
      <button class="btn" id="btnRight">▶</button>
    </div>
    <div class="row">
      <button class="btn" id="btnRev">▼</button>
    </div>
  </div>

  <div class="status" id="statusBox">Conexión: <span id="ws-status">OFFLINE</span></div>
  <button id="reconnect-btn" onclick="forceReconnect()">↺ RECONECTAR</button>

  <script>
    let ws = null;
    let activeCmd = "STOP";
    let heartbeatInterval  = null;
    let pingInterval       = null;
    let reconnectTimeout   = null;
    let isConnecting       = false;

    // ── WebSocket ─────────────────────────────────────────────────────────────
    function setStatus(online) {
      const box = document.getElementById('statusBox');
      const lbl = document.getElementById('ws-status');
      const btn = document.getElementById('reconnect-btn');
      if (online) {
        box.classList.add('connected');
        lbl.innerText = 'ONLINE';
        btn.style.display = 'none';
      } else {
        box.classList.remove('connected');
        lbl.innerText = 'OFFLINE';
        btn.style.display = 'inline-block';
      }
    }

    function scheduleReconnect(ms) {
      if (reconnectTimeout) clearTimeout(reconnectTimeout);
      reconnectTimeout = setTimeout(initWebSocket, ms || 2000);
    }

    function forceReconnect() {
      if (reconnectTimeout) clearTimeout(reconnectTimeout);
      if (pingInterval)     clearInterval(pingInterval);
      isConnecting = false;
      if (ws) { ws.onclose = null; ws.onerror = null; ws.close(); ws = null; }
      initWebSocket();
    }

    function initWebSocket() {
      if (isConnecting) return;
      isConnecting = true;

      // Limpiar instancia anterior
      if (ws) {
        ws.onopen    = null;
        ws.onclose   = null;
        ws.onerror   = null;
        ws.onmessage = null;
        ws.close();
        ws = null;
      }
      if (pingInterval) { clearInterval(pingInterval); pingInterval = null; }

      ws = new WebSocket('ws://' + window.location.hostname + '/ws');

      ws.onopen = () => {
        isConnecting = false;
        setStatus(true);

        // Esta página es de control MANUAL: pide el perfil de velocidad máxima (el carro arranca en AUTO, 210).
        ws.send('MODO_MANUAL');

        // Ping cada 800 ms para mantener el watchdog del ESP32 y la conexión viva
        // cuando el celular está en reposo (pantalla apagada). Cada 4 pings (3.2 s) se repite MODO_MANUAL:
        // si el carro volvió a AUTO por el watchdog, así se repone (repetirlo en el mismo perfil no frena nada).
        let pings = 0;
        pingInterval = setInterval(() => {
          if (ws && ws.readyState === WebSocket.OPEN) {
            ws.send('PING');
            if (++pings % 4 === 0) ws.send('MODO_MANUAL');
          }
        }, 800);
      };

      ws.onclose = () => {
        isConnecting = false;
        setStatus(false);
        if (pingInterval) { clearInterval(pingInterval); pingInterval = null; }
        if (heartbeatInterval) { clearInterval(heartbeatInterval); heartbeatInterval = null; }
        activeCmd = "STOP";
        scheduleReconnect(2000);
      };

      ws.onerror = () => {
        // El onclose se disparará justo después; solo marcar
        isConnecting = false;
      };
    }

    function sendCmd(cmd) {
      if (ws && ws.readyState === WebSocket.OPEN) ws.send(cmd);
    }

    // ── Control de botones ───────────────────────────────────────────────────
    function startCmd(cmd) {
      activeCmd = cmd;
      sendCmd(activeCmd);
      if (heartbeatInterval) clearInterval(heartbeatInterval);
      // Heartbeat 250 ms → mantiene watchdog activo mientras se pulsa
      heartbeatInterval = setInterval(() => sendCmd(activeCmd), 250);
    }

    function stopCmd() {
      if (activeCmd === "STOP") return;  // ya parado
      activeCmd = "STOP";
      sendCmd("STOP");
      if (heartbeatInterval) { clearInterval(heartbeatInterval); heartbeatInterval = null; }
    }

    function ebrakeCmd() {
      activeCmd = "STOP";
      sendCmd("EBRAKE");
      if (heartbeatInterval) { clearInterval(heartbeatInterval); heartbeatInterval = null; }
    }

    function bindBtn(id, cmd) {
      const btn = document.getElementById(id);
      let isTouch = false;

      btn.addEventListener('touchstart',  (e) => { e.preventDefault(); isTouch = true; startCmd(cmd); },  { passive: false });
      btn.addEventListener('touchend',    (e) => { e.preventDefault(); stopCmd(); setTimeout(() => isTouch = false, 300); }, { passive: false });
      btn.addEventListener('touchcancel', (e) => { e.preventDefault(); stopCmd(); setTimeout(() => isTouch = false, 300); }, { passive: false });

      btn.addEventListener('mousedown',  () => { if (!isTouch) startCmd(cmd); });
      const onRelease = () => { if (!isTouch) stopCmd(); };
      btn.addEventListener('mouseup',    onRelease);
      btn.addEventListener('mouseleave', onRelease);
    }

    // ── Reconexión al volver la pantalla ─────────────────────────────────────
    // Cuando el celular apaga la pantalla, el navegador puede suspender el WS.
    // Al volver, forzamos reconexión inmediata.
    document.addEventListener('visibilitychange', () => {
      if (document.visibilityState === 'visible') {
        if (!ws || ws.readyState !== WebSocket.OPEN) {
          forceReconnect();
        }
      } else {
        // Pantalla se va a apagar → parar motores por seguridad
        sendCmd('EBRAKE');
      }
    });

    // También reconectar si la página recupera el foco (Safari/iOS)
    window.addEventListener('focus', () => {
      if (!ws || ws.readyState !== WebSocket.OPEN) {
        forceReconnect();
      }
    });

    // ── Inicio ───────────────────────────────────────────────────────────────
    window.addEventListener('load', () => {
      initWebSocket();

      bindBtn('btnFwd',   'FORWARD');
      bindBtn('btnLeft',  'LEFT');
      bindBtn('btnRight', 'RIGHT');
      bindBtn('btnRev',   'REVERSE');

      document.getElementById('btnStop').addEventListener('mousedown',  ebrakeCmd);
      document.getElementById('btnStop').addEventListener('touchstart', (e) => { e.preventDefault(); ebrakeCmd(); }, { passive: false });

      // Soltar fuera de cualquier botón también para
      document.addEventListener('mouseup',  stopCmd);
      document.addEventListener('touchend', stopCmd);
    });
  </script>
</body>
</html>
)=====";

#endif

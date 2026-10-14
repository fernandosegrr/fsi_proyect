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
  <title>Wall-E Control</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; user-select: none; -webkit-touch-callout: none; }

    body {
      background: #050505;
      color: #d37628;
      font-family: 'Courier New', Courier, monospace;
      display: flex; flex-direction: column; align-items: center;
      justify-content: center; min-height: 100vh;
      overflow: hidden; position: relative;
    }
    body::before {
      content: ""; position: absolute; top: 0; left: 0; width: 100%; height: 100%;
      background: repeating-linear-gradient(0deg, transparent, transparent 2px, rgba(0,0,0,0.1) 2px, rgba(0,0,0,0.1) 4px);
      pointer-events: none; z-index: 10;
    }
    body::after {
      content: ""; position: absolute; top: 0; left: 0; width: 100%; height: 100%;
      background: radial-gradient(circle, transparent 50%, rgba(0,0,0,0.8) 100%);
      pointer-events: none; z-index: 9;
    }

    .header { text-align: center; margin-bottom: 24px; z-index: 20; position: relative; }

    h1 {
      font-family: 'Arial Black', 'Arial Bold', Impact, sans-serif;
      font-size: 2.5rem; letter-spacing: 5px;
      color: #e68a00; text-shadow: 0 0 10px rgba(230,138,0,0.5); margin-bottom: 5px;
    }

    .walle-face { width: 120px; height: auto; margin-bottom: 10px; }
    .eye-lens { animation: blink 6s infinite; transform-origin: center; }
    @keyframes blink { 0%, 96%, 98%, 100% { transform: scaleY(1); } 97%, 99% { transform: scaleY(0.1); } }

    .panel {
      background: linear-gradient(135deg, #2a2a2a, #111);
      border: 2px solid #555; border-radius: 10px; padding: 30px 20px;
      width: 90%; max-width: 380px; position: relative; z-index: 20;
      box-shadow: 0 10px 30px rgba(0,0,0,0.8), inset 0 0 15px rgba(0,0,0,0.5);
    }
    .rivet {
      position: absolute; width: 8px; height: 8px;
      background: radial-gradient(circle, #888, #333); border-radius: 50%;
      box-shadow: 1px 1px 2px #000, inset -1px -1px 2px rgba(0,0,0,0.5);
    }
    .r-tl { top: 10px; left: 10px; } .r-tr { top: 10px; right: 10px; }
    .r-bl { bottom: 10px; left: 10px; } .r-br { bottom: 10px; right: 10px; }

    .row { display: flex; justify-content: center; gap: 15px; margin: 15px 0; }

    .btn {
      width: 85px; height: 85px; border-radius: 8px; font-size: 28px; cursor: pointer;
      display: flex; align-items: center; justify-content: center; touch-action: none;
      background: linear-gradient(to bottom, #d37628, #a85215);
      border: 1px solid #ff9d4d; color: #111;
      box-shadow: 0 6px 0 #6b3105, 0 10px 10px rgba(0,0,0,0.5);
      transition: all 0.1s; text-shadow: 0 1px 1px rgba(255,255,255,0.3);
    }
    .btn:active {
      transform: translateY(6px);
      box-shadow: 0 0px 0 #6b3105, 0 2px 4px rgba(0,0,0,0.5);
      background: linear-gradient(to bottom, #e68a00, #b85d19);
    }

    .btn-stop {
      background: linear-gradient(to bottom, #ff4c4c, #b30000);
      border-color: #ff9999;
      box-shadow: 0 6px 0 #660000, 0 10px 10px rgba(0,0,0,0.5);
      color: white;
    }
    .btn-stop:active {
      background: linear-gradient(to bottom, #ff6666, #cc0000);
      box-shadow: 0 0px 0 #660000; transform: translateY(6px);
    }

    /* Badge MAX en botones de giro */
    .btn-turn { position: relative; }
    .btn-turn::after {
      content: "MAX"; position: absolute; bottom: 5px; right: 6px;
      font-size: 8px; color: rgba(255,200,50,0.75); letter-spacing: 0.5px;
    }

    .status {
      text-align: center; margin-top: 22px;
      font-size: 0.9rem; z-index: 20; position: relative; color: #888;
    }
    #ws-status { color: #ff4c4c; font-weight: bold; }
    .connected #ws-status { color: #00ffcc; }

    /* Indicador de reconexión */
    #reconnect-btn {
      display: none; margin-top: 14px; padding: 8px 20px;
      background: #e68a00; color: #111; border: none; border-radius: 6px;
      font-family: inherit; font-size: 0.85rem; letter-spacing: 1px;
      cursor: pointer; font-weight: bold; z-index: 20; position: relative;
    }
    #reconnect-btn:active { opacity: 0.7; }
  </style>
</head>
<body>
  <div class="header">
    <svg class="walle-face" viewBox="0 0 100 60" xmlns="http://www.w3.org/2000/svg">
      <path d="M 30 50 L 70 50 L 65 60 L 35 60 Z" fill="#555" stroke="#333" stroke-width="2"/>
      <rect x="45" y="40" width="10" height="10" fill="#777" stroke="#222" stroke-width="2"/>
      <g transform="translate(10, 10) rotate(-15)">
        <path d="M 0 10 Q 0 0 15 0 L 35 15 Q 40 20 35 30 L 15 35 Q 0 35 0 20 Z" fill="#b85d19" stroke="#333" stroke-width="2"/>
        <circle cx="18" cy="18" r="10" fill="#111"/>
        <circle class="eye-lens" cx="18" cy="18" r="7" fill="#00d9f5" opacity="0.8"/>
        <circle cx="15" cy="15" r="2" fill="#fff" opacity="0.6"/>
      </g>
      <g transform="translate(50, 10) rotate(15)">
        <path d="M 40 10 Q 40 0 25 0 L 5 15 Q 0 20 5 30 L 25 35 Q 40 35 40 20 Z" fill="#b85d19" stroke="#333" stroke-width="2"/>
        <circle cx="22" cy="18" r="10" fill="#111"/>
        <circle class="eye-lens" cx="22" cy="18" r="7" fill="#00d9f5" opacity="0.8"/>
        <circle cx="25" cy="15" r="2" fill="#fff" opacity="0.6"/>
      </g>
    </svg>
    <h1>WALL-E</h1>
  </div>

  <div class="panel">
    <div class="rivet r-tl"></div><div class="rivet r-tr"></div>
    <div class="rivet r-bl"></div><div class="rivet r-br"></div>

    <div class="row">
      <button class="btn" id="btnFwd">▲</button>
    </div>
    <div class="row">
      <button class="btn btn-turn" id="btnLeft">◀</button>
      <button class="btn btn-stop" id="btnStop">■</button>
      <button class="btn btn-turn" id="btnRight">▶</button>
    </div>
    <div class="row">
      <button class="btn" id="btnRev">▼</button>
    </div>
  </div>

  <div class="status" id="statusBox">SYS: <span id="ws-status">OFFLINE</span></div>
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
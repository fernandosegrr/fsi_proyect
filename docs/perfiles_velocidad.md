# Perfiles de velocidad: MANUAL (255) y AUTO (210)

El carro tiene dos perfiles de velocidad. Solo cambia la potencia al **avanzar y retroceder** después del arranque; los pivotes (giros) siguen a
255 en los dos y el arranque a plena potencia (250 ms) también.

| | PERFIL_MANUAL | PERFIL_AUTO |
|---|---|---|
| Para qué | manejar con los botones de la app (velocidad máxima) | rutas del modo semiautónomo, prueba de obstáculo y calibración |
| `VELOCIDAD_IZQ` / `VELOCIDAD_DER` | 252 / 255 (ajuste recto **validado**: 3 cm de desvío a 150 cm) | 207 / 210 (el mismo cociente; **sin medir**) |
| `ARRANQUE_PLENO_MS` | 250 ms | 250 ms |
| Velocidad medida | ~38 cm/s (26.3 ms/cm) | ~19 cm/s (51.7 ms/cm) |

Las constantes están en `firmware_carro/config.h` (`PERFIL_MANUAL_VELOCIDAD_IZQ`, `PERFIL_AUTO_ARRANQUE_PLENO_MS`, etc.) y la lógica en
`firmware_carro/carro.c` (tabla `PERFILES`). Antes de este cambio el carro solo tenía el perfil de 210 (experimento del 13 oct, confirmado: arranca,
va más lento y frena bien).

## Protocolo (sin parámetros)

- `MODO_MANUAL` → el carro contesta `ACK_MANUAL`. `MODO_AUTO` → `ACK_AUTO`.
- Si el carro se está moviendo y el perfil **cambia**, primero hace STOP. Repetir el mismo perfil no frena nada (la app lo repite cada ~3 s).
- Estos comandos **no** son un movimiento: no se guardan como comando actual (un FORWARD que ya estaba activo se vuelve a aplicar después de cambiar
  de perfil, porque el cambio lo detuvo) y renuevan el reloj del watchdog como cualquier comando real.
- **Al arrancar, al conectarse o reconectarse el WebSocket y tras el watchdog el perfil es siempre AUTO** (el seguro). La app pide MODO_MANUAL
  explícitamente. STOP, EBRAKE y PING no cambian el perfil. El heartbeat y el STOP por pérdida de señal (1000 ms) son iguales en los dos.

## En la app

- **ControlActivity** abre en manual: manda `MODO_MANUAL` al abrir la conexión y al reconectar, lo repite cada 4 PING (3.2 s) para reponerlo si el
  carro volvió a AUTO por el watchdog, y muestra el perfil («✓ confirmado por el carro» o «pidiendo…»). Al pasar a **Semiautónomo** pide `MODO_AUTO`.
- **Rutas y prueba de obstáculo:** al tocar Iniciar la app pide `MODO_AUTO` y **espera `ACK_AUTO` hasta 1 s**; sin él **no arranca** y muestra un ERROR
  claro («el carro no confirmó el perfil AUTO… ¿está cargado el firmware nuevo?»). Con el firmware viejo (que no conoce el comando) pasa lo mismo, que
  es lo seguro.
- **CalibracionActivity** pide siempre `MODO_AUTO` y **no deja probar** hasta que el carro confirma AUTO (si no, mediría otra velocidad).
- **Página web del carro** (`pagina.h`): es de control manual, así que pide `MODO_MANUAL` al abrir y cada 4 PING.
- **Servidor falso** (`pruebas_firmware/servidor_falso.py`): hace lo mismo que el firmware (ACK, STOP primero, AUTO al conectar y tras el watchdog).

## En manual la detección de obstáculos no frena (por defecto)

En el perfil MANUAL la app **no usa la detección de obstáculos para frenar**: la cámara solo se usa en el modo semiautónomo y manda quien maneja. A
255 el carro recorre ~15 cm entre que el obstáculo aparece y que se detiene (`docs/latencia.md`), así que hay que frenar uno mismo: el botón **STOP** de
la app y el comando **EBRAKE** del firmware (hace lo mismo que STOP y siempre se ejecuta, aunque se repita) siguen disponibles. No hay una opción para
frenar por detección en manual; si algún día se quiere, habría que abrir la cámara también en manual y mandar `EBRAKE` cuando el navegador lo pida.

## Pruebas

- **Firmware en PC** (`pruebas_firmware`, `make test`): 87 casos, 0 fallos, entre ellos los perfiles (ACK, valores 252/255 y 207/210, STOP primero al cambiar
  en movimiento, mismo perfil sin frenar, watchdog y conexión vuelven a AUTO, STOP/EBRAKE/PING no cambian el perfil) y la comparación aleatoria con un
  modelo del firmware (2 × 300 000 pasos, ahora con MODO_MANUAL/MODO_AUTO mezclados en las entradas).
- **Servidor falso** (`probar_servidor_falso.py`): 14 casos, 0 fallos (ACK, idempotencia, STOP por cambio de perfil, watchdog).
- **Teléfono contra el servidor falso** (por `adb reverse tcp:8080`, sin tocar el WiFi del carro): la app manda `MODO_MANUAL` al abrir (antes del primer PING),
  el servidor contesta `ACK_MANUAL`, la pantalla lo confirma y lo repite cada 3.2 s.

## PENDIENTE HARDWARE: rectas de 150 cm en los dos perfiles

Cargar el DevKit (`scripts\cargar.ps1 nuevo`). Para cada perfil, 3 pasadas en línea recta de 150 cm sobre el mismo piso, anotando el desvío lateral
en cm al final (+ derecha, − izquierda):

1. **Manual (255):** en la app abre Control (modo Manual) y confirma «Perfil: MANUAL (255) ✓». Mantén **adelante** presionado hasta ~150 cm y suelta.
2. **Auto (210):** pasa a **Semiautónomo**, «Obstáculos: botones», y la ruta de recta (**Prueba de obstáculo (1 m)**: 100 cm; o **Esquivar objeto grande
   (1.4 m)** sin objeto) con «Perfil: AUTO (210) ✓». Anota el desvío.
3. Si el automático se desvía, ajusta `PERFIL_AUTO_VELOCIDAD_IZQ` o `_DER` (baja el lado que empuja más: si se va a la derecha, baja la izquierda) y
   vuelve a cargar. Reporta: desvío manual (3 pasadas) y desvío automático (3 pasadas).

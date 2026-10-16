# Perfiles de velocidad: MANUAL (255) y AUTO (210)

El carro tiene dos perfiles de velocidad. Solo cambia la potencia al **avanzar y retroceder** después del arranque; los pivotes (giros) siguen a
255 en los dos y el arranque a plena potencia (250 ms) también.

| | PERFIL_MANUAL | PERFIL_AUTO |
|---|---|---|
| Para qué | manejar con los botones de la app (velocidad máxima) | rutas del modo semiautónomo, prueba de obstáculo y calibración |
| `VELOCIDAD_DER` (el tope) y balance del lado izquierdo | 255 y **988** → izquierdo 252 (ajuste recto **validado**: 3 cm de desvío a 150 cm) | 210 y **986** → izquierdo 207 (el mismo cociente; **medido: no alcanza**, ver «Equilibrio de los lados») |
| `ARRANQUE_PLENO_MS` | 250 ms | 250 ms |
| Velocidad medida | ~38 cm/s (26.3 ms/cm) | ~19 cm/s (51.7 ms/cm) |

Las constantes están en `firmware_carro/config.h` (`PERFIL_MANUAL_VELOCIDAD_IZQ`, `PERFIL_AUTO_ARRANQUE_PLENO_MS`, etc.) y la lógica en
`firmware_carro/carro.c` (tabla `PERFILES`). Antes de este cambio el carro solo tenía el perfil de 210 (experimento del 13 oct, confirmado: arranca,
va más lento y frena bien).

## Equilibrio de los lados (recta): balance por perfil

**El problema (prueba real, 15 oct):** en AUTO (210) en recta el carro se desvía a la derecha ~**6 cm en 2000 ms** (~38 cm recorridos): una diferencia de ~5 a 10 % entre los dos lados.
La tabla de antes (207 izquierdo / 210 derecho) corrige solo ~1.5 %.

**Qué cambió.** La potencia del lado izquierdo ya no es una constante: es `tope del perfil x balance / 1000`, con el **balance** (factor del lado izquierdo en milésimas, de **800 a 1000**)
guardado **por perfil** (AUTO y MANUAL) en la **memoria no volátil** del carro (Preferences del ESP32, claves `balAuto` y `balManual`). El balance **solo reduce el lado más rápido** (el izquierdo en este carro): el derecho
queda siempre en el tope del perfil y el izquierdo **nunca lo pasa** (probado con los 201 valores de 800 a 1000). Los valores por defecto son los de siempre: **986** en AUTO (207/210) y **988** en MANUAL
(252/255), así que sin calibrar nada cambia (`config.h`: `PERFIL_x_BALANCE_DEFECTO`). Un valor guardado fuera de 800 a 1000 se ignora al arrancar.

**Protocolo nuevo** (no cambia ninguno de los comandos de antes; la página web del carro no se toca, su hash sigue igual):

| La app manda | El carro contesta | Qué hace |
|---|---|---|
| `BAL_AUTO:NNN` | `ACK_BAL_AUTO:NNN` | Cambia el balance de AUTO y lo guarda (NNN de 800 a 1000, 3 o 4 cifras, sin ceros a la izquierda, sin signo ni espacios) |
| `BAL_MANUAL:NNN` | `ACK_BAL_MANUAL:NNN` | Igual para MANUAL |
| `BAL_AUTO:...` / `BAL_MANUAL:...` fuera de rango o mal escrito | `NAK_BAL` | No cambia nada |
| `BAL?` | `BAL:AAA:MMM` | Los balances actuales (AUTO y luego MANUAL) |

No son un movimiento: no frenan, no cambian el comando actual ni el perfil (como `MODO_*`, renuevan el watchdog); el cambio se aplica **en el siguiente arranque**. La app los manda **al conectar**
(los que tiene guardados en el teléfono; si nunca los cambió, no manda nada y vale lo que el carro tenga) y **al cambiarlos** (con ACK; si el carro no contesta en 1 s, avisa). El servidor falso hace lo mismo
(y conserva el balance al reconectar, como la memoria del carro).

**Calibración «Recta»** (Calibración → «Ajuste fino: giro y recta» → segundo paso; ver también `docs/calibracion.md`): una prueba por pantalla.

1. Pon el carro sobre una **línea de cinta**, mirando a lo largo de ella, con las pilas que usarás en la demo (la pantalla lo avisa: **«Recalibra con las pilas que usarás en la demo»**).
2. «Avanzar 4 s»: la app manda el balance al carro (con ACK) y el carro avanza 4 s con su perfil (antes eran 2 s; el ruido de arranque, de ±2 a 3 cm, era mayor que la meta de 1 cm) (AUTO por defecto) y se detiene.
3. Dices **hacia dónde se desvió** (izquierda, derecha o recto) y **cuántos cm** (de 1 en 1) y tocas «Aplicar el desvío».
4. La app calcula el balance nuevo con la geometría de un arco (`cal_balance_nuevo`, en C y probado en PC): ángulo ≈ **2 x desvío / recorrido**; la diferencia entre los dos lados es ese ángulo x
   el **ancho entre las ruedas** (configurable, **12 cm** por defecto) / recorrido; el recorrido sale de la calibración (en AUTO ~39 cm en 2 s; en MANUAL 76 cm con 26.3 ms por cm). Un desvío a la derecha baja el balance; uno a la
   izquierda lo sube (nunca más de 1000). Con 6 cm a la derecha en 38.7 cm: 986 → **~895**. Lo guarda en el teléfono y se lo manda al carro.
5. Repite hasta **1 cm o menos** de desvío (o «recto»), **máximo 4 intentos**. «Restaurar valor por defecto» vuelve al balance del firmware. Con 4 intentos sin llegar, avisa y queda el último balance.

En PC: con un carro que empuja 12 % más del izquierdo el balance que lo endereza es ~893 y la geometría lo encuentra en **una** pasada (desvío 0). Si el carro se desviara a la izquierda con el balance en 1000, no hay
nada más que subir (el derecho ya está en el tope): habría que revisar las ruedas o los motores.

**Simulador.** El carro virtual puede empujar de más del lado izquierdo (`desbalance`) y el balance lo reduce: `desbalance x balance = 1000 x 1000` va recto; si no, se tuerce hacia el lado más rápido. La pantalla Rutas usa
**el mismo balance de AUTO** que el carro y, como desbalance, el que deja recto el balance calibrado (o el típico medido, 1.12, si la recta no está calibrada; entonces avisa). Prueba en PC (`pruebas_sim_ruta.c`): una
recta de 1 m con el factor por defecto (986) termina a 39 cm del punto final; con el calibrado (893) llega (0 mm de error).

**Arranque escalonado** (`ARRANQUE_ESCALONADO_MS` en `config.h`, **0 = apagado** por defecto): si es mayor que 0, al arrancar avanzando o retrocediendo entra primero el motor **izquierdo** y el **derecho** entra esos ms
después, para repartir el pico de corriente (los dos motores a 255 a la vez piden mucha corriente de golpe). Los pivotes no cambian. Prueba de firmware
(`pruebas_firmware`, `make test` compila una segunda vez con `-DARRANQUE_ESCALONADO_MS=40`): primero solo el izquierdo, el derecho a los 40 ms, el ajuste por lado al terminar el arranque a plena potencia, STOP o giro antes de que entre el
segundo lo cancelan, y el watchdog igual. **PENDIENTE HARDWARE:** probar con el carro si ayuda con un valor de 30 a 60 ms; sin probarlo se queda en 0.

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

## Pruebas del balance (PC)

- **Firmware** (`pruebas_firmware`, `make test`): 118 casos y 15 del arranque escalonado, 0 fallos. Entre ellos: valores por defecto, `BAL_AUTO:900` → izquierdo 189 y derecho 210, `BAL_MANUAL:950` → 242 y 255, el
  tope nunca se pasa con ningún valor de 800 a 1000, `NAK_BAL` con 799, 1001, 0, vacío, letras, espacios, signo, decimales y ceros a la izquierda, `BAL?`, no frena ni cambia el comando actual ni el perfil, lo guardado se
  lee al arrancar y lo inválido se ignora, y la comparación aleatoria con el código original (600 000 pasos) sigue sin diferencias.
- **Servidor falso** (`probar_servidor_falso.py`): 26 casos (ACK, NAK, `BAL?`, conservar al reconectar).
- **Núcleo en C** (`pruebas_calibracion.c`, `pruebas_sim_ruta.c`): la geometría de la recta y la simulación con el factor equivocado y con el calibrado.

## Pruebas

- **Firmware en PC** (`pruebas_firmware`, `make test`): 87 casos, 0 fallos, entre ellos los perfiles (ACK, valores 252/255 y 207/210, STOP primero al cambiar
  en movimiento, mismo perfil sin frenar, watchdog y conexión vuelven a AUTO, STOP/EBRAKE/PING no cambian el perfil) y la comparación aleatoria con un
  modelo del firmware (2 × 300 000 pasos, ahora con MODO_MANUAL/MODO_AUTO mezclados en las entradas).
- **Servidor falso** (`probar_servidor_falso.py`): 14 casos, 0 fallos (ACK, idempotencia, STOP por cambio de perfil, watchdog).
- **Teléfono contra el servidor falso** (por `adb reverse tcp:8080`, sin tocar el WiFi del carro): la app manda `MODO_MANUAL` al abrir (antes del primer PING),
  el servidor contesta `ACK_MANUAL`, la pantalla lo confirma y lo repite cada 3.2 s.

## PENDIENTE HARDWARE: balance de la recta (al final, con las pilas de la demo)

1. Cargar el DevKit con el firmware nuevo (`.\scripts\cargar.ps1 nuevo`) y comprobar en el monitor serie que arranca.
2. En la app: Calibración → «Ajuste fino: giro y recta» → paso de la recta. Línea de cinta, «Avanzar 4 s», decir hacia dónde y cuántos cm, repetir hasta 1 cm (máximo 4 intentos), en **AUTO**; si se maneja en MANUAL, también en MANUAL.
3. Anotar el balance final de cada perfil (aparece en la pantalla) y comprobar que al apagar y encender el carro lo conserva (`BAL?` o el siguiente «Avanzar 4 s»).
4. Una ruta recta de 1 m desde Rutas: el desvío debe ser de 1 a 2 cm.

## PENDIENTE HARDWARE (anterior): rectas de 150 cm en los dos perfiles

Cargar el DevKit (`scripts\cargar.ps1 nuevo`). Para cada perfil, 3 pasadas en línea recta de 150 cm sobre el mismo piso, anotando el desvío lateral
en cm al final (+ derecha, − izquierda):

1. **Manual (255):** en la app abre Control (modo Manual) y confirma «Perfil: MANUAL (255) ✓». Mantén **adelante** presionado hasta ~150 cm y suelta.
2. **Auto (210):** pasa a **Semiautónomo**, «Obstáculos: botones», y la ruta de recta (**Prueba de obstáculo (1 m)**: 100 cm; o **Esquivar objeto grande
   (1.4 m)** sin objeto) con «Perfil: AUTO (210) ✓». Anota el desvío.
3. Si el automático se desvía, ajusta `PERFIL_AUTO_VELOCIDAD_IZQ` o `_DER` (baja el lado que empuja más: si se va a la derecha, baja la izquierda) y
   vuelve a cargar. Reporta: desvío manual (3 pasadas) y desvío automático (3 pasadas).

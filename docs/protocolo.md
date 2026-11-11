# Protocolo de comunicación del carro FSI

**Materia:** Fundamentos de los Sistemas de Información (FSI)  
**Canal principal:** WebSocket sobre TCP/IP (`ws://192.168.4.1/ws`)  
**Formato:** Texto plano en codificación ASCII/UTF-8, sin envoltorios JSON ni binarios  
**Fecha de referencia:** 27 de octubre de 2026  

---

## 1. Principios de diseño

1. **Simplicidad de procesamiento:** El ESP32 es un microcontrolador que no debe desperdiciar ciclos ni memoria dinámica parseando estructuras JSON complejas. Los mensajes son palabras clave sencillas delimitadas por retornos de línea o separadores de dos puntos (`:`).
2. **Asincronía total:** A través de un único socket WebSocket bidireccional, la app envía órdenes de movimiento y el carro emite ráfagas de telemetría de sensores sin requerir un esquema de petición-respuesta bloqueante.
3. **Prioridad absoluta para la seguridad:** Los comandos de detención (`STOP` y `EBRAKE`) tienen vía libre en la cola y se ejecutan de manera inmediata, incluso si la cola de mensajes se encuentra saturada.

---

## 2. Comandos de control de movimiento

| Comando enviado | Efecto en motores | Respuesta del carro | Notas de seguridad |
|---|---|---|---|
| `FORWARD` | Avanza en línea recta aplicando el balance de potencia del perfil activo. | Ninguna | Arranca a potencia 255 durante 250 ms para vencer inercia inicial y luego baja al balance. |
| `REVERSE` | Retrocede en línea recta. | Ninguna | Mismo comportamiento de arranque pleno y balance. |
| `LEFT` | Pivote hacia la izquierda (rueda derecha empuja adelante a potencia 255; rueda izquierda parada). | Ninguna | No se aplica escalonamiento en pivotes. |
| `RIGHT` | Pivote hacia la derecha (rueda izquierda empuja adelante a potencia 255; rueda derecha parada). | Ninguna | Giro con un solo lado activo. |
| `STOP` | Apaga el PWM de los cuatro pines de motor (0 %). | Ninguna | Se ejecuta SIEMPRE. Si se envía repetido, limpia cualquier residuo en los registros. |
| `EBRAKE` | Freno de emergencia (idéntico a `STOP`). | Ninguna | Comando de máxima prioridad para botones de pánico. |

---

## 3. Modos de conducción y calibración de balance

El carro cuenta con dos perfiles de velocidad independientes (`firmware_carro/config.h`):

| Comando enviado | Respuesta | Descripción del perfil |
|---|---|---|
| `MODO_AUTO` | `ACK_AUTO` | Perfil para navegación autónoma y pruebas de ruta. Velocidad base 210, motor izquierdo ajustado a 207. Envío activo de telemetría `SENS`. Si el carro estaba en marcha al conmutar, primero se detiene. |
| `MODO_MANUAL` | `ACK_MANUAL` | Perfil de velocidad máxima para conducción manual desde la app (255 derecho / 252 izquierdo medido). No emite ráfagas `SENS` para no saturar el canal de teleoperación. |
| `BAL_AUTO:NNN` | `ACK_BAL_AUTO:NNN` o `NAK_BAL` | Ajusta el factor de balance del perfil AUTO en milésimas (rango permitido: 800 a 1000). Se guarda en memoria no volátil del ESP32. |
| `BAL_MANUAL:NNN` | `ACK_BAL_MANUAL:NNN` o `NAK_BAL` | Ajusta el factor de balance del perfil MANUAL en milésimas (800 a 1000) y lo almacena. |
| `BAL?` | `BAL:AAA:MMM` | Consulta los balances guardados: `AAA` = factor AUTO, `MMM` = factor MANUAL (ej. `BAL:986:988`). |

---

## 4. Giro de precisión en bucle cerrado

Para realizar giros angulares exactos sin depender de aproximaciones de tiempo, el firmware implementa un control cerrado supervisado por el giroscopio MPU6050:

- **Sintaxis:** `GIRAR:<grados>`
- **Rango admitido:** `5` a `200` grados enteros.
- **Convenio de signo:** Grados positivos indican giro a la **izquierda** (sentido antihorario visto desde arriba); grados negativos indican giro a la **derecha**.
- **Respuestas posibles:**
  - `GIRO_OK`: El vehículo completó el giro angular dentro de la tolerancia programada.
  - `GIRO_TIMEOUT`: Expiró el tiempo límite de maniobra antes de alcanzar el ángulo deseado (ej. ruedas patinando o carro atorado).
  - `GIRO_ERROR:<motivo>`: Falla en la ejecución (ej. `GIRO_ERROR:sin_imu` si el giroscopio no está disponible o se encuentra descalibrado).

---

## 4b. Rumbo sostenido al avanzar (J16, `RUMBO`)

Solo en firmware que lo anuncia con `RUMBO=1` en la respuesta a `CAPS?`. La app nunca manda `RUMBO` a un firmware que no lo anunció: uno anterior guardaría el texto como comando actual y dejaría de reconocer el `FORWARD` en curso (y el freno del láser, que solo actúa «avanzando»).

| Mensaje | Respuesta | Efecto |
|---|---|---|
| `RUMBO:ON` | `ACK_RUMBO:1` | Enciende el lazo. Vale desde el siguiente `FORWARD`. Al arrancar el firmware y al conectarse o desconectarse un cliente queda apagado. |
| `RUMBO:OFF` | `ACK_RUMBO:0` | Apaga el lazo; si el carro avanza, vuelve en el acto a las potencias fijas del perfil (exactamente lo de antes). |
| `RUMBO:<n>` | (ninguna) | Objetivo del avance en curso, en décimas de grado con la convención del giroscopio (+ izquierda, el mismo ángulo de `SENS`). Se recorta a ±20° del ángulo con que empezó el avance. Sin avance en curso se ignora. |
| `RUMBO?` | `RUMBO:<encendido>,<en marcha>,<objetivo x10>,<corrección milésimas>,<aprendido milésimas>` | Diagnóstico. |

Funcionamiento: con el lazo encendido, al empezar cada `FORWARD` el objetivo es el ángulo de ese momento; en cada muestra del giroscopio (100 Hz) un control proporcional-integral reparte la potencia entre los lados (`RUMBO_KP_MIL_GRADO` = 10 milésimas por grado, `RUMBO_TI_MS` = 1500 ms, tope ±15 %), también durante el arranque a plena potencia (ahí solo baja un lado). Con el giroscopio no listo o un error de más de 30° no corrige. Lo aprendido (el desbalance de los lados) pasa al siguiente avance; vive en RAM. `RUMBO` no es un movimiento: no renueva el watchdog ni cambia el comando actual; `REVERSE`, `LEFT`, `RIGHT` y `GIRAR` no cambian. Ganancias elegidas en simulación: NO MEDIDAS en el carro.

---

## 5. Telemetría periódica y eventos de seguridad

### 5.1 Trama de telemetría `SENS`

En perfil `AUTO`, si hay un cliente WebSocket enlazado, el ESP32 transmite periódicamente cada **100 ms** un mensaje de estado compuesto por tres campos:

```text
SENS:<angulo_x10>,<distancia_mm>,<banderas>
```

Ejemplo: `SENS:450,320,7`

- **Campo 1 (`angulo_x10`):** Ángulo actual acumulado por el giroscopio multiplicado por 10. En el ejemplo, `450` representa `45.0` grados.
- **Campo 2 (`distancia_mm`):** Lectura milimétrica del sensor de distancia láser VL53L0X. En el ejemplo, `320` representa 320 mm (32 cm). Si no hay objeto en rango, reporta un valor fuera de rango.
- **Campo 3 (`banderas`):** Entero decimal que representa una máscara de bits con el estado del subsistema:
  - **Bit 0 (valor 1):** MPU6050 inicializado y en estado operativo (`IMU_LISTO`).
  - **Bit 1 (valor 2):** Sensor láser VL53L0X vivo y respondiendo transacciones I2C.
  - **Bit 2 (valor 4):** Medición de distancia válida y dentro de rango óptico confiable.
  - **Bit 4 (valor 16):** Maniobra de giro en curso (`GIRO_ACTIVO`).

*Nota:* Un valor de banderas `7` (1 + 2 + 4) indica salud perfecta: IMU lista, láser respondiendo y distancia válida.

### 5.2 Notificación de parada de emergencia ToF

Cuando el sensor láser detecta un obstáculo a una distancia inferior al umbral crítico mientras el vehículo avanza, el firmware detiene los motores de manera autónoma y difunde por el WebSocket:

```text
TOF_PARO:<distancia_mm>
```

**Nota J16 (app):** al recibir `TOF_PARO` la app deja de repetir el `FORWARD` (antes el siguiente heartbeat lo volvía a arrancar contra el obstáculo). Si una ruta sigue pidiendo avanzar después de un `TOF_PARO` (el navegador no lo esperaba), la ruta se detiene con un mensaje.

### 5.3 Aviso del watchdog (J16)

Cuando el watchdog de 1 s frena el carro porque no le llegaron comandos, el firmware J16 avisa por el WebSocket (y por el puerto serie):

```text
WD:<frenos desde el encendido>
```

Una ruta en marcha se detiene al recibirlo (el navegador seguiría contando un avance que no ocurrió). Si el enlace está muerto el aviso no llega: para eso la app detiene la ruta cuando un firmware que contestó `CAPS` pasa 1.5 s sin mandar `SENS`. Un firmware anterior no manda `WD`; una app anterior lo ignora. Al terminar una ruta con `RUMBO` la app pregunta `RUMBO?` y deja la respuesta en el registro `SENS_DIAG`.

---

## 6. Monitoreo de latencia y Watchdog

- **Comando `PING`:** La aplicación Android envía periódicamente `PING` para estimar el retardo de la red inalámbrica.
- **Respuesta `PONG`:** El ESP32 responde inmediatamente `PONG`.
- **Nota J16:** el watchdog lo renueva cualquier texto que no sea `PING`, una consulta de sensores o `RUMBO` (también `MODO_*`, `BAL_*` y texto desconocido); las pruebas del firmware fijan ese comportamiento. `STOP` y el fin de un giro dejan las ruedas libres (PWM 0 en las dos entradas del TC1508A), no frenan activamente.
- **Regla crítica del Watchdog:** El comando `PING` **NO renueva el temporizador del watchdog**. Para que el carro mantenga los motores encendidos durante un avance manual, la app debe repetir el comando `FORWARD` cada 250 ms. Si pasan **1000 ms** sin recibir un comando de movimiento real, el vigilante apaga los motores automáticamente.

---

## 7. Consultas de diagnóstico y sensores

| Comando | Formato de respuesta | Descripción |
|---|---|---|
| `CAPS?` | `CAPS:IMU=<0\|1>,TOF=<0\|1>,TOFR=<n>,IMUR=<n>,IMUM=<motivo>,IMUE=<estado>,RUMBO=1,IMUS=<ppm>,FW:<hash>-<fecha>` | Qué sensores funcionan, reintentos y motivo, estado del giroscopio y versión. `RUMBO=1` (J16) dice que el firmware entiende `RUMBO`; `IMUS=<ppm>` (J16) que entiende `IMU_ESCALA` y la corrección en uso. Un firmware anterior no los incluye. Se vuelve a anunciar solo cuando cambia. |
| `IMU?` | `IMU:angulo_x10,sesgo_x100,estado,who_am_i,motivo` | Diagnóstico del giroscopio (corregido en J16: este es el orden real de `sensores.c`). Estado 2 = listo; la app (J16) solo pasa el ángulo al navegador con estado 2. |
| `TOF?` | `TOF:distancia_mm,valida` | Diagnóstico de distancia cruda del láser. |
| `IMU_CAL` | `ACK_IMU_CAL` o `NAK_IMU_CAL` | Fuerza recalibración del sesgo del giroscopio (solo admitido con motores parados por >300 ms). |
| `IMU_CERO` | `ACK_IMU_CERO` | Establece el ángulo actual del giroscopio como nueva referencia de 0 grados. |
| `IMU_ESCALA:<ppm>` | `ACK_IMU_ESCALA:<ppm>` o `NAK_IMU_ESCALA` | J16: corrección de escala del giroscopio, de -50000 a 50000 partes por millón (signo opcional y hasta 6 cifras). Solo con el carro detenido y sin giro en curso. Se guarda en la memoria no volátil y la usan el giro por ángulo, `SENS` y `RUMBO`. La mide la pantalla Sensores. No renueva el watchdog. |
| `IMU_ESCALA?` | `IMU_ESCALA:<ppm>` | J16: la corrección de escala en uso (0 = sin corregir). |
| `TOF_USO:<0\|1>` | `ACK_TOF_USO:<0\|1>` | Activa (`1`) o desactiva temporalmente (`0`) la autoridad de parada de emergencia del láser ToF. |
| `LOOP?` | `LOOP:peor_us,excesos` | Reporta el peor tiempo de ejecución de una vuelta del bucle `loop()` en microsegundos. |
| `ARR?` | `ARR:reintentos,motivo` | Registro de incidencias ocurridas durante la inicialización de periféricos. |

---

## 8. Gestión y descarte en la cola de comandos

El firmware dispone de un buffer circular de 32 ranuras (`COLA_COMANDOS_MAX`). Si una ráfaga saturara la cola:
1. Se descartan prioritariamente mensajes `PING` antiguos no despachados.
2. Si no hay `PING`, se descarta el comando más viejo que no sea `STOP`, `EBRAKE` ni `__STOP_AUTO` (corregido en J16: puede ser un `FORWARD` o un `GIRAR`, no solo una consulta; ver `carro.c`).
3. **Bajo ninguna circunstancia se descarta un comando `STOP` o `EBRAKE`**.

---

## 9. Referencias visuales

- [`docs/figuras/fig_03_flujo_comando.png`](file:///e:/Dev/fsi_proyecto/docs/figuras/fig_03_flujo_comando.png): Recorrido de los mensajes desde la app hasta el hardware.
- [`docs/figuras/fig_11_protocolo.png`](file:///e:/Dev/fsi_proyecto/docs/figuras/fig_11_protocolo.png): Secuencia temporal y periodicidad de mensajes WebSocket.

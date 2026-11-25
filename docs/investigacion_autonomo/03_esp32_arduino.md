# 03 — ESP32 y core Arduino-ESP32 3.x: WiFi, WebSocket asíncrono, FreeRTOS, watchdogs, I2C, PWM y alimentación

Investigación del 16-nov-2026 (fecha de consulta de todas las fuentes: **2026-11-16**). Rama `feat/autonomo-v3`.
Convención de confianza: **alta** = hoja de datos, documentación o código fuente del fabricante/autor de la librería, RFC; **media** = tutorial o varias fuentes secundarias coherentes; **baja** = README de aficionado, patente, extracto; **INFERENCIA** = deducción mía (aritmética o lectura del código), no dicha por la fuente. Lo no medido en el carro es **NO MEDIDO** o **SIMULADO**. Lo **MEDIDO (proyecto)** viene de `docs/validacion_sensores.md`, `docs/latencia.md` y `docs/latencia_modos.md`, no de esta investigación. No se conectó con el carro.

**Cómo se leyó cada fuente (transparencia).** Versiones reales del proyecto (`docs/referencia/compilacion.md`): core Arduino-ESP32 **3.3.12** (basado en ESP-IDF **v5.5.5**, según la ficha de la versión en GitHub), AsyncTCP **3.5.0**, ESPAsyncWebServer **3.12.0**. Se leyó el **código fuente** de esas etiquetas exactas (no una versión "parecida") y el `sdkconfig` de las librerías precompiladas del propio equipo (`esp32-libs/3.3.12/sdkconfig`), que es la configuración con la que se compila el carro. La hoja de datos del ESP32 v5.3 se leyó en PDF. Las páginas de ESP-IDF v5.5.5 (watchdogs, LEDC, WiFi, FreeRTOS, flash) se leyeron en `docs.espressif.com`. No se consultó el Manual de Referencia Técnica (TRM) ni la hoja del TC1508A (ver 04_motores_cinematica.md).

---

## 1. Cómo funciona

### 1.1 Reparto de tareas y núcleos (código del core + sdkconfig real)

- `loop()` corre en la tarea `loopTask`: núcleo **1** (`ARDUINO_RUNNING_CORE=1`), prioridad **1**, pila **8192 B** [3: `cores/esp32/main.cpp`; 6] (alta). Los eventos WiFi del core van en el núcleo 1 (`ARDUINO_EVENT_RUNNING_CORE=1`) [6].
- La tarea del controlador WiFi está fijada al **núcleo 0**; la tarea de LwIP (`tcpip_thread`) tiene prioridad 18, afinidad CPU0 y *core locking* activo [6] (alta). Con core locking, las llamadas a la pila TCP desde otras tareas (`tcpip_api_call`) toman un mutex de la pila y no envían un mensaje a la tarea [2: Kconfig `CONFIG_LWIP_TCPIP_CORE_LOCKING`; 4 código de `AsyncTCP.cpp`] (alta para el mecanismo; el efecto sobre `loop()` es INFERENCIA, ver 3.1).
- **AsyncTCP 3.5.0** crea **una** tarea `async_tcp`: prioridad 10, pila 16 KB, sin afinidad fija (cualquiera de los dos núcleos), cola de eventos de 64, se suscribe al watchdog; los eventos de sondeo (`poll`) se agrupan y los envíos pasan por `tcpip_api_call` [4] (alta: código leído). Como su prioridad (10) es mayor que la de `loopTask` (1), si cae en el núcleo 1 **interrumpe a `loop()`** mientras procesa un evento (INFERENCIA por prioridades).
- La callback del WebSocket del proyecto corre en `async_tcp`, no en `loop()`; solo encola comandos bajo sección crítica y contesta PING/ACK con `client->text` [P: `firmware_carro.ino`]. `loop()` es el único que escribe pines de motor (INFERENCIA por lectura; `ledcWrite` no es seguro entre tareas sobre el mismo canal [2: doc LEDC] — se cumple hoy).
- `loopTask` no se duerme: `loop()` se reentra sin `delay` (el diseño del proyecto es *busy loop*), así que la tarea inactiva del núcleo 1 (IDLE1) **nunca corre**; por eso el TWDT de ESP-IDF **no** vigila IDLE1 por defecto (`CHECK_CPU1` desactivado) y el firmware se suscribe él mismo con `esp_task_wdt_add` (`loopTaskWDTEnabled=false` en el core) [3; 6; P] (alta + lectura del código).

### 1.2 Watchdogs

| Watchdog | Config. real | Qué hace |
|---|---|---|
| **TWDT** (tarea) | 5 s, pánico (reinicio) al vencer; vigila IDLE0; **no** IDLE1 [6] | El firmware llama `esp_task_wdt_reconfigure` con `idle_core_mask=0` (quita las tareas inactivas) y `trigger_panic=true`, 5000 ms (`WATCHDOG_HW_MS`) y suscribe `loopTask` y la tarea de AsyncTCP [P; 2: `task_wdt.c` leído] (alta) |
| **IWDT** (interrupciones) | 300 ms, vigila ambos núcleos [6] | Salta si las interrupciones están bloqueadas ≥300 ms en un núcleo (p. ej. una escritura a flash mal hecha) [2: doc WDT] |
| **WDT del bootloader** | 9000 ms [6] | Cubre solo el arranque |
| **Watchdog de comunicación (proyecto)** | `TIEMPO_WATCHDOG_MS` 1000 ms [P: `config.h`, `carro.c`] | Detiene motores si no llega un `FORWARD`/`BACKWARD`/... repetido; **no se renueva** con PING ni con texto desconocido (alta: lectura del código) |

### 1.3 WiFi (modo punto de acceso, AP)

- El carro es un **SoftAP** sin STA: `WiFi.softAP(SSID, clave, canal 1)`; `max_connection` por defecto **4** (rango 1–4) [3: `WiFiAP.cpp/.h`; 2: doc WiFi API] (alta). Intervalo de baliza por defecto 100 TU (102.4 ms); tiempo de inactividad de estación 300 s por defecto [2: doc WiFi API] (alta).
- **El ahorro de energía (*modem sleep*) es solo de estación**: el AP no entra en reposo; solo guarda en memoria las tramas unicast dirigidas a estaciones que sí duermen [2: doc WiFi driver, "power save"] (alta). En el core 3.3.12, `WiFi.setSleep()` con STA no iniciada solo guarda el valor [3: `WiFiGeneric.cpp`]. **Consecuencia:** `WiFi.setSleep(false)` en el firmware del carro (AP puro) es **inocuo y sin efecto** (INFERENCIA por lectura del código del core). El ahorro que sí afecta la latencia está en las estaciones (teléfono y cámara), no en el ESP32 del carro.
- `setTxPower` usa unidades de 0.25 dBm: `WIFI_POWER_13dBm` = 52 [3] (alta); el máximo del PHY compilado es 20 dBm (`CONFIG_ESP_PHY_MAX_WIFI_TX_POWER=20`) [6].
- La cámara (192.168.4.2) es solo otra **estación** que sirve vídeo al teléfono; en `camara_prueba` no hay referencia al WebSocket ni a 192.168.4.1 (grep, MEDIDO proyecto: lectura de código). El vídeo pasa por la radio del AP (el teléfono y la cámara están ambos en el mismo AP): el aire se usa **dos veces** por cada cuadro (cámara → AP → teléfono) [INFERENCIA: topología de un AP infraestructura; no verifiqué si ESP32 reenvía entre estaciones en capa 2].
- El teléfono: `WifiManager.WIFI_MODE_FULL_LOW_LATENCY` existe en Android para "dar prioridad a lograr baja latencia" [10] (alta pero solo la tabla de resumen se leyó, no la nota completa). La app (OkHttp, `Enlace.kt`) **no usa ningún `WifiLock`** (grep, lectura del código).

### 1.4 WebSocket asíncrono (ESPAsyncWebServer 3.12.0 + AsyncTCP 3.5.0)

- Cola por cliente `WS_MAX_QUEUED_MESSAGES = 32`; con `_closeWhenFull = false` (valor por defecto) **los mensajes nuevos se descartan** cuando la cola está llena [5: `AsyncWebSocket.h/.cpp`] (alta). `cleanupClients(max)` cierra el cliente más viejo si hay más de `max` (4 en ESP32 por defecto).
- AsyncTCP cierra una conexión si un ACK pendiente no llega en `MAX_ACK_TIME = 5000 ms` [4] (alta). El stack LwIP usa `TCP_RTO` inicial de 3000 ms y hasta 12 retransmisiones (`CONFIG_LWIP_TCP_RTO_TIME=3000`, `MAXRTX=12`) [6] — mucho mayor que lo que necesita un control a 100 ms.
- `setNoDelay(true)` (el firmware lo activa) desactiva Nagle: los mensajes cortos salen al instante en vez de esperar a juntarse [P] (INFERENCIA por el significado estándar de Nagle).
- Todas las escrituras (`ws.text`, `client->text`) toman el candado del núcleo de LwIP; llamadas desde `loop()` **pueden bloquear** mientras `tcpip_thread` o la tarea de WiFi (núcleo 0) usan ese candado (INFERENCIA por el mecanismo de [4] y [6]; no medido por fase).

### 1.5 I2C (core 3.3.12, API "ng")

- `Wire` usa el controlador `i2c_master` de IDF; tiempo de espera por defecto **50 ms**; el proyecto lo fija en **5 ms** (`I2C_TIMEOUT_MS`) [3: `Wire.cpp`, `esp32-hal-i2c-ng.c`] (alta). El filtro de glitches está a 7 ciclos (`glitch_ignore_cnt=7`), `scl_wait_us=0` (el valor por defecto de IDF es 2000 µs en ESP32) y las resistencias internas pull-up (≈45 kΩ, [1]) se activan [3] (alta para el código; que sean **insuficientes** a 400 kHz es INFERENCIA, ver 2).
- **`endTransmission(false)` no toca el bus en este core**: la secuencia combinada escritura+lectura ocurre en `requestFrom` (`i2cWriteReadNonStop`); la rama "devuelve 1" que maneja el firmware en `hw_i2c_leer` es código muerto [3: `Wire.cpp`] (alta: código leído).
- Tras un tiempo agotado o bus ocupado, la siguiente transacción de `i2c_master.c` (IDF 5.5.5) reinicia la máquina de estados del hardware y hace una **limpieza de bus por software**: hasta 9 pulsos de SCL (≈100 kHz) y un STOP, porque el ESP32 no tiene esa función en hardware [2: `i2c_master.c`] (alta; no probado en el carro). El FIFO del I2C del ESP32 es de 32 B.
- Una llamada puede esperar **varias veces** el tiempo agotado (candado del bus, semáforo, cola de eventos): una llamada bloqueante puede tardar ≥5 ms y, en el peor caso, ~2× (INFERENCIA por lectura del código).

### 1.6 PWM (LEDC) y UART

- `ledcAttach(pin, frecuencia, resolución)` y `ledcWrite(pin, duty)`: la escritura hace `set_duty` + `update_duty`; un valor ≥255 a 8 bits se interpreta como 256 (encendido pleno) [3: `esp32-hal-ledc.c`] (alta). A 1500 Hz el máximo de bits que cabe en el temporizador con reloj APB de 80 MHz es ≈15 (cálculo mío: log2(80e6/1500)); la doc pone 13 bits a 5 kHz [2: doc LEDC] (alta la doc; el cálculo es INFERENCIA). **1500 Hz a 8 bits está bien dentro de lo permitido.**
- `HardwareSerial` tiene búfer de transmisión **0** por defecto: `uart_write_bytes` **bloquea** hasta empujar los bytes al FIFO [2: `uart.h` v5.5.5; 3: `HardwareSerial.cpp`] (alta). A 115200 baudios un byte tarda 86.8 µs → un `Serial.println` de 60 caracteres bloquea ≈5.2 ms (INFERENCIA aritmética; el FIFO de 128 B del UART absorbe parte).

### 1.7 Flash y NVS

Durante una escritura o borrado de flash (NVS incluido) las demás tareas y las interrupciones no ubicadas en IRAM quedan **suspendidas** en ese núcleo/ambos según el caso [2: doc `spi_flash_concurrency`] (alta el hecho; la duración **no** la da la doc, NO MEDIDO). El proyecto escribe NVS en `BAL_*`, `IMU_ESCALA`, `ANT_BORRAR` y al guardar anticipación tras un giro [P: `carro.c`]. Un borrado de sector del flash suele ser lo más lento (la doc no da cifra; no la invento).

### 1.8 Alimentación y supervisión de tensión

- Detector de apagón (*brownout*) activo; nivel `LVL_SEL_0` = **2.43 V ± 0.05** (referencia Kconfig de IDF) [2; 6] (alta). Rango de alimentación del chip 2.3–3.6 V; la hoja recomienda una fuente de **≥500 mA** [1 Tabla 5-1, Tabla 5-4] (alta).
- Consumo (hoja v5.3, a 3.3 V): Tx 802.11b +19.5 dBm = **240 mA**; 11g 54 Mbps +16 dBm = **190 mA**; 11n MCS7 +14 dBm = **180 mA**; Rx ≈ **95–100 mA** [1 Tabla 5-3/5-4] (alta).
- GPIO2 es un pin de arranque (*strapping*): forma parte de la selección del modo de arranque junto con GPIO0; tiene pull-down interno por defecto (IO_MUX) [1 §3 y pág. 69] (alta para el hecho; el estado reset por defecto de GPIO25/26/27/33 (oe=0, ie=0) se leyó en una imagen pequeña: **media, verificar**).

---

## 2. Parámetros y límites relevantes

### 2.1 Cifras de las fuentes

| Tema | Cifra | Fuente |
|---|---|---|
| Prioridad/pila `loopTask` | 1 / 8192 B, núcleo 1 | [3; 6] |
| TWDT | 5 s, pánico, IDLE0 sí, IDLE1 no | [6] |
| IWDT | 300 ms, ambos núcleos | [6] |
| WDT bootloader | 9000 ms | [6] |
| FreeRTOS tick | 1000 Hz (1 ms) | [6] |
| AsyncTCP | 1 tarea, prio. 10, 16 KB, cola 64 | [4] |
| Cola WS por cliente | 32 msg.; si se llena, descarta nuevos | [5] |
| ACK máx. | 5000 ms, luego cierra | [4] |
| `max_connection` softAP | 4 (1–4) | [2; 3] |
| Baliza | 100 TU | [2] |
| Wire timeout | 50 ms (por defecto) | [3] |
| Tx ESP32 | 240 mA (11b, 19.5 dBm); 190 mA (11g); 180 mA (11n) | [1] |
| Rx ESP32 | 95–100 mA | [1] |
| Brownout | 2.43 V ± 0.05 | [2; 6] |
| Fuente recomendada | ≥ 500 mA | [1] |
| I2C Fast-mode | 400 kHz máx., tr/tf ≤ 300 ns, Cb < 400 pF | [8 UM10204] y [MPU6050 PS p.18] |
| Resistencias internas | ≈45 kΩ | [1] |
| RTO mínimo TCP | "SHOULD" 1 s; inicial 1 s; se duplica | [7 RFC 6298] |
| Powerbank (cota de corte) | ≈50–200 mA según modelo (**media/baja**: README y blog) | [9] |

### 2.2 Valor usado en el proyecto frente a lo que dice la fuente

| Parámetro | Proyecto | Fuente | Veredicto |
|---|---|---|---|
| `LOOP_MAX_US` | 5000 µs (5 ms) | `loop()` MEDIDO (proyecto): 1.7–1.8 ms sin cliente, **6.8–8.4 ms con la app**, pico de arranque 12.9 ms | **Incumplido** con la app conectada (ver 6) |
| `WATCHDOG_HW_MS` | 5000 ms, pánico | TWDT defecto 5 s [6] | Coherente; el reinicio ocurre tras ≥5 s sin `esp_task_wdt_reset` |
| `TIEMPO_WATCHDOG_MS` | 1000 ms | RTO de TCP 1–3 s [6, 7] | 1 s es menor que el RTO inicial: una sola pérdida de paquete ya puede agotarlo (INFERENCIA) |
| `COLA_COMANDOS_MAX` | 32 × 32 B, FIFO, STOP protegido pero sin prioridad | — | Ver 6 (STOP y la cola) |
| `COLA_SALIDA_MAX` | 16 (se descarta el más viejo) | cola WS 32 [5] | Coherente: nunca llena la de la librería por sí sola (INFERENCIA) |
| `SENS_PERIODO_MS` | 100 ms con cliente (perfil AUTO) | cola WS 32 | Con el cliente detenido, 32 mensajes se llenan en 3.2 s (INFERENCIA aritmética) |
| `WIFI_POTENCIA_TX` | 13 dBm | datos de consumo a 14–19.5 dBm [1] | Menos corriente que el pico de la hoja (INFERENCIA); alcance NO MEDIDO |
| `WIFI_CANAL` | 1 | — | Sin fuentes que comparar; canal fijo evita saltos (INFERENCIA). Escaneo de interferencia NO MEDIDO |
| `WiFi.setSleep(false)` | activado | solo STA [2; 3] | **Sin efecto** en AP puro |
| `CPU_MHZ` | 160 | hoja: 80/160/240 MHz | Válido |
| `PWM_FRECUENCIA_HZ` / resolución | 1500 Hz / 8 bits | LEDC permite ≈15 bits a 1.5 kHz (cálculo) | Correcto |
| `I2C_FRECUENCIA_HZ` | 400 kHz | Fast-mode máx. 400 kHz | En el límite, con pull-ups internos de 45 kΩ (INFERENCIA: 45 kΩ con Cb≈100 pF da τ=4.5 µs, mucho mayor que los 300 ns de tr máx.; sin pull-ups externos la señal puede no cumplir; **NO MEDIDO**; el módulo GY-521/GY-VL53 suele traer 4.7 kΩ — no lo verifiqué en el carro) |
| `I2C_TIMEOUT_MS` | 5 ms | por defecto 50 ms; lectura nominal ≈0.12–0.4 ms | Ver 3 y 6: un timeout agota `LOOP_MAX_US` |
| Pin keep-alive | GPIO 2 pulso 50 ms cada 2000 ms | pin *strapping*; una GPIO entrega ≤40 mA | Ver 3 y 6 |

---

## 3. Errores típicos y modos de falla

1. **`loop()` bloqueado por la propia pila de red.** `ws.text(...)`, `ws.cleanupClients()` y `client->text(...)` toman mutex de AsyncWebSocket y el candado del núcleo de LwIP; si la tarea de WiFi (núcleo 0) o `tcpip_thread` los retienen, `loop()` espera (INFERENCIA; consistente con el MEDIDO: 1.7 ms sin cliente → 6.8–8.4 ms con la app en las mismas condiciones de reposo, con 2–4 excesos de 5 ms por minuto).
2. **Serial bloqueante.** Con búfer TX = 0, cada `Serial.println` desde `loop()` espera la salida del UART (86.8 µs/byte a 115200). Un diagnóstico verboso puede por sí solo pasar de 5 ms (INFERENCIA).
3. **Escrituras NVS en `loop()`.** Suspenden otras tareas durante el borrado/escritura de flash [2]; duración NO MEDIDA. Puede coincidir con `BAL_*` o con el guardado de `ANT` tras un giro, momento en que el carro está parado (el riesgo es menor) pero también donde el PI de rumbo y el watchdog de 1 s siguen corriendo (INFERENCIA).
4. **Bloqueos I2C (síncronos).** Todas las llamadas `hw_i2c_*` son síncronas en `loop()`: lectura del giroscopio cada 10 ms, sondeo del láser cada 4 ms, lectura del láser cada 33 ms, sondeo de reconexión (`hw_i2c_presente`) y diagnósticos. Una lectura sana cuesta 0.12–0.4 ms; **una sola falla cuesta hasta 5 ms (timeout) y en el peor caso ~10 ms** (INFERENCIA; ver 1.5). `imu_iniciar` (≈130 ms) y `hw_tof_iniciar` (decenas de ms) son bloqueantes: solo deben ejecutarse con el carro parado (el firmware lo hace así para el láser; `imu_recalibrar` llama a `imu_iniciar`).
5. **Cola de WebSocket llena (cliente atascado).** Con la cola de 32 llena, los mensajes nuevos —incluidas las respuestas— se **descartan** en silencio [5]; `SENS` cada 100 ms llena esa cola en ≈3.2 s. Tras 5 s sin ACK, AsyncTCP cierra la conexión [4] → el firmware ve `WS_EVT_DISCONNECT` y activa `__STOP_AUTO`. Es el comportamiento correcto, pero tarda hasta 5 s: **el freno real en ese lapso es el watchdog de comunicación de 1 s**.
6. **Cliente nuevo expulsa al teléfono.** `WS_EVT_CONNECT` cierra los demás clientes: cualquier segundo dispositivo (p. ej. otra pestaña o la cámara si se conectara) desconecta al teléfono y dispara STOP (lectura del código [P]).
7. **Pérdida de paquetes y bloqueo de cabeza de línea de TCP.** TCP entrega en orden: un `STOP` enviado tras un mensaje perdido espera a la retransmisión; el RTO inicial de TCP es 1 s según RFC 6298 (mínimo 1 s *SHOULD*, se duplica) y el stack del ESP32 está configurado a 3 s [7; 6]. Es decir, **bajo pérdida, un STOP del teléfono puede tardar más de 1 s en llegar aunque se haya enviado al instante**; el watchdog de 1 s es la red de seguridad (INFERENCIA; el valor real del stack Android/Linux (≈200 ms mínimo) **NO CONSULTADO**).
8. **Reinicio por TWDT en la mitad de un movimiento.** Si `loop()` no llama `esp_task_wdt_reset` en 5 s se reinicia el chip. Durante el reinicio los pines de motor pueden quedar **flotantes** (IO_MUX: GPIO25/26/27/33 con oe=0 e ie=0 al resetear, imagen pequeña de [1] pág. 69, **verificar**) y la entrada del TC1508A depende de sus resistencias internas, **no verificadas** (hoja no leída): un motor podría quedar impulsado hasta que el firmware reasigne los pines (≈1–2 s de arranque). Prueba segura: ver 6.
9. **Brownout y powerbanks.** El arranque con la powerbank dejó el giroscopio en `MAL` (MEDIDO, proyecto, `validacion_sensores.md`): coherente con una caída transitoria del riel de 3.3 V durante el arranque o con un esclavo I2C a medio comando (INFERENCIA). Las powerbanks se apagan solas por debajo de una corriente mínima (≈50–200 mA según el modelo; **media/baja**); el ESP32 en AP puro con carga baja consume cerca de ese umbral (Rx ≈95–100 mA), de ahí el pulso de keep-alive.
10. **Pulso de keep-alive desde GPIO2.** Una GPIO del ESP32 no entrega la corriente que piden las powerbanks (≈100 mA mínimo en el README consultado); hace falta un transistor y una resistencia de carga [9] (baja/media). GPIO2 es pin de arranque: una carga externa mal diseñada puede cambiar el modo de arranque (hoja [1] §3). **NO MEDIDO** en el carro.
11. **Muestras perdidas por `loop()` lento.** Un exceso de `LOOP_MAX_US` retrasa también la lectura del IMU (omite muestras) y el PI de rumbo (`RUMBO_DT_MAX_MS`=50 recorta el hueco): ver 02_mpu6050.md.
12. **Clon del chip o módulo de baja calidad.** No consultado; fuera de alcance.

---

## 4. Buenas prácticas reconocidas

- **Suscribir al TWDT solo las tareas críticas y alimentarlo desde un único punto** [2: doc WDT] (alta). Hecho.
- **Evitar trabajo largo en callbacks de la librería asíncrona**: ejecutarse en la tarea `async_tcp`, que se suscribe al TWDT, y un bloqueo ahí lo hace vencer; el patrón recomendado por las propias librerías es *encolar y salir* [4; 5] (alta, código) — **ya hecho** (la callback solo encola).
- **No tocar el hardware (LEDC, GPIO) desde dos tareas** sin protección [2: doc LEDC] (alta). Cumplido: solo `loop()` escribe los pines.
- **Dejar el bus I2C con tiempos de espera finitos y recuperación**: nueve pulsos de reloj + STOP, y si no libera, reinicio/ciclo de alimentación del esclavo [8 §Bus clear] (alta). El controlador de IDF 5.5.5 ya incluye una limpieza automática [2].
- **Pull-ups externos de valor adecuado en I2C** (2.2–4.7 kΩ típico a 400 kHz; calculable a partir de tr ≤ 300 ns y Cb) en lugar de depender de las internas de 45 kΩ [8; 1] (alta la teoría; el valor exacto es INFERENCIA).
- **Alimentar el ESP32 con ≥500 mA y condensadores de desacople** [1] (alta); no depender de la regulación del puerto USB cuando los motores arrancan.
- **Registrar la causa del último reinicio** (`esp_reset_reason()`: TWDT, brownout, pánico, etc.) y mostrarla por `CAPS?`/`LOOP?` para distinguir brownout de watchdog en campo (INFERENCIA; la API existe en ESP-IDF [2], no revisé su doc).
- **No usar `Serial` en la ruta crítica** o reservar un búfer de TX (`Serial.setTxBufferSize`) suficiente; limitar el tamaño de los mensajes (INFERENCIA a partir de 1.6).
- **WifiLock en el teléfono** (`WIFI_MODE_FULL_LOW_LATENCY`) mientras la app de conducción esté en primer plano [10] (alta la existencia; efecto NO MEDIDO).

---

## 5. Fuentes

Fecha de consulta de todas: **2026-11-16**.

1. Espressif Systems, *ESP32 Series Datasheet*, v5.3 (2025–2026), `https://documentation.espressif.com/esp32_datasheet_en.pdf`. Tablas 5-1/5-3/5-4 (consumo, alimentación), §3 (pines de arranque), pág. 69 (IO_MUX, imagen pequeña). **Confianza: alta** (con la salvedad anotada para el IO_MUX: media).
2. Espressif Systems, ESP-IDF **v5.5.5**: (a) código leído de `raw.githubusercontent.com/espressif/esp-idf/v5.5.5/…`: `components/esp_driver_i2c/i2c_master.c`, `components/hal/esp32/include/hal/i2c_ll.h`, `components/soc/esp32/include/soc/soc_caps.h`, `components/esp_system/task_wdt/task_wdt.c`, `components/esp_driver_uart/include/driver/uart.h`; (b) documentación en `docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32/`: *Watchdogs* (api-reference/system/wdts), *LEDC*, *ESP-WiFi API / WiFi driver* (power save), *I2C*, *FreeRTOS (IDF)*, *SPI flash and concurrency* (spi_flash_concurrency), referencia Kconfig. **Confianza: alta.**
3. Espressif, Arduino-ESP32 **3.3.12** (release basada en ESP-IDF v5.5.5, GitHub API): `cores/esp32/main.cpp`, `cores/esp32/HardwareSerial.cpp`, `esp32-hal-uart.c`, `esp32-hal-i2c-ng.c`, `esp32-hal-ledc.c`, `libraries/Wire/src/Wire.cpp`, `libraries/WiFi/src/WiFiAP.cpp/.h`, `WiFiGeneric.cpp/.h`, leídos de `raw.githubusercontent.com/espressif/arduino-esp32/3.3.12/…`. **Confianza: alta.**
4. ESP32Async, **AsyncTCP v3.5.0**, `https://github.com/ESP32Async/AsyncTCP` (`src/AsyncTCP.cpp/.h`). **Confianza: alta** (código fuente).
5. ESP32Async, **ESPAsyncWebServer v3.12.0**, `https://github.com/ESP32Async/ESPAsyncWebServer` (`AsyncWebSocket.h/.cpp`). **Confianza: alta** (código fuente).
6. Espressif, `sdkconfig` de `esp32-libs/3.3.12` instalado localmente (`…\Arduino15\packages\esp32\tools\esp32-libs\3.3.12\sdkconfig`): configuración real de compilación del carro. **Confianza: alta.**
7. V. Paxson, M. Allman, J. Chu, M. Sargent, *RFC 6298: Computing TCP's Retransmission Timer*, IETF, jun-2011, `https://www.rfc-editor.org/rfc/rfc6298`. **Confianza: alta.**
8. NXP Semiconductors, *UM10204 I2C-bus specification and user manual*, Rev. 7.0, 1-oct-2021, `https://www.nxp.com/docs/en/user-guide/UM10204.pdf`, "Bus clear" (p. 19) y §3.1.9. **Confianza: alta.**
9. Staubgeborener, *ESP32_KeepPowerbankAlive* (README), GitHub; y voussoir, *Auto-off power banks* (`voussoir.net/writing/auto_off_power_banks`, sin cifras). **Confianza: media/baja** (aficionado; una sola powerbank probada).
10. Google, Android Developers, *WifiManager* (referencia), `WIFI_MODE_FULL_LOW_LATENCY` ("priority to achieve low latency"). **Confianza: alta para la existencia; solo se leyó la tabla de resumen.**
P. Código y datos del proyecto: `firmware_carro/firmware_carro.ino`, `carro.c`, `sensores.c`, `config.h`, `tof.c`, `docs/latencia.md`, `docs/latencia_modos.md`, `docs/validacion_sensores.md`, `docs/estado_20261110.md`, `docs/referencia/compilacion.md`.

**NO CONSULTADAS (declaradas):** Manual de Referencia Técnica del ESP32 (TRM); hoja del TC1508A (ver 04); nota completa de `WifiManager` (solo tabla de resumen); valor mínimo de RTO de Linux/Android (200 ms) — solo se usa RFC 6298; hilos de foro sobre latencia de WebSocket/AsyncTCP en ESP32 (no buscados ni leídos); si el SoftAP del ESP32 reenvía tramas entre estaciones en capa 2 (INFERENCIA, no verificado); duración de escrituras/borrados de NVS (doc no la da; no medida); estado de los pines de motor y del TC1508A durante el reinicio (NO MEDIDO).

---

## 6. Qué implica para este carro

### Respuestas a las cuatro preguntas

**1. ¿El diseño de `loop()` es compatible con `LOOP_MAX_US` = 5000?**
**No bajo las condiciones MEDIDAS con la app conectada.** Cifras del proyecto: 1.68–1.80 ms sin cliente en reposo (0 excesos); con la app conectada en reposo 8.0 y 8.43 ms (4 y 2 excesos por 60 s); en el aire con 5 giros 3.25 ms (0 excesos); en el suelo 6.78 ms (2–3 excesos); pico de arranque 12.86–12.94 ms (1 exceso, al menos 3 arranques). Es decir, la diferencia (≈5–6 ms) aparece **al conectar el WebSocket**, y apunta a `ws.text`/`ws.cleanupClients`/`client->text` (candado del núcleo de LwIP o mutex del WS, contención con la tarea de WiFi del núcleo 0), a la tarea `async_tcp` de prioridad 10 que preempta a `loop()` si cae en el núcleo 1, y a `Serial.println` bloqueante (INFERENCIA; **no hay temporizadores por fase** que lo demuestren). Un timeout I2C (5 ms) agota por sí solo el presupuesto; `imu_iniciar` (≥130 ms) y `hw_tof_iniciar` lo superan por mucho, y solo ocurren con el carro parado.
**Qué hacer:** tratar `LOOP_MAX_US` como **métrica y no como garantía** hasta medir por fase; ver recomendaciones 1–3.

**2. ¿Qué pasa si el WebSocket se atasca?**
Secuencia (de las fuentes y del código): (a) la cola por cliente (32) se llena y los mensajes nuevos se **descartan** [5]: con `SENS` cada 100 ms eso ocurre en ≈3.2 s; (b) el watchdog propio de 1 s sin `FORWARD` repetido detiene los motores, **antes** de lo anterior; (c) a los 5 s sin ACK AsyncTCP cierra el cliente [4] → `__STOP_AUTO`; (d) si `loop()` en sí se atasca (no el WS), a los 5 s el TWDT reinicia el chip. **Distancia recorrida durante el atasco**: ≈19 cm al perfil AUTO (19.3 cm/s × 1 s, MEDIDO proyecto, `latencia.md`) y ≈38 cm a velocidad MANUAL (INFERENCIA: el doble de velocidad). La comunicación puede caer **sin cerrar la conexión TCP**; por eso el watchdog de 1 s es la defensa real.

**3. ¿Puede STOP quedar detrás de otros mensajes en la cola?**
**Sí, de dos maneras.** (a) *Cola de comandos del firmware* (32 × 32 B, FIFO): `carro_encolar_comando` nunca expulsa STOP/EBRAKE/`__STOP_AUTO` pero **no los adelanta**: esperan a los comandos anteriores; cada uno cuesta microsegundos, salvo los que escriben NVS (`BAL_*`, `IMU_ESCALA`, `ANT_BORRAR`) o los que bloquean (`IMU_CAL`→`imu_iniciar` ≥130 ms), y a la siguiente vuelta de `loop()` (1.7–8.4 ms MEDIDOS) (INFERENCIA por lectura del código). (b) *TCP*: un STOP enviado tras un mensaje perdido espera al reenvío (RTO ≥ 1 s según RFC 6298; 3 s configurado en el stack del ESP32) [7; 6]; bajo pérdida el STOP puede tardar más que el watchdog de 1 s (INFERENCIA). (c) *Cola de salida a la app* (16, descarta el más viejo) y cola WS (32, descarta los nuevos): los STOP del teléfono viajan en la dirección contraria y **no** se ven afectados por estas colas, pero **la confirmación del STOP** (ACK/respuesta) sí puede descartarse con la cola llena [5].
**No procesar el STOP en la tarea `async_tcp`** (LEDC/GPIO no son seguros entre tareas sobre el mismo canal): lo correcto es que la callback active una bandera de parada atómica que `loop()` lea **primero** en cada vuelta, antes de `carro_procesar_cola` (INFERENCIA de diseño; ver recomendación 4).

**4. ¿Qué lecturas I2C bloquean?**
Todas las `hw_i2c_*` llamadas desde `loop()` (la llamada es síncrona a través de `Wire`): (i) giroscopio, 2 bytes cada 10 ms (≈0.12–0.2 ms); (ii) sondeo del láser cada 4 ms (`TOF_SONDEO_MS`); (iii) lectura del láser cada 33 ms (`TOF_PERIODO_MS`); (iv) `hw_i2c_presente` en el reintento del láser cada 5 s; (v) diagnósticos `IMU?`/`CAPS?`; (vi) `imu_iniciar` (≥130 ms por esperas `delay`) y `hw_tof_iniciar` (decenas de ms, bloqueante). En operación normal ≈0.12–0.4 ms cada una; **ante una falla, hasta 5 ms** y en el peor caso ~2× por esperas múltiples [3; INFERENCIA]. Cifras de duración del bus: cálculo mío a 400 kHz (9 bits/byte). El firmware **no tiene** tiempos por fase que lo confirmen (NO MEDIDO).

### Recomendaciones concretas y verificables (ordenadas por valor)

1. **Instrumentar `loop()` por fases** (`micros()` antes/después de: `ws.cleanupClients`, `sensores_actualizar`, `carro_procesar_cola`, `enviar_mensajes_de_sensores`, `Serial`, I2C) y exponer el máximo por fase en `LOOP?`. Criterio: identificar la fase que aporta los 5–6 ms con la app conectada. Cuesta cero hardware (INFERENCIA de método).
2. **Sacar `Serial.println` de la ruta crítica** (o `Serial.setTxBufferSize(512)` y limitar el tamaño) y **mover el envío de `SENS` a una sola llamada `ws.textAll` por periodo**; medir el cambio en `LOOP?` con la app conectada (esperado: bajar de 8 a <5 ms, **NO MEDIDO**, INFERENCIA).
3. **No bloquear con `imu_iniciar`/`hw_tof_iniciar` mientras el carro puede moverse** y avisar por `LOOP?` cuando una llamada excede 20 ms; verificar que `IMU_CAL` nunca se acepte con motores activos (hoy el firmware ya exige quieto 300 ms; probar con un `FORWARD` real).
4. **Prioridad real de STOP:** al recibir STOP/EBRAKE en la callback, poner una bandera `volatile` (o `std::atomic`) que `loop()` revise al inicio de cada vuelta; además, en `carro_procesar_cola`, procesar primero cualquier STOP aunque haya comandos antes. Prueba: encolar 20 comandos de NVS simulados y medir el tiempo hasta que los motores se detienen (**NO MEDIDO**).
5. **Medir el rizado del riel de 3.3 V** (osciloscopio, con la app conectada, motores en marcha y en arranque, powerbank y batería del carro): si baja de ≈2.8 V hay riesgo de brownout (2.43 V) y de sesgo del giroscopio (ver 02_mpu6050.md: sensibilidad de 4 °/s a 250 Hz–100 kHz). Registrar `esp_reset_reason()` en `CAPS?` (INFERENCIA).
6. **Pull-ups I2C externos** de 2.2–4.7 kΩ en SDA/SCL si el módulo no los trae (confirmar con multímetro o escáner de bus) y verificar la forma de onda a 400 kHz; si no cumple tr ≤ 300 ns, bajar a 100 kHz (la ganancia de tiempo de lectura es ≈0.3 ms, irrelevante; INFERENCIA).
7. **Verificar los pines de motor al reiniciar:** con las ruedas en el aire, provocar un reinicio (RST o corte de TWDT simulado con `esp_restart`) con motores girando y observar si se detienen durante todo el arranque; si no, añadir **resistencias de pull-down (≈10 kΩ)** a las entradas del TC1508A (según su hoja, **no leída**, ver 04_motores_cinematica.md). Esto cierra el mayor NO MEDIDO de seguridad.
8. **Teléfono:** adquirir un `WifiLock` con `WIFI_MODE_FULL_LOW_LATENCY` mientras la app esté en primer plano; comparar PING/PONG antes y después (6–30 ms MEDIDOS; criterio: p95 y máximo, no el promedio). Verificar que la app envíe la orden de movimiento repetida cada ≤200 ms (el firmware lo exige para el watchdog de 1 s).
9. **Un solo cliente WS:** documentar que un segundo cliente desplaza al teléfono; si la cámara o una pestaña de depuración deben conectarse, usar otro puerto/ruta o `max_connection` de la lógica.
10. **Keep-alive:** confirmar con medida (multímetro en serie en la salida de la powerbank) la corriente mínima de **esa** powerbank y diseñar el pulso con transistor (≥100 mA según el único README); no cargar directamente GPIO2 [1; 9]. Cifras de powerbank: **media/baja**, no usarlas como garantía.
11. **Quitar o comentar `WiFi.setSleep(false)`** como supuesto de latencia (no tiene efecto en AP puro), y anotar que el ahorro de energía relevante está en las estaciones; si la latencia de PING fluctúa, es el teléfono o la cámara, no el carro (INFERENCIA).
12. **Perfil de `Serial` y NVS:** registrar con el temporizador por fase cuánto bloquea una escritura NVS real (`BAL_*`) y, si supera 20 ms, diferirla a cuando el carro lleve parado >1 s (INFERENCIA).

Para el **PWM y la zona muerta de los motores** (1500 Hz a 8 bits, arranque a 255 durante 250 ms) véase `04_motores_cinematica.md`; aquí solo se verificó que 1500 Hz/8 bits cabe en el temporizador LEDC (máx. ≈15 bits a 80 MHz, cálculo mío) y que solo `loop()` escribe los canales.

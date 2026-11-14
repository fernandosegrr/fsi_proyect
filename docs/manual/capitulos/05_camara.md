# 5. Cámara

Este capítulo documenta el firmware que ejecuta la placa independiente `Freenove ESP32-WROVER` con el sensor de imagen `GC0308` (`192.168.4.2`). Explica por qué se seleccionó la resolución `HQVGA` (`240×176` píxeles) con reloj a `20 MHz` frente a `QVGA`, cómo opera la arquitectura de doble núcleo con buzón de último cuadro para eliminar el rezago visual, qué entregan sus tres modos de transmisión y cómo gestiona las desconexiones el módulo de recuperación en `C99`.

## 5.1 Hardware de captura y justificación de la resolución `HQVGA`

La placa `Freenove ESP32-WROVER` integra un microcontrolador `ESP32` de doble núcleo, memoria `PSRAM` externa y un sensor óptico CMOS **`GC0308`**. A diferencia de sensores como el `OV2640`, el chip `GC0308` **no posee codificador JPEG por hardware**: entrega cada cuadro únicamente en formatos sin comprimir (`RGB565` de 2 bytes por píxel o `YUV422`). Por lo tanto, cuando se desea transmitir video `MJPEG` al teléfono, la propia `ESP32-WROVER` debe comprimir la imagen por software empleando la biblioteca `JPEGENC` (con factor de calidad `50`).

Para determinar la configuración óptima en navegación semiautónoma, se realizó un barrido experimental de **100 cuadros consecutivos** comparando las resoluciones `HQVGA` (`240×176` píxeles = `42,240` píxeles) y `QVGA` (`320×240` píxeles = `76,800` píxeles) con frecuencias de reloj externo (`XCLK`) de `16 MHz` y `20 MHz`:

| Configuración probada (100 cuadros) | Tasa media (`fps`) | Tiempo de captura (`ms`) | Compresión JPEG (`ms`) | Edad media de cuadro (`ms`) | Edad percentil 95 (`ms`) | Tamaño medio JPEG (`B`) |
|---|---|---|---|---|---|---|
| **`HQVGA` (`240×176`) @ `20 MHz`** *(elegida)* | **`12.4 fps`** | `2.6 ms` | `9.2 ms` | **`137 ms`** | **`165 ms`** | `4,837 B` |
| `HQVGA` (`240×176`) @ `16 MHz` | `10.1 fps` | `3.1 ms` | `9.3 ms` | `164 ms` | `198 ms` | `4,812 B` |
| `QVGA` (`320×240`) @ `20 MHz` | `8.8 fps` | `8.0 ms` | `16.4 ms` | `252 ms` | `317 ms` | `7,940 B` |
| `QVGA` (`320×240`) @ `16 MHz` | `7.3 fps` | `9.4 ms` | `16.5 ms` | `289 ms` | `358 ms` | `7,905 B` |

### Por qué la edad de cuadro importa más que los megapíxeles

En un robot móvil que avanza en modo `AUTO` a una velocidad típica de `~20 cm/s` (`200 mm/s`), una edad de cuadro de `252 ms` (con p95 de `317 ms` en `QVGA`) significa que el teléfono está tomando decisiones sobre una imagen capturada cuando el carro estaba entre `5.0 cm` y `6.3 cm` más atrás. Al reducir la resolución a **`HQVGA` (`240×176`) con `XCLK = 20 MHz`**, la edad media de extremo a extremo baja a **`137 ms`** (p95 de **`165 ms`**) y la cadencia sube a **`12.4 fps`**, reduciendo casi a la mitad el desplazamiento ciego entre captura y decisión, mientras que una rejilla de `240×176` píxeles sigue ofreciendo `24×17` bloques de `10×10` píxeles más que suficientes para detectar una caja de `20×20 cm` en el piso.

## 5.2 Arquitectura de doble núcleo y buzón de último cuadro

En una cámara de red convencional, si los cuadros capturados se depositan en una cola FIFO de varios elementos (`fb_count > 1` con retención) y el enlace `WiFi` sufre una pausa momentánea, el consumidor lee cuadros atrasados acumulados en memoria. Para evitarlo, `camara_prueba.ino` implementa una arquitectura de **buzón de último cuadro (*latest-frame mailbox*)** repartida entre los dos núcleos del `ESP32-WROVER`:

1. **Tarea de captura (`Core 1`):** extrae continuamente el buffer del driver `esp_camera` (`fb_count = 2`, modo `CAMERA_GRAB_LATEST`), copia o comprime el cuadro en un doble buffer en `PSRAM` protegido por semáforo mutex y actualiza el puntero al cuadro más reciente, descartando cualquier cuadro anterior que aún no haya sido enviado.
2. **Tareas de red (`Core 0`):** atienden las peticiones `HTTP` y el transmisor `UDP`, leyendo siempre la última instantánea disponible en el buzón sin bloquear la captura del siguiente cuadro.

Además, el servidor de video continuo (`:81/stream`) acepta deliberadamente **un solo cliente a la vez**: si un segundo dispositivo intentara abrir otro flujo `MJPEG` simultáneo, degradaría el ancho de banda del canal `WiFi` compartido con el control del carro. En la aplicación Android, `SesionVideo` comparte ese único flujo entre todas las pantallas.

[FIGURA:camara_modos|Los tres modos de transmisión de la cámara Freenove: flujo MJPEG para vista humana y red neuronal, cuadro crudo RGB565 y rejilla compacta de celdas por UDP.]

## 5.3 Endpoints HTTP y protocolo UDP de celdas (`1,244` bytes)

La cámara se conecta como estación (`STA`) a la red `CarroFSI` solicitando la dirección IP estática **`192.168.4.2`** y expone cinco servicios de red:

| Servicio / Endpoint | Protocolo y puerto | Formato de salida | Tamaño por entrega | Uso dentro del sistema |
|---|---|---|---|---|
| `/stream` | `HTTP` `:81` | `multipart/x-mixed-replace` (`MJPEG`) | `~4.8 KB` / cuadro | Flujo principal en vivo en la aplicación Android (vista humana, `obstaculos.c` y `DetectorTflite`). |
| `/capture` | `HTTP` `:80` | `image/jpeg` (cuadro único) | `~4.8 KB` | Captura puntual y pruebas de conectividad. |
| `/raw` | `HTTP` `:80` | `application/octet-stream` (`RGB565`) | `84,480 B` (`240×176×2`) | Procesamiento sin artefactos de compresión `JPEG` y cabeceras `X-Width`, `X-Height`, `X-Frame-Seq`. |
| `/stats` | `HTTP` `:80` | `application/json` | `~220 B` | Telemetría interna de tiempos de captura, compresión, envío, RSSI y memoria libre. |
| **Modo celdas** | `UDP` `:4210` | Binario `CEL1` (promedios RGB de `10×10` px) | **`1,244 B`** exactos | Envío ultracompacto de la rejilla de `24×17` celdas (`408` celdas) para detección cromática de baja latencia. |

### Estructura del paquete UDP de celdas (`paquete_celdas.h`)

Cuando un cliente envía un datagrama de suscripción al puerto `UDP 4210` de la cámara (y lo renueva antes de `5,000 ms`), la propia `ESP32-WROVER` divide cada imagen `RGB565` de `240×176` píxeles en **`24` columnas por `17` filas** de `10×10` píxeles (descartando las 6 líneas inferiores sobrantes), calcula el promedio $(R, G, B)$ de 8 bits de cada bloque y transmite un único datagrama `UDP` de **`1,244` bytes**:

- **Cabecera (`20` bytes, *little-endian*):** magia `"CEL1"` (`4 B`), versión `1` (`1 B`), columnas `24` (`1 B`), filas `17` (`1 B`), reservado `0` (`1 B`), número de secuencia `seq` (`4 B`), marca de tiempo `ts_ms` (`4 B`), tiempo de captura `cap_us` (`2 B`) y tiempo de cálculo de celdas `proc_us` (`2 B`).
- **Carga útil (`1,224` bytes):** `408` tripletas consecutivas $(R, G, B)$ en orden por filas ($24 \times 17 \times 3 = 1{,}224\text{ bytes}$).

De este modo, en lugar de transmitir `84,480` bytes crudos o `~4,837` bytes comprimidos y decodificarlos en el teléfono, el enlace inalámbrico transporta solo `1,244` bytes que alimentan directamente al clasificador `obstaculos_procesar_celdas_rgb`.

## 5.4 Vigilancia y recuperación automática ante fallos (`recuperacion.c`)

Dado que la cámara opera con su propia batería portátil y sobre un enlace `WiFi` sujeto a interferencias, el firmware incluye el módulo en `C99` `recuperacion.c` (verificado en computadora dentro de `pruebas_firmware`), que supervisa tres contadores de salud y aplica acciones escalonadas sin intervención humana:

1. **Pérdida de asociación `WiFi`:** si la cámara pierde conexión con el punto de acceso `CarroFSI`, intenta reconectar cada **`2,000 ms`** (`RECUP_WIFI_REINTENTO_MS`). Si acumula **`15` intentos fallidos** (`~30 s` sin red), solicita un reinicio limpio del microcontrolador (`RECUP_REINICIAR_ESP`).
2. **Fallos consecutivos del sensor `GC0308`:** si `esp_camera_fb_get()` devuelve `NULL` en **`5` lecturas seguidas** (`RECUP_FALLOS_CAM_REINICIO`), el módulo ejecuta una reinicialización en caliente del periférico de cámara (`RECUP_REINICIAR_CAMARA`). Si aun así alcanza **`20` fallos consecutivos** (`RECUP_FALLOS_CAM_ESP`), reinicia la placa completa.
3. **Inactividad prolongada sin producir cuadros:** si transcurren **`10,000 ms`** (`RECUP_SIN_CUADRO_MS`) sin que la tarea de captura logre actualizar el buzón, dispara un reinicio completo para destrabar el bus `I2S/DMA` de la cámara.

Dónde está en el código: `camara_prueba.ino`, `recuperacion.h`, `recuperacion.c`, `paquete_celdas.h`, `paquete_celdas.c`.

## 5.5 Autónomo v2: telemetría fotométrica y autoprueba (`COMPILADO`, no grabado)

Rama `feat/autonomo-v2`. Este firmware de la cámara (`cam-v2`) **solo se compiló** (1 084 349 bytes, 82 %); no se grabó en la placa ni se probó con el sensor.

- `/stats` agrega tres claves **al final** del JSON, que un cliente anterior ignora: `"fw":"cam-v2"`, `"caps":["foto"]` y `"foto":{...}`.
- `foto` trae solo números: `medidas`, `hace_ms`, la media de rojo, verde y azul de todo el cuadro (`r`, `g`, `b`) y de su mitad de abajo, que es el piso (`r_abajo`, `g_abajo`, `b_abajo`), y las milésimas de muestras quemadas y oscuras. Se mide un cuadro cada 500 ms saltando de 8 en 8 píxeles. No sale ninguna imagen.
- Sirve para saber si la cámara cambió su exposición o su balance de blancos entre «Aprender piso» y la ruta: la app copia estos números a la cabecera de la caja negra.
- `AUTOPRUEBA` queda en 0. Encendida, un minuto después de cada arranque pedía `size=qvga`, cambiaba el compresor y no restauraba nada.

El bloqueo de exposición y de balance de blancos quedó en diseño: el proyecto no tiene la hoja de datos del `GC0308` y el firmware no escribe ningún registro del sensor (`PENDIENTE`).

# 5. Cámara

Este capítulo documenta el firmware que ejecuta la placa independiente `Freenove ESP32-WROVER` con el sensor de imagen `GC0308` (`192.168.4.2`). Explica por qué la cámara arranca en `HQVGA` (`240×176` píxeles) con reloj a `20 MHz` frente a `QVGA`, cómo opera la arquitectura de doble núcleo con casilla única de último cuadro para eliminar el rezago visual, qué entregan sus tres modos de transmisión y cómo gestiona las desconexiones el módulo de recuperación en `C`. Los datos de este capítulo se revisaron contra `camara_prueba/camara_prueba.ino`, `camara_prueba/recuperacion.h` y `nucleo_c/paquete_celdas.h` (v1.2); si algo no coincide, manda el código.

## 5.1 Hardware de captura y justificación de la resolución `HQVGA`

La placa `Freenove ESP32-WROVER` integra un microcontrolador `ESP32` de doble núcleo, memoria `PSRAM` externa y un sensor óptico CMOS **`GC0308`**. A diferencia de sensores como el `OV2640`, el chip `GC0308` **no posee codificador JPEG por hardware**: entrega cada cuadro únicamente en formatos sin comprimir (`RGB565` de 2 bytes por píxel o `YUV422`). Por lo tanto, cuando se desea transmitir video `MJPEG` al teléfono, la propia `ESP32-WROVER` debe comprimir la imagen por software con `frame2jpg` (de la biblioteca `esp32-camera`) con calidad `50`. La biblioteca `JPEGENC` (`camara_prueba/src/JPEGENC`, de terceros, Apache-2.0) está incluida solo para comparar compresores (`COMPRESOR_JPEGENC 1`, se activa con `/control?comp=`); la tabla de comparación sigue vacía, así que el compresor es `frame2jpg`.

La configuración por defecto del código es `CONFIG_DEFECTO = { 20 MHz, RGB565, HQVGA, calidad 50 }`. Para escogerla se hizo un barrido en la placa real (13-oct-2026, 100 cuadros por combinación, sin cliente de video; `docs/camara_prueba.md`). Estas medidas son de la placa sola, **NO MEDIDO** en el carro en marcha:

| Configuración probada (RGB565, calidad 50) | Tasa de salida (`fps`) | Compresión JPEG (`ms`) | Edad del cuadro hasta el fin de la compresión (`ms`) | JPEG medio (`B`) |
|---|---|---|---|---|
| **`HQVGA` (`240×176`) @ `20 MHz`** *(la del código)* | **`12.4`** | `56` | **`137`** | `3,986` |
| `HQVGA` (`240×176`) @ `16 MHz` | `12.8` | `56` | `135` | `3,556` |
| `QVGA` (`320×240`) @ `20 MHz` | `8.8` | `107` | `252` | `6,254` |
| `QVGA` (`320×240`) @ `16 MHz` | `8.0` | `106` | `231` | `5,989` |

A `16 MHz`, `HQVGA` da prácticamente lo mismo que a `20 MHz` (12.8 contra 12.4 fps); `20 MHz` es lo máximo que se probó. `RGB565` comprime más rápido que `YUV422` (107 contra 125 ms en `QVGA`) y la calidad `50` baja el JPEG un 23 % frente a la `70` sin cambiar el tiempo. El documento de pruebas (`docs/camara_prueba.md`) todavía llama «por defecto» a `QVGA`, decisión del 13-oct; el código arranca en `HQVGA` y esa diferencia ya está señalada en su nota inicial (la pregunta de quién decidió el cambio está anotada en el subsistema C).

### Por qué la edad de cuadro importa más que los megapíxeles

En un robot que avanza en modo `AUTO` a unos `20 cm/s`, una edad de cuadro de `252 ms` (`QVGA`) significa que el teléfono decide sobre una imagen tomada cuando el carro estaba unos `5 cm` más atrás. Con `HQVGA` a `20 MHz` la edad baja a `137 ms` (unos `2.7 cm`) y la cadencia sube a `12.4 fps`. La rejilla de `240×176` píxeles da `24×17` celdas de `10×10` píxeles para el detector de color. Esa edad no incluye el WiFi, el navegador ni la pantalla del teléfono: el retraso total medido con cronómetro fue de `320 ms` en una sola medida (`docs/latencia.md`).

## 5.2 Arquitectura de doble núcleo y casilla única de último cuadro

En una cámara de red convencional, si los cuadros capturados se depositan en una cola de varios elementos y el enlace `WiFi` sufre una pausa momentánea, el consumidor lee cuadros atrasados. Para evitarlo, `camara_prueba.ino` usa una **casilla única** (el cuadro más reciente) repartida entre los dos núcleos del `ESP32-WROVER`:

1. **Tarea de captura (`tarea_captura`, núcleo 0, prioridad 4):** pide al driver `esp_camera` el cuadro más nuevo (`fb_count = 3` con `RGB565`, modo `CAMERA_GRAB_LATEST`: uno en compresión, uno en la casilla y uno llenándose) y lo deja en la casilla. Si llega uno nuevo y el anterior seguía ahí, el viejo se devuelve a la cámara y se cuenta en `descartados`. Un cuadro con más de `EDAD_MAX_US` (`450,000 µs`) al salir de la cámara se tira. Si nadie pide video, la captura se detiene y la casilla se vacía, para que ningún cuadro envejezca guardado.
2. **Tareas de envío (núcleo 1):** `tarea_stream` toma el cuadro más reciente, lo comprime y lo manda; `tarea_raw` lo manda sin comprimir; `tarea_udp` atiende `HOLA` y `PING`. Un cliente que no recibe en `1.5 s` se corta, así nunca se acumulan cuadros.

Además, el servidor de video continuo (`:80/stream`) acepta deliberadamente **un solo cliente a la vez**: si otro dispositivo pide `/stream` o `/raw`, contesta `503 ya hay un stream activo`. En la aplicación Android, `SesionVideo` comparte ese único flujo entre todas las pantallas.

[FIGURA:camara_modos|Los tres modos de transmisión de la cámara Freenove: flujo MJPEG para vista humana, cuadro crudo RGB565 y rejilla compacta de celdas por UDP.]

## 5.3 Endpoints HTTP y protocolo UDP de celdas (`1,244` bytes)

La cámara se conecta como estación (`STA`) a la red `CarroFSI` con la dirección IP estática **`192.168.4.2`** y atiende HTTP en el puerto `80` y UDP en el `4210`. Las rutas HTTP son `/` (página con estadísticas), `/capture`, `/stream`, `/raw`, `/control`, `/stats` y `/salud`:

| Servicio / Endpoint | Protocolo y puerto | Formato de salida | Tamaño por entrega | Uso dentro del sistema |
|---|---|---|---|---|
| `/stream` | `HTTP` `:80` | `multipart/x-mixed-replace` (`MJPEG`) | `~2 a 4 KB` / cuadro según la escena | Flujo principal en vivo en la aplicación Android (vista humana, `obstaculos.c` y `DetectorTflite`). |
| `/capture` | `HTTP` `:80` | `image/jpeg` (cuadro único) | `~2 a 4 KB` | Captura puntual y pruebas de conectividad. |
| `/raw` | `HTTP` `:80` | `application/octet-stream` (`RGB565`) | `84,480 B` (`240×176×2`) | Procesamiento sin artefactos de compresión `JPEG`; cabeceras `X-Ancho`, `X-Alto`, `X-Numero` y `X-Ms-Captura-Envio`. |
| `/stats` | `HTTP` `:80` | `application/json` | cientos de bytes | Telemetría de tiempos de captura, compresión, envío, RSSI, memoria libre y último reinicio. |
| `/control`, `/salud` | `HTTP` `:80` | texto | pocos bytes | `/control?modo=` (jpeg, raw o celdas), `size=` y `comp=` cambian el modo, la resolución o el compresor (al reiniciar vuelve a jpeg); `/salud` responde `ok` y sirve al vigilante. |
| **Modo celdas** | `UDP` `:4210` | Binario `CEL1` (promedios RGB de `10×10` px) | **`1,244 B`** exactos | Envío compacto de la rejilla de `24×17` celdas (`408` celdas) para detección cromática de baja latencia. |

### Estructura del paquete UDP de celdas (`nucleo_c/paquete_celdas.h`)

En modo celdas el teléfono manda `HOLA` por UDP al puerto `4210` cada segundo; la cámara contesta `HOLA-OK`, recuerda su dirección y su puerto, y deja de mandar si pasan `5 s` sin `HOLA`. Con `PING n` contesta `PONG`, y así el teléfono mide el ida y vuelta de la red. La cámara divide cada imagen `RGB565` de `240×176` en **`24` columnas por `17` filas** de `10×10` píxeles, calcula el promedio $(R, G, B)$ de cada bloque y manda un único datagrama de **`1,244` bytes**:

- **Cabecera (`20` bytes):** marca `"CEL1"` (bytes 0 a 3; el `1` es la versión del formato), número de cuadro (4 a 7, sube de uno en uno), ms desde la captura hasta armar el paquete (8 a 11), ancho (12 a 13) y alto (14 a 15) de la imagen original, columnas (16), filas (17) y dos bytes reservados en `0` (18 a 19).
- **Carga útil (`1,224` bytes):** `408` tripletas consecutivas $(R, G, B)$ por filas ($24 \times 17 \times 3 = 1{,}224\text{ bytes}$).

En lugar de transmitir `84,480` bytes crudos o unos `4,000` comprimidos y decodificarlos en el teléfono, el enlace lleva solo `1,244` bytes que alimentan al clasificador de celdas de `nucleo_c`. El paquete se arma y se lee con el mismo código C (`paquete_armar`, `paquete_leer`).

## 5.4 Vigilancia y recuperación automática ante fallos (`recuperacion.c`)

Dado que la cámara opera sobre un enlace `WiFi` sujeto a interferencias, el firmware incluye el módulo en C `camara_prueba/recuperacion.c` (verificado en computadora en `pruebas_firmware/pruebas_camara.c`), que **solo decide** qué hacer; quien actúa es `camara_prueba.ino` desde la tarea `tarea_vigilante` (núcleo 0, prioridad 5). Los valores salen de `recuperacion.h`:

1. **Pérdida de asociación `WiFi`:** vuelve a llamar a `WiFi.begin` con esperas de **`1, 2, 4, 8` y luego `10 s`** (`REC_ESPERA_INICIAL_MS` = 1,000; `REC_ESPERA_MAX_MS` = 10,000). En el arranque, el primer intento tiene `5 s` (`REC_GRACIA_ARRANQUE_MS`) antes del primer reintento. Si pasan **`60 s`** sin conectar (`REC_SIN_WIFI_MAX_MS` = 60,000), reinicia la placa.
2. **Servidor HTTP que no atiende:** la placa se pide `/salud` a sí misma cada `10 s`; **`3` fallos seguidos** (`REC_FALLOS_SERVIDOR_MAX`, unos `30 s`) la reinician.
3. **Captura atorada:** si alguien pide video y pasan más de **`5 s`** (`REC_CUADRO_MAX_MS` = 5,000) sin un cuadro nuevo, reinicia. Sin demanda de video no cuenta.
4. **Bucle principal colgado:** si `loop()` no da señal en más de **`8 s`** (`REC_LATIDO_MAX_MS` = 8,000), reinicia.

El motivo de cada reinicio propio (`sin_wifi_60s`, `servidor_no_atiende`, `cuadro_viejo_5s`, `bucle_sin_senal`) queda en memoria RTC y se lee en `/stats`, campo `ultimo_reinicio`. No hay una reinicialización «en caliente» del sensor: si no inicia al arrancar, la placa no se reinicia sola (`docs/camara_prueba.md`, «Qué NO cubre»).

Dónde está en el código: `camara_prueba/camara_prueba.ino`, `camara_prueba/recuperacion.h`, `camara_prueba/recuperacion.c`, `nucleo_c/paquete_celdas.h` y `nucleo_c/paquete_celdas.c`.

## 5.5 Autónomo v2: telemetría fotométrica y autoprueba (`COMPILADO`, no grabado)

Rama `feat/autonomo-v2`. Este firmware de la cámara (`cam-v2`) **solo se compiló** (1 084 349 bytes, 82 %); no se grabó en la placa ni se probó con el sensor.

- `/stats` agrega tres claves **al final** del JSON, que un cliente anterior ignora: `"fw":"cam-v2"`, `"caps":["foto"]` y `"foto":{...}`.
- `foto` trae solo números: `medidas`, `hace_ms`, la media de rojo, verde y azul de todo el cuadro (`r`, `g`, `b`) y de su mitad de abajo, que es el piso (`r_abajo`, `g_abajo`, `b_abajo`), y las milésimas de muestras quemadas y oscuras. Se mide un cuadro cada 500 ms saltando de 8 en 8 píxeles. No sale ninguna imagen.
- Sirve para saber si la cámara cambió su exposición o su balance de blancos entre «Aprender piso» y la ruta: la app copia estos números a la cabecera de la caja negra.
- `AUTOPRUEBA` queda en 0. Encendida, un minuto después de cada arranque pedía `size=qvga`, cambiaba el compresor y no restauraba nada.

El bloqueo de exposición y de balance de blancos quedó en diseño: el firmware no escribe ningún registro del sensor (`PENDIENTE`). *Corrección del 16-nov-2026:* existe una copia de la hoja de datos del `GC0308` (V1.0, 2010; copia de un tercero, marcada confidencial, no oficial; se enlaza y no se copia al repositorio); lo que faltaba era leerla contra el controlador que usa el firmware (ver la siguiente sección).

## 5.6 Investigación del `GC0308` (v3, solo lectura de documentación)

Documento completo: `docs/investigacion_autonomo/06_gc0308_camara.md`. Nada de esto se probó con el sensor real.

- **El controlador y la hoja de datos discrepan** en el bit de exposición automática (AEC), en el de ganancia automática (AGC) y en la disposición del registro de exposición manual. Solo el bit de balance de blancos automático coincide en la hoja, el controlador `esp32-camera` y el núcleo de Linux. Por eso **no se deben llamar `set_exposure_ctrl`, `set_gain_ctrl` ni `set_aec_value` sin leer antes los registros** (confianza alta en la discrepancia; el sensor real no se consultó).
- Varias funciones (`set_awb_gain`, `set_gainceiling`, `set_brightness`, `set_quality`) son esqueletos vacíos en la rama principal del controlador; no se pudo saber qué versión del controlador trae el firmware compilado.
- **HQVGA (240×176) usa solo ~75 % del ancho del sensor** en el modo de submuestreo por omisión, así que el campo horizontal de 56° supuesto no puede valer a la vez para HQVGA y QVGA. Hay que medirlo por modo de video.
- **Sobre bloquear exposición y balance de blancos:** sigue sin decidirse hasta medir (M4 del protocolo). Si hace falta, lo de menor riesgo es fijar primero **solo el balance de blancos**, con lectura antes de escribir, un latido por UDP, un plazo con restauración, y reinicio del sensor o del chip como último recurso; sin guardar nada en la memoria no volátil.

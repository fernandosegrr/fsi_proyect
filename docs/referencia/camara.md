# Referencia de la cámara ESP32

**Estado:** documento de referencia · **Fecha:** 27 de octubre de 2026 · **Rama:** `docs/documentacion-completa`
**Código documentado:** `camara_prueba/` en `deae8b6`. **Fuentes:** `camara_prueba.ino`, `recuperacion.h/.c`, `docs/camara_prueba.md`, `docs/hardware_cargar.md` y `docs/entrega.md`.

> El encargo llama a esta carpeta `firmware_camara`. En el repositorio se llama `camara_prueba/` y así se mantiene; este documento y `camara_prueba/README.md` explican el nombre.

## 1. En una frase

La cámara es una **placa aparte con WiFi** que mira al frente, comprime lo que ve y lo sirve por HTTP (el mismo protocolo con el que se abren las páginas web) para que el teléfono lo muestre; además puede mandar solo los colores del piso por UDP (un envío rápido, sin confirmación de llegada) para que el detector de obstáculos trabaje sin transmitir video.

No es una ESP32-CAM genérica: es una **Freenove ESP32-WROVER v3.0** con sensor **GC0308**, un sensor que no sabe hacer JPEG. Por eso la placa entrega la imagen en bruto y la ESP32 la comprime por software. Esa decisión explica casi todos sus límites de velocidad.

## 2. La placa y el sensor

| Dato | Valor | Fuente |
|---|---|---|
| Placa | Freenove ESP32-WROVER v3.0 | `docs/camara_prueba.md` §H2 |
| Sensor | GC0308, 0.3 MP (PID 0x9B) | `docs/camara_prueba.md` §H2 |
| PSRAM | 4 MB | Igual |
| USB-serie | CH340 | Igual |
| Mapa de pines | `CAMERA_MODEL_WROVER_KIT` | `camara_prueba.ino:24-26` |
| XCLK / SIOD / SIOC | 21 / 26 / 27 | `docs/camara_prueba.md` §H2 |
| Datos Y2..Y9 | 4, 5, 18, 19, 36, 39, 34, 35 | Igual |
| VSYNC / HREF / PCLK | 25 / 23 / 22 | Igual |
| PWDN / RESET | No usados | Igual |

El sensor **no acepta el formato JPEG**: si se intenta, la inicialización falla con `0x106 ESP_ERR_NOT_SUPPORTED`. Por eso el programa pide RGB565 o YUV422 y comprime cada cuadro con `frame2jpg` (`camara_prueba.ino:2-15`). El mapa `AI_THINKER` tampoco sirve en esta placa.

Para que no se den por sabidos los formatos: **JPEG** es una foto comprimida, más pequeña pero con pérdida de calidad; **RGB565** y **YUV422** guardan cada píxel sin comprimir, así que ocupan mucho más pero conservan todo el detalle. La placa no tiene un compresor JPEG en el sensor, y por eso gasta tiempo de procesador en comprimir.

## 3. La red

La cámara se conecta como **cliente** a la red que crea el carro:

- IP fija **192.168.4.2**, puerta 192.168.4.1, máscara 255.255.255.0 (`camara_prueba.ino:89-91`).
- La IP es fija a propósito: el teléfono siempre sabe dónde buscar el video.
- El DHCP del carro reparte de .10 a .20 para no entregar la .2 (`config.h:42-52`; `firmware_carro.ino:287-310`).
- `WiFi.setSleep(false)` para no dormir la radio y no aumentar el retraso.
- Si la red no está o se cae, reintenta con esperas de 1, 2, 4, 8 y 10 s. A los 60 s sin conectar, se reinicia (`recuperacion.h:22-28`).

La cámara **no crea** su propia red: necesita que la DevKit esté encendida. Los dos dispositivos se pueden encender en cualquier orden; la cámara entra sola cuando aparece la red.

## 4. Los tres modos de video

El modo se cambia con `/control?modo=...`. Al reiniciar, vuelve siempre a `jpeg` (`camara_prueba.ino:167-171`, `1273-1287`).

| Modo | Qué manda | Para qué | Dónde |
|---|---|---|---|
| `jpeg` | Video MJPEG (una sucesión de fotos JPEG) y capturas sueltas | Ver por el teléfono | `/stream`, `/capture` |
| `raw` | Un cuadro RGB565 sin comprimir | Procesar la imagen en el teléfono sin pérdida | `/raw` |
| `celdas` | Promedio RGB de cada celda de 10×10 por UDP | Detector de obstáculos sin video | UDP 4210 |

En modo `celdas`, el JPEG sigue disponible en `/stream`; lo que cambia es que además se manda el paquete de celdas. El paquete tiene la marca `CEL1`, número de cuadro, antigüedad, tamaño, columnas, filas y los colores (`nucleo_c/paquete_celdas.h:1-30`). Con HQVGA son 24×17 celdas = 1244 bytes, que caben en un datagrama UDP.

El teléfono pide las celdas mandando `HOLA` al puerto 4210; la cámara responde `HOLA-OK` y recuerda la dirección. `PING n` se responde con `PONG n` para medir el retardo (`camara_prueba.ino:643-673`).

## 5. Las rutas HTTP

| Ruta | Devuelve | Uso |
|---|---|---|
| `/` | Página con el video y los números | Ver en el navegador |
| `/capture` | Un JPEG | Foto suelta |
| `/stream` | MJPEG continuo | Video |
| `/raw` | Un cuadro RGB565 | Procesamiento sin compresión |
| `/control?size=...` | Cambia QQVGA/HQVGA/QVGA/VGA | Resolución |
| `/control?comp=...` | Cambia el compresor (`esp`, `420`, `444`, `baja`) | Comparar compresores |
| `/control?modo=...` | Cambia `jpeg`, `raw` o `celdas` | Modo de video |
| `/stats` | JSON con fps, tiempos, memoria y estado | Diagnóstico |
| `/salud` | `ok` | Vigilante interno |

Cada petición se anota en el puerto serie como `GET <ruta> desde <ip>` (`camara_prueba.ino:21-22`). `/stats` incluye, entre otros: fps de captura y envío, edad del cuadro, ms de compresión y envío, descartados, RSSI, memoria libre, `uptime_s`, reconexiones, último reinicio y estado del modo celdas (`camara_prueba.ino:1315-1335`).

## 6. Cómo se procesa un cuadro

El programa usa **dos núcleos** para no acumular retraso (`camara_prueba.ino:8-15`):

```text
núcleo 0: tarea_captura
   pide un cuadro a la cámara
   lo deja en UNA sola casilla
   si la casilla estaba ocupada, tira el viejo

núcleo 1: tarea_stream / manejar_captura
   toma el cuadro más reciente
   lo comprime con frame2jpg
   lo envía por HTTP
```

La casilla única es la clave: si el teléfono va lento, se pierden cuadros viejos en vez de acumularse. Un cuadro que ya tiene más de 450 ms al salir de la cámara se descarta (`EDAD_MAX_US`, `camara_prueba.ino:95`). Sin demanda de video, la captura se detiene; así no se guarda un cuadro que envejece.

La configuración por defecto en el código es:

```c
static const Config CONFIG_DEFECTO = { 20, PIXFORMAT_RGB565, FRAMESIZE_HQVGA, 50 };
```

Es decir, 20 MHz, RGB565, **HQVGA 240×176** y calidad JPEG 50 (`camara_prueba.ino:105-108`). El comentario del código dice que se eligió HQVGA por sus ~12 fps y ~0,32 s de retraso, y que QVGA se siente más lenta.

> **Contradicción detectada:** `docs/camara_prueba.md` §«Lo medido» concluye «se queda QVGA como valor por defecto», pero el código actual usa HQVGA. Se anota en `docs/pendientes_documentacion.md`. Para el manual vale el código; si se prefiere QVGA, se cambia con `/control?size=qvga` o en `CONFIG_DEFECTO`.

## 7. Rendimiento medido

El barrido de H3 midió 100 cuadros por combinación, sin cliente, con luz intermedia (Y ≈ 124-127). Los números completos están en `docs/camara_prueba.md` §«Lo medido». Los más útiles:

| Configuración | fps de salida | Compresión | Edad del cuadro |
|---|---|---|---|
| 20 MHz, RGB565, QVGA | 8,8 | 107 ms | 252 ms |
| 20 MHz, RGB565, HQVGA | 12,4 | 56 ms | 137 ms |
| 20 MHz, RGB565, QQVGA | 10,0 | 26 ms | 126 ms |
| 16 MHz, RGB565, HQVGA | 12,8 | 56 ms | 135 ms |
| 10 MHz, RGB565, QVGA | 5,0 | 101 ms | 301 ms |

Qué significan:

- **La resolución manda.** HQVGA tiene el 55 % de los píxeles de QVGA y da ~41 % más fps y ~46 % menos edad.
- **RGB565 comprime más rápido que YUV422** (107 contra 125 ms en QVGA) y genera JPEG algo más chicos.
- **La calidad 50 contra 70 casi no cambia el tiempo** (107 contra 109 ms), pero baja el tamaño ~23 %. Por eso se queda 50.
- **El límite es el sensor y la compresión por software**, no la red. Con un sensor con JPEG por hardware (OV2640) el límite cambiaría.

La edad del cuadro se mide desde que empieza la captura hasta que termina el envío. Incluye la lectura del sensor (~100 ms). El **retraso total hasta la pantalla del teléfono no está medido**: solo hay una muestra histórica de ~530 ms (`docs/camara_prueba.md` §«Cómo repetir la prueba del cronómetro»). Cualquier cifra de latencia de extremo a extremo es **NO MEDIDA**.

## 8. Autoprueba y recuperación

Al arrancar, con `AUTOPRUEBA 1`, la placa se pide a sí misma `/stats`, `/capture` y `/stream` por 127.0.0.1 (`camara_prueba.ino:84-85`). Eso comprueba que el servidor responde, pero **no mide el WiFi**: el cliente local comparte el procesador y los resultados son pesimistas. En H3 dio 8,2 fps en `/stream` y 250 ms de edad media (`docs/camara_prueba.md` §«Autoprueba local»).

El vigilante de `recuperacion.c` decide, y el `.ino` actúa:

| Situación | Qué hace |
|---|---|
| Se pierde el WiFi | `WiFi.begin` con espera creciente; a los 60 s, reinicio |
| `/salud` no responde 3 veces seguidas (30 s) | Reinicio |
| Alguien pide cuadros y no llega ninguno en 5 s | Reinicio |
| `loop()` no avanza en 8 s | Reinicio |

El motivo del reinicio se guarda en memoria RTC y sale en `/stats` (`ultimo_reinicio`) y por el puerto serie. Los códigos y el detalle están en `docs/camara_prueba.md` §«Robustez del WiFi y de la cámara».

Lo que **no** cubre: si el sensor no inicia al arrancar, la placa se queda detenida y no se reinicia sola; si la radio se cuelga pero `WiFi.status()` sigue diciendo «conectado», la placa tampoco lo nota (`docs/camara_prueba.md` §«Qué NO cubre»).

## 9. Compilar y cargar

```powershell
.\scripts\cargar.ps1 camara -SoloCompilar
.\scripts\cargar.ps1 camara -Puerto COM8
```

Se compila para **ESP32 Wrover Module** (`esp32:esp32:esp32wrover`, con PSRAM). `cargar.ps1` copia el sketch a una carpeta temporal y, si existen, los archivos `obstaculos.c/h` y `paquete_celdas.c/h` de `nucleo_c/` para el modo CELDAS (`scripts/cargar.ps1:176-190`). Sin esos archivos, el sketch compila solo con JPEG y RAW.

La tarjeta microSD de la placa debe estar vacía: no se usa. El secreto de la WiFi vive en `camara_prueba/secretos.h`, ignorado por git; si no existe, se usa `secretos.ejemplo.h`. **No se publica ni se lee en la documentación.**

La cámara ocupa ~83 % de la flash al compilar (`docs/camara_prueba.md` §«Cargar»). El binario no se versiona.

## 10. Qué se probó y qué no

| Afirmación | Estado | Evidencia |
|---|---|---|
| La placa es Freenove con GC0308 y PSRAM | VERIFICADO | `docs/camara_prueba.md` §H2 |
| El sensor no hace JPEG | VERIFICADO | Error `0x106` documentado |
| El barrido de fps y compresión | VERIFICADO en local, sin cliente | `docs/camara_prueba.md` §«Lo medido» |
| El video se ve en el teléfono | VERIFICADO en pruebas de H3 | Documento citado |
| El retraso real hasta la pantalla | NO MEDIDO (una muestra de ~530 ms) | Documento citado |
| La recuperación ante caída de WiFi | VERIFICADO en pruebas de PC (34 casos) y en la placa, según el documento | `pruebas_camara.c`, `docs/camara_prueba.md` |
| El modo CELDAS por UDP en el teléfono | VERIFICADO según la bitácora; no se repitió en el cierre | `docs/camara_prueba.md`, `docs/bitacora.md` |
| El comportamiento con la cámara montada en el carro en movimiento | NO MEDIDO | No hay registro |

## 11. Límites y cuidados

- **No es la cámara de la app:** la app usa la cámara del teléfono para el detector en `VisionActivity`; la del carro se usa para video.
- **Un solo cliente de video:** si dos dispositivos piden `/stream`, el segundo puede desplazar al primero o quedarse esperando. La app lo comparte entre pantallas, pero no con otro teléfono.
- **La cámara necesita la red del carro.** Si la DevKit está apagada, no hay video.
- **La IP .2 es fija.** Si otro dispositivo la toma, hay choque. El DHCP del carro ya la evita.
- **El modo RAW solo funciona con RGB565**; con YUV422 responde 503 (`camara_prueba.ino:1215`).
- **El modo `celdas` necesita que el sketch se haya compilado con los archivos del núcleo.** Si se abre suelto en el IDE de Arduino, no existe.
- **No se tocan los registros del sensor**: no hay hoja de datos del GC0308 en el proyecto y escribir a ciegas no es seguro (`docs/camara_prueba.md` §H3).
- **La cámara no se ha probado montada y en movimiento**; la vibración y el cambio de luz pueden afectar al detector.

## 12. Para seguir leyendo

- `docs/codigo/camara_prueba.md`: recorrido por funciones y tareas.
- `docs/camara_prueba.md`: documento histórico con el barrido completo.
- `docs/referencia/hardware.md`: alimentación y montaje.
- `docs/protocolo.md`: cómo pide la app el video y las celdas.
- `camara_prueba/README.md`: arranque rápido y rutas.

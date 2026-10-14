# Modos de video del carro: jpeg, raw y celdas (Trabajo I)

Objetivo: probar dos formas de bajar la latencia del video, **A) imagen cruda (raw)** y **B) resumen por celdas por UDP**, contra el **JPEG de siempre**. El modo **jpeg sigue siendo el
predeterminado** y no cambia. El protocolo de motores no se toca. Si un modo falla, la app vuelve sola a jpeg y avisa.

## 1. Cómo se elige el modo

- **Cámara** (`camara_prueba/camara_prueba.ino`): `http://192.168.4.2/control?modo=jpeg|raw|celdas`. Al reiniciar la cámara vuelve a jpeg. `/stats` trae `modo`, `bytes_s`, `celdas_enviadas`, `celdas_ms`, `celdas_max_ms`, `cliente_celdas` y `pings`; el puerto serie escribe cada 5 s una línea `MODO_CAM ...`.
- **App**: pantalla de visión, botón «Modo de video del carro: jpeg / raw / celdas» (debajo de «Fuente del video»). Cambiarlo vuelve a abrir el video. Si el modo pedido falló, el botón dice «(en uso: jpeg, falló el pedido)» y la pantalla muestra el motivo.
- **Cuándo falla un modo** (la app vuelve a jpeg y lo anota con `VIDEO W: ... vuelvo a jpeg`): la cámara contesta distinto de 200 a `/control?modo=`; en raw no llega ningún cuadro en 3 s o hay un error antes del primero; en celdas no llega ningún paquete UDP en 3 s o se dejan de recibir 3 s.

## 2. Qué hace cada modo

| | jpeg | raw | celdas |
|---|---|---|---|
| Cámara → teléfono | MJPEG por HTTP (`/stream`), compresión en la cámara | partes HTTP (`/raw`) con el cuadro RGB565 sin comprimir (84 480 bytes en HQVGA) | un paquete UDP por cuadro con el promedio RGB de cada celda de 10x10 (24 x 17 celdas = 1244 bytes) al puerto 4210; el JPEG sigue por `/stream` en paralelo |
| Cabecera por cuadro | `Content-Length` | `Content-Length`, `X-Ancho`, `X-Alto`, `X-Numero`, `X-Ms-Captura-Envio` | marca `CEL1`, número, ms captura→paquete, tamaño, rejilla (`nucleo_c/paquete_celdas.h`) |
| Cuadro viejo | casilla de 1 en la cámara y cola de 1 en la app | igual | casilla de 1; cola de 1 en la app; el paquete sale en la tarea de captura, antes que el JPEG |
| Lo que corre en el teléfono | decodificar JPEG + detector + giro visual + modelo | detector + giro visual + modelo (sin decodificar) | **solo el detector** sobre las celdas (mismo código C que con la imagen) |
| Limitaciones | — | ~0.8 MB/s de WiFi (9 fps en la prueba con cámara falsa) | **sin giro visual** (el giro se hace por tiempo con la calibración), **sin modelo de objetos**, **sin «Aprender piso»**, sin flujo del piso |

- El dueño de las celdas es **el mismo código del detector**: la cámara compila `nucleo_c/obstaculos.c` y `paquete_celdas.c` (`scripts/cargar.ps1 camara` los copia junto al sketch al compilar). `obstaculos_analizar_modelo_celdas` da exactamente el resultado de la imagen completa (`nucleo_c/pruebas_celdas.c`: 15 secuencias de 14 cuadros, otros tamaños y configuraciones, sin ninguna diferencia).
- Sin IP fija: el teléfono manda `HOLA` por UDP al puerto 4210 cada segundo y la cámara recuerda de quién vino y le manda las celdas ahí; sin `HOLA` en 5 s deja de mandar. El teléfono manda también `PING n` cada segundo (en cualquier modo) y la cámara contesta `PONG n`: ese es el **ida y vuelta de la red** (`rtt_*` en `MODO_STATS`).
- El fail-safe de 500 ms del navegador y las estadísticas de `VIDEO_STATS` cuentan **los paquetes de celdas** en el modo celdas (y los cuadros crudos en raw).

## 3. Lo que ya se probó SIN el carro (HECHOS)

- PC: `pruebas_celdas` 40 casos (rejilla 24 x 17, equivalencia cuadro/celdas, paquete UDP ida y vuelta, paquetes mal formados y 2000 de basura).
- Compilación del firmware de la cámara con los tres modos (82 % de la memoria de programa); los modos no se probaron en la cámara real.
- Teléfono (Huawei) con una cámara falsa (`scripts/probar_modos_video.py`: `scripts/camara_falsa.py` por `adb reverse` + `scripts/camara_falsa_udp.c` corriendo en el teléfono, solo imágenes sintéticas):

| Escenario | `MODO_STATS` típico (ventana de 5 s) |
|---|---|
| jpeg | 9.3 fps, llegada→decisión 10 a 12 ms (máx. 27), 0 descartados |
| raw | 9.2 fps, 775 KB/s, llegada→decisión 9 a 12 ms, 0 descartados |
| celdas | 10 fps, 12 KB/s, llegada→decisión 3.3 ms (máx. 21), 0 perdidos, JPEG paralelo 9.2 fps, ida y vuelta UDP 2 a 7 ms |
| raw con la cámara contestando 400 a `/control` | vuelve a jpeg y avisa |
| `/raw` sin mandar nada | vuelve a jpeg a los 3 s y avisa |
| celdas sin que llegue ningún UDP | vuelve a jpeg a los 3 s y avisa |

Estos números son de la cámara FALSA (sin sensor, sin WiFi del carro, sin compresión real): sirven para comprobar que el código funciona, **no** para comparar los modos de la cámara real.

## 4. PENDIENTE HARDWARE: la comparación (3 modos x 3 pasadas con una caja a ~80 cm)

Mismo obstáculo (una caja), misma distancia (~80 cm del arranque), misma luz y mismo piso en las 9 pasadas. Con la cámara cargada (`scripts\cargar.ps1 camara`) y la app instalada:

1. `adb logcat -G 16M` y `adb logcat -c`. Piso aprendido con «Aprender piso» **en modo jpeg** antes de empezar (en celdas ese botón no funciona; el modelo del piso vive en la app y no se borra al cambiar de modo, y si se perdiera la tira de abajo lo vuelve a aprender sola: lo importante es que la pista esté vacía al empezar cada pasada).
2. En la pantalla de visión: «Fuente del video» = video del carro y botón de modo = **jpeg**. En control: ruta recta (`Obstáculos: cámara`, la ruta de la prueba de obstáculos de `docs/prueba_obstaculos.md`), 3 pasadas con la caja a ~80 cm. Cada pasada escribe una línea `OBST_PRUEBA` con `modo_video=...`.
3. Repetir con el modo **raw** y con **celdas** (3 pasadas cada uno).
4. Al terminar: `adb logcat -d -v time -s OBST_PRUEBA MODO_STATS VIDEO_STATS > modos.txt`. De ahí salen las columnas de la tabla. En la cámara, la línea serie `MODO_CAM` (si la tienes conectada) o `/stats` dan `bytes_s` y `celdas_ms`.
5. Comprobar con números que **el color del piso es el mismo en los tres modos** (`ref=` / `Piso (RGB)` en la pantalla de visión o `pantalla| piso=` en el log): si en celdas o en raw salen el rojo y el azul intercambiados respecto a jpeg, el orden de bytes del RGB565 es el contrario al supuesto (el primer byte es el alto) y hay que cambiarlo en `enviar_celdas` (cámara) y `alCrudoDelCarro` (app).
6. Con el modo **celdas** activo, mirar `jpeg_paralelo_fps` en `MODO_STATS` (el JPEG de adorno): **no debe bajar de 8 fps**.

Tabla (se llena con las 9 pasadas; **vacía** hasta entonces):

| Modo | Pasada | retraso_total_ms | cm_recorridos | llegada→decisión (ms) | fps | hueco máx. (ms) | huecos > 300 ms | bytes/s | ida y vuelta/2 (ms) | JPEG paralelo (fps) | Notas |
|---|---|---|---|---|---|---|---|---|---|---|---|
| jpeg | 1 | | | | | | | | | — | |
| jpeg | 2 | | | | | | | | | — | |
| jpeg | 3 | | | | | | | | | — | |
| raw | 1 | | | | | | | | | — | |
| raw | 2 | | | | | | | | | — | |
| raw | 3 | | | | | | | | | — | |
| celdas | 1 | | | | | | | | | | |
| celdas | 2 | | | | | | | | | | |
| celdas | 3 | | | | | | | | | | |

Cómo leer el resultado: gana el modo que da menos `retraso_total_ms` y menos `cm_recorridos` sin más huecos de más de 300 ms. En raw y celdas el campo `latencia_video_ms` de `OBST_PRUEBA` es una ESTIMACIÓN (ms de la cámara + la mitad del ida y vuelta; en raw no incluye lo que tarda en bajar un cuadro de 84 KB por WiFi): si hay una ventaja clara, hay que medirla con cronómetro (como en `docs/latencia.md`) antes de cambiar el modo predeterminado.

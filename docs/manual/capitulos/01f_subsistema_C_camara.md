# Subsistema C · Módulo de cámara

## C.1 Qué es y para qué sirve

El módulo de cámara es la segunda placa del carro: una Freenove ESP32-WROVER v3.0 con un sensor GC0308 de 0.3 megapíxeles y 4 MB de PSRAM (memoria extra, fuera del chip). Toma imágenes del frente del carro y las manda por WiFi al teléfono, que las usa para mostrar el video y para detectar obstáculos. La cámara no mueve el carro ni decide nada: solo entrega imágenes. Se une a la red del carro como cliente, con IP fija `192.168.4.2`.

Más detalle: capítulo 5 del manual (`docs/manual/capitulos/05_camara.md`).

## C.2 Cómo funciona, paso a paso

```
 GC0308     núcleo 0: tarea_captura      núcleo 1: tarea_stream
 (RGB565) ─▶ pide el cuadro más nuevo ─▶ [casilla única] ─▶ frame2jpg ─▶ HTTP
                  │                      (el viejo se tira)              :80
                  │ modo celdas
                  ▼
            paquete CEL1 (UDP :4210, 1,244 B)
```

1. **Arranque** (`setup()` en `camara_prueba/camara_prueba.ino`). Configura el sensor con `CONFIG_DEFECTO`: reloj de 20 MHz, RGB565, HQVGA (240x176 píxeles) y calidad JPEG 50. Pide 3 búferes en PSRAM (`fb_count = 3`) y el modo `CAMERA_GRAB_LATEST`, que entrega siempre el cuadro más nuevo. Después mide la cámara unos 12 s (`docs/camara_prueba.md`), conecta el WiFi y levanta el servidor.
2. **WiFi.** `WiFi.config(IP_FIJA, PUERTA, MASCARA)` fija la `192.168.4.2` y `WiFi.setSleep(false)` apaga el ahorro de energía del radio para tener menos retraso. El DevKit reparte direcciones de `.10` a `.20`, así que su DHCP ya no entrega la `.2` (`docs/camara_prueba.md`, 16-oct-2026).
3. **Captura** (`tarea_captura`, núcleo 0, prioridad 4). Deja cada cuadro en UNA casilla. Si llega uno nuevo y el anterior seguía ahí, el viejo se devuelve a la cámara y se cuenta en `descartados`. Un cuadro con más de `EDAD_MAX_US` (450,000 µs = 450 ms) se tira. Si nadie pide video, la captura se detiene y la casilla se vacía, para que ningún cuadro envejezca guardado.
4. **Envío** (núcleo 1). `tarea_stream` toma el cuadro más reciente, lo comprime y lo manda; `tarea_raw` hace lo mismo sin comprimir; `tarea_udp` atiende `HOLA` y `PING`. Un cliente que no recibe en 1.5 s se corta: así nunca se acumulan cuadros.
5. **Servidor HTTP en el puerto 80** (`c.server_port = 80`): `/` (página con estadísticas), `/capture` (un JPEG), `/stream` (MJPEG, un JPEG tras otro), `/raw`, `/control`, `/stats` y `/salud`. Solo atiende un stream a la vez: si otro cliente pide `/stream` o `/raw`, contesta `503 ya hay un stream activo`.

### Los tres modos de video

Se eligen con `/control?modo=` seguido de `jpeg`, `raw` o `celdas`; al reiniciar la cámara vuelve a jpeg.

| Modo | Qué manda | Tamaño por cuadro |
|---|---|---|
| jpeg (predeterminado) | MJPEG por `/stream`, comprimido con `frame2jpg` | De 2 a 4 KB según la escena |
| raw | RGB565 sin comprimir por `/raw`, con las cabeceras `X-Ancho`, `X-Alto`, `X-Numero` y `X-Ms-Captura-Envio` | 84,480 B (240x176x2) |
| celdas | Promedio RGB de cada celda de 10x10 píxeles, por UDP al puerto 4210 | 1,244 B |

En el modo celdas el teléfono manda `HOLA` cada segundo y la cámara contesta `HOLA-OK` y recuerda su IP y su puerto; sin `HOLA` en 5 s (`cliente_celdas_vigente()`) deja de mandar. Con `PING n` contesta `PONG`, y así el teléfono mide el ida y vuelta de la red. El paquete `CEL1` (`nucleo_c/paquete_celdas.h`) lleva 20 bytes de encabezado (marca, número de cuadro, ms de captura a paquete, ancho, alto, columnas y filas) y 24 x 17 celdas de 3 bytes de color: 1,224 + 20 = 1,244 B, un solo paquete.

### Vigilante y recuperación

El vigilante es la tarea `tarea_vigilante` (núcleo 0, prioridad 5). Las decisiones están en `camara_prueba/recuperacion.c`, C puro; el `.ino` solo ejecuta la acción.

| Situación | Qué hace |
|---|---|
| Se pierde el WiFi | Reintenta con espera de 1, 2, 4, 8 y 10 s (`REC_ESPERA_INICIAL_MS` 1,000; `REC_ESPERA_MAX_MS` 10,000) |
| 60 s sin WiFi (`REC_SIN_WIFI_MAX_MS`) | Reinicia la placa (motivo `sin_wifi_60s`) |
| El servidor no contesta `/salud` | La placa se lo pide a sí misma cada 10 s; 3 fallos seguidos (`REC_FALLOS_SERVIDOR_MAX`) reinician |
| Hay demanda de video y más de 5 s sin cuadro (`REC_CUADRO_MAX_MS` 5,000) | Reinicia (`cuadro_viejo_5s`) |
| `loop()` sin dar señal más de 8 s (`REC_LATIDO_MAX_MS` 8,000) | Reinicia (`bucle_sin_senal`) |

El motivo del reinicio queda en memoria RTC y se lee en `/stats`, campo `ultimo_reinicio`. Cada `FOTO_PERIODO_MS` (500 ms) la cámara publica en `/stats` el campo `foto`: medias de rojo, verde y azul del cuadro y de su mitad de abajo (el piso), y cuántas muestras están quemadas u oscuras. Son solo números, ninguna imagen; es parte del firmware `cam-v2` (`FW_CAMARA`).

### Del lado del teléfono

La visión va en el subsistema F y la app en el D; aquí, solo quién consume el video.

- `SesionVideo.kt` abre UN solo `VideoCarro` y lo comparten las pantallas. Quién abre, comparte, cierra y reintenta se decide en C (`nucleo_c/sesion_compartida.c`): cierra 1.5 s después de salir el último usuario (`SES_GRACIA_CIERRE_MS`) y reabre solo cada 3 s si falló (`SES_REINTENTO_MS`).
- `VideoCarro.kt` pide `http://192.168.4.2/stream` (`URL_DEFECTO`) y abre la conexión por la WiFi que está en `192.168.4.x`, para que el video no se vaya por datos móviles. Si pasan 3 s sin datos (`TIEMPO_LECTURA_MS`) da la conexión por caída y reintenta cada 1 s. Si un modo falla, vuelve solo a jpeg y avisa.
- `VisorJpeg.kt` guarda en una casilla única el JPEG más nuevo, lo decodifica en otro hilo y pinta solo el más reciente, sin modificar la imagen.
- `PaqueteCeldasNativa.kt` lee el paquete `CEL1` con `paquete_leer` (C); la cámara lo arma con `paquete_armar`, del mismo código.
- El navegador (C) manda STOP y queda en ERROR si pasan más de `NAV_MAX_HUECO_VIDEO_MS` = 700 ms sin cuadro, y `ModoSemiautonomo` cancela la ruta a 1,000 ms sin cuadros (`TIMEOUT_CAMARA_MS`). El orden completo de las capas de seguridad va en el subsistema G.

## C.3 Cómo lo hicimos

- **Compresión por software.** El GC0308 no tiene compresor JPEG: si se le pide ese formato, la librería contesta `0x106` (`docs/camara_prueba.md`). Por eso entrega RGB565 y la ESP32 comprime con `frame2jpg` (de la librería esp32-camera). Se escogió RGB565 y no YUV422 porque comprime más rápido (107 contra 125 ms en QVGA) y da JPEG algo más chicos.
- **Dos núcleos y casilla única.** Antes (13-oct-2026) todo ocurría en una sola tarea y la salida era de 6.7 fps en QVGA; con la tubería de dos núcleos subió a 8.6 a 9.1 fps (`docs/camara_prueba.md`). La alternativa descartada es una cola de varios cuadros: con una pausa del WiFi el teléfono recibiría cuadros viejos (`docs/camara_prueba.md`; capítulo 5 del manual, sección 5.2).
- **Reloj de 20 MHz y calidad 50.** A 10 MHz el sensor da la mitad de fps y a 16 MHz QVGA no mejora; 20 MHz es lo máximo que se probó. La calidad 50 baja el JPEG 23 % frente a la 70 sin cambiar el tiempo (107 contra 109 ms); al detector de color le da igual, porque promedia celdas de 10x10 píxeles.
- **HQVGA por defecto.** En el barrido, HQVGA dio 12.4 fps y 137 ms de edad de cuadro contra 8.8 fps y 252 ms de QVGA (`docs/camara_prueba.md`). El código usa HQVGA (`CONFIG_DEFECTO`) y `camara_prueba/README.md` lo declara vigente, pero `docs/camara_prueba.md` todavía dice QVGA: manda el código. CONFIRMAR CON EL EQUIPO: ¿quién decidió pasar de QVGA a HQVGA por defecto y con qué prueba?
- **JPEGENC es una dependencia, no el compresor.** `camara_prueba/src/JPEGENC` es una biblioteca de terceros (BitBank Software, Apache-2.0; licencia en esa carpeta), copiada sin cambios el 14-oct-2026 salvo renombrar `jpegenc.inl` a `jpegenc_inl.h` (`LEEME.txt`). Se incluyó (`COMPRESOR_JPEGENC 1`) para compararla con `frame2jpg` y se activa con `/control?comp=`. Reemplazaría a `frame2jpg` si ganaba 5 ms o más con el mismo color de piso (`docs/latencia.md`); esa tabla está vacía, así que el compresor sigue siendo `frame2jpg`.
- **Modos raw y celdas.** Nacieron para bajar la latencia (Trabajo I, `docs/latencia_modos.md`). El jpeg sigue siendo el predeterminado y, si otro modo falla, la app vuelve sola a jpeg. En celdas el teléfono no decodifica y la red lleva 1,244 B, pero se pierde el giro visual, el modelo de objetos y «Aprender piso». Las celdas usan el mismo código C del detector, así que el resultado es idéntico al de la imagen completa (`nucleo_c/pruebas_celdas.c`).
- **Recuperación.** El 16-oct-2026 la cámara dejó de responder y solo volvió al desconectarla; no se supo si fue un cuelgue o un choque de IP, y se cubrieron las dos causas: el DHCP del DevKit ya no reparte la `.2` y la cámara tiene vigilante (`docs/camara_prueba.md`). Las decisiones están en C puro para probarlas en la PC.
- **Alimentación.** Según el capítulo 2 del manual y `docs/guia_conexion_J15.md`, la cámara lleva una powerbank propia (la «#2») y los motores sus pilas, para que los picos de los motores no reinicien la electrónica. Pero `docs/arquitectura.md` y `docs/operacion.md` hablan de una sola powerbank con dos cables, y `docs/referencia/hardware.md` lo da como pendiente. CONFIRMAR CON EL EQUIPO: ¿en el carro la cámara lleva su propia powerbank o comparte la del DevKit?

## C.4 Cómo lo probamos y con qué resultado

| Qué se probó | Cómo | Resultado | Etiqueta |
|---|---|---|---|
| Barrido de reloj, formato y tamaño (100 cuadros por combinación) | Placa real, sin cliente de video, 13-oct-2026 (`docs/camara_prueba.md`) | HQVGA RGB565 a 20 MHz: 12.4 fps, compresión 56 ms, edad 137 ms. QVGA: 8.8 fps, 107 ms, 252 ms | **NO MEDIDO** en el carro (medido en la placa) |
| Retraso de la imagen hasta la pantalla | Cronómetro frente a la cámara, una sola medida, HQVGA (`docs/latencia.md`) | 320 ms; la meta de 300 ms no se cumple. En una muestra anterior eran unos 530 ms | **NO MEDIDO** en el carro (una medida, sin fecha en el documento) |
| Lógica de recuperación | `pruebas_firmware/pruebas_camara.c`, 34 casos en PC | Documentado como probado, incluida la vuelta de `millis()` a los 49 días; no se corrieron para este capítulo | **SIMULADO** |
| Reintentos de WiFi en la placa real | Cámara cargada con la recuperación y el DevKit apagado, 16-oct-2026 (`docs/bitacora.md`) | Reintentó a los 5, 7, 11, 19 y 29 s. No se provocaron el reinicio a los 60 s ni el vigilante | **NO MEDIDO** en el carro (observado en la placa) |
| La cámara en las pruebas físicas | B4, 27-oct-2026, v1.0 (`docs/reporte_J14.md`) | En 2 de 4 corridas dejó de responder en el puerto 80 aunque el ping seguía; se recuperó apagándola y encendiéndola (~15 s). No dice qué firmware llevaba | **VALIDADO EN EL CARRO** (hecho observado) |
| Paquete `CEL1` y su equivalencia con la imagen | `nucleo_c/pruebas_celdas.c`, 40 casos en PC | Sin diferencias entre analizar la imagen y las celdas (15 secuencias de 14 cuadros); 2,000 paquetes basura rechazados | **SIMULADO** |
| Los tres modos de video | Huawei con cámara falsa (`scripts/camara_falsa.py`), ventanas de 5 s (`docs/latencia_modos.md`) | jpeg 9.3 fps; raw 9.2 fps y 775 KB/s; celdas 10 fps y 12 KB/s (sin sensor ni WiFi del carro) | **SIMULADO** |
| Modos raw y celdas con la cámara real | Compilado (82 % de la memoria de programa) | No se han probado; las 9 pasadas de comparación están vacías | **PENDIENTE** |
| ¿El video estorba al control? (PONG con y sin video) | Carro y cámara falsos por USB (`docs/manual_video.md`) | Diferencia de 1 a 2 ms; la regla apaga el video si pasa de 30 ms. Con el WiFi real falta medir | **SIMULADO** y **PENDIENTE** |
| Esquive con la cámara del carro como fuente de obstáculos | B6, 10-nov-2026, v1.1: recta de 200 cm con caja a unos 100 cm (`docs/estado_20261110.md`, `docs/pruebas_fisicas_20261110.csv`). Ejecutar ruta usa esa cámara (`docs/rutas.md`) | 6 de 6 sin tocar la caja, 5 de 6 completadas. Mide todo el sistema, no la cámara sola | **VALIDADO EN EL CARRO** (v1.1, 10-nov-2026) |
| Compresores, tropiezos con canal fijo, consumo, temperatura | Tablas vacías de `docs/latencia.md`; sin telemetría térmica (`docs/auditoria_autonomo_v2.md`) | Sin medición | **PENDIENTE** y **NO MEDIDO** |

Lo que NO se ha medido: el retraso del video con el carro en marcha, el WiFi real con la cámara en modo raw o celdas, y cualquier cifra de v3 en el carro. El documento de validación del 10-nov-2026 no dice qué versión de firmware llevaba la cámara.

## C.5 Límites conocidos

- **Retraso.** Solo el video cuesta unos 320 ms; pesan más la compresión por software y el reloj del sensor (`docs/latencia.md`). A 19.3 cm/s en AUTO, 100 ms son 1.9 cm, pero esa velocidad puede estar obsoleta: las cajas negras del 15-nov sugieren unos 30 cm/s (`docs/limites_conocidos.md`).
- **Un solo cliente de video.** La app comparte el flujo entre pantallas, pero la cámara no atiende a dos dispositivos.
- **Imagen pequeña y luz.** HQVGA es de 240x176 píxeles. No se escribió ningún registro del sensor: la exposición y el balance de blancos los decide la cámara, y la app no detecta sola un cambio de iluminación (la caja negra guarda números para verlo después, `docs/limites_conocidos.md`). En el simulador, sol, sombras y reflejos detienen un momento al carro en 8 a 13 % de las corridas (**SIMULADO**).
- **Lo que la cámara ve mal.** Las patas delgadas, que el color ve tarde, y una pared clara sobre un piso claro, que no distingue (`docs/limites_conocidos.md`, **SIMULADO**). Por eso la cámara no es la única capa de seguridad (subsistema G).
- **La cámara se ha colgado y su recuperación tiene huecos.** En J14 (27-oct-2026) dejó de responder en el puerto 80 aunque el ping seguía y se recuperó apagándola y encendiéndola (`docs/reporte_J14.md`); no se sabe si el vigilante estaba cargado ni si habría actuado. Además, si el sensor no inicia al arrancar la placa no se reinicia sola, y si la radio queda colgada pero `WiFi.status()` dice «conectado» no se nota (`docs/camara_prueba.md`).
- **Modo celdas.** No tiene giro visual, ni modelo de objetos, ni «Aprender piso» (`docs/latencia_modos.md`).
- **Capítulo 5 del manual.** Trae datos que no coinciden con el código (puerto del `/stream`, núcleos, compresor, cabeceras de `/raw`, paquete y constantes de recuperación). Aquí se usan los del código.

## C.6 Términos clave

- **GC0308**: sensor de imagen de 0.3 megapíxeles de la Freenove; no comprime JPEG.
- **RGB565**: formato sin comprimir de 2 bytes por píxel que entrega el sensor.
- **HQVGA**: tamaño de 240x176 píxeles, el que usa la cámara por defecto.
- **`frame2jpg`**: función de la librería esp32-camera que comprime a JPEG por software; es el compresor de siempre.
- **JPEGENC**: biblioteca de terceros (Apache-2.0) incluida solo para comparar compresores.
- **MJPEG (`/stream`)**: video como una conexión HTTP que manda un JPEG tras otro.
- **casilla única** (buzón del último cuadro): lugar donde cabe un solo cuadro; si llega otro, el viejo se tira.
- **edad del cuadro**: ms desde que empezó la captura hasta que termina su envío.
- **paquete `CEL1`**: paquete UDP de 1,244 B con el promedio RGB de cada celda de 10x10 píxeles.
- **vigilante** (`recuperacion.c`): tarea que reinicia la placa si el WiFi, el servidor o la captura se atoran.
- **`SesionVideo`**: objeto de la app que abre un solo video y lo comparte entre pantallas.
- **`CAMERA_GRAB_LATEST`**: modo del driver que siempre entrega el cuadro más nuevo.

## C.7 Preguntas probables del profesor

1. **¿Por qué comprimen el video en la ESP32 y no usan una cámara con JPEG?** El GC0308 no tiene compresor: con formato JPEG la librería contesta el error `0x106`. Por eso entrega RGB565 y la ESP32 comprime por software con `frame2jpg`, que tarda 56 ms por cuadro en HQVGA. Una cámara con JPEG por hardware quitaría ese límite (`docs/camara_prueba.md`), pero no la probamos.
2. **¿Cuánto retraso tiene el video y qué significa para el carro?** Medimos 320 ms con cronómetro, una sola vez, en HQVGA; la meta era 300 ms y no se cumple. La cuenta de toda la cadena, con la calibración, da unos 8.5 cm hasta quedar detenido (`docs/latencia.md`); es una cuenta, no una medida con cinta, y no la medimos con el carro en marcha.
3. **¿Cómo evitan que se acumule retraso?** La cámara guarda solo el último cuadro y tira el viejo; corta al cliente que no recibe en 1.5 s; y en el teléfono hay una cola de uno entre cada etapa (`VideoCarro`, `VisorJpeg`), así que solo se pinta el cuadro más reciente.
4. **¿Qué pasa si la cámara deja de mandar imágenes con el carro avanzando?** El navegador manda STOP si pasan más de 700 ms sin cuadro y queda en ERROR sin reanudar solo; la app cancela la ruta a 1,000 ms. La cámara, si perdió el WiFi, reintenta y a los 60 s se reinicia; si se atora la captura, se reinicia a los 5 s con demanda de video. Esa lógica está probada en PC (**SIMULADO**); en el carro no se ha probado una caída a media ruta (**NO MEDIDO**).
5. **¿Qué es el modo celdas y lo usan?** Es un modo donde la cámara manda por UDP solo el promedio de color de cada celda de 10x10 píxeles, 1,244 bytes por cuadro. No es el predeterminado (es jpeg): lo probamos solo con una cámara falsa (**SIMULADO**) y con la real falta (**PENDIENTE**).

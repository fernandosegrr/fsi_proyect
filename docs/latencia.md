# Latencia: de la cámara al STOP, y cuánto avanza el carro

Meta del proyecto: **menos de 300 ms** desde que el obstáculo aparece en la cámara hasta que el
carro recibe el STOP. Aquí se suma toda la cadena con lo medido y se dice qué es suposición.

**Velocidad del carro en el perfil AUTO (210): 19.3 cm/s** (calibración real: 51.7 ms por cm) y **1.4 cm de frenado** después del STOP
(`docs/calibracion.md`, medido en el carro el 14 oct 2026, leído de `calibracion.txt` del teléfono). A esa velocidad, **cada 100 ms de
retraso son 1.9 cm**. La cuenta es: `cm = retraso_ms / 51.7 + 1.4` (hecha en `nucleo_c/prueba_obstaculo.c`, con pruebas en PC).

Antes (perfil a 255, 13 oct): 38 cm/s (26.3 ms por cm) y 0.9 cm de frenado, `cm = retraso_ms / 26.3 + 0.9`. Las dos columnas de las cuentas de
abajo muestran el antes y el después.

## La cadena, etapa por etapa (medida)

1. **Cámara (Freenove, GC0308) hasta que el cuadro llega al teléfono: 320 ms.** Medido con cronómetro en HQVGA 240x176
   (~12 fps). Incluye la exposición y lectura del sensor (~100 ms), la compresión JPEG por software en la ESP32 (edad en la
   placa ~135 ms en HQVGA), el WiFi y la decodificación en el teléfono. Es una sola medida; no se sabe cómo se reparte.
   Con la cámara del teléfono este tiempo es ~0.
2. **Convertir y detectar en el teléfono: ~7 ms** (medido en el Huawei; con el video del carro la decodificación del JPEG sube
   la conversión a 4 a 10 ms). El modelo de objetos (45 a 70 ms) no entra en la cadena de seguridad: el detector de color manda.
3. **Antirrebote del navegador** (`MS_CONFIRMAR_LEJOS` = 150 ms):
   - obstáculo cerca (cercanía 3 en cualquier zona, o 2 en el centro): **0 ms**, se detiene con el primer cuadro;
   - obstáculo de cercanía 2 a un lado: hay que verlo 150 ms y en 2 cuadros como mínimo. Con cuadros cada 80 ms se confirma al
     tercero (160 ms después del primero); cada 120 ms, 240 ms después (probado en `pruebas_navegador.c`).
4. **Tick del navegador (50 ms) y envío del STOP por WebSocket.** El tick espera entre 0 y 50 ms (25 ms en promedio). El ida y vuelta
   PING/PONG por WiFi con el carro real se midió en la app: **30 ms** (el STOP tarda la mitad, ~15 ms, en llegar).
5. **Firmware: del STOP a los motores a cero**: en la misma vuelta del `loop` (`carro_detener`), unos milisegundos. No medido.
6. **Frenado mecánico: 1.4 cm a 210** (0.9 cm a 255; medido en la calibración).

## Suma y distancia recorrida: a 210 (51.7 ms/cm, frenado 1.4 cm) y, para comparar, a 255 (26.3 ms/cm, frenado 0.9 cm)

| Caso | Retraso | cm a 210 (AUTO) | cm a 255 (antes) |
|---|---|---|---|
| Obstáculo cerca (cercanía 3, o 2 al centro), tick promedio | 367 ms | **8.5** | 14.9 |
| Lo mismo, peor caso del tick (+25 ms) | 392 ms | **9.0** | 15.8 |
| Cercanía 2 a un lado, cuadros cada 80 ms (+160 ms) | 527 ms | **11.6** | 20.9 |
| Cercanía 2 a un lado, cuadros cada 120 ms (+240 ms) | 607 ms | **13.1** | 24.0 |

Cuentas: obstáculo cerca = 320 (video) + 7 (detectar) + 0 (antirrebote) + 25 (tick, promedio) + 15 (STOP por WiFi) = **367 ms**; a 210 el carro
avanza 367 / 51.7 + 1.4 = **8.5 cm** antes de quedar detenido (a 255: 367 / 26.3 + 0.9 = 14.9 cm). Son cuentas con la calibración, no una
medida de cm con cinta: **la prueba con el termo a 210 «frenó bien»**, pero falta la serie de pasadas con cinta (PENDIENTE HARDWARE).
OJO: el retraso (367 ms) se midió a 255; el STOP y el video no dependen de la velocidad, así que se supone el mismo.

- Con «frenar desde lejos en el centro» (video del carro) el STOP sale con cercanía 1 (2 cuadros y 60 ms), antes que la cercanía 2: más margen todavía.
- La meta de 300 ms **no se cumple** con esta cámara: solo el video ya son 320 ms.

Lo que sí se puede hacer:

- bajar la velocidad: la distancia es `velocidad × retraso` + frenado: a 19 cm/s (perfil AUTO, **ya hecho**) son ~8.5 cm; a 38 cm/s eran ~15 cm;
- elegir `OBS_CERCA_MIL` y la inclinación de la cámara para que la **cercanía 3 empiece al menos a ~14 cm** del carro (9.0 cm de la
  cadena a 210 + 5 cm de margen; eran ~21 cm a 255). Hay que medir con una cinta a cuántos cm del carro queda la fila donde empieza la cercanía 3;
- atacar la cámara: la compresión por software y el reloj del sensor son lo que más pesa (`docs/camara_prueba.md`).

## Vigilancia del video (fail-safe)

Si la cámara de obstáculos pasa más de **`NAV_MAX_HUECO_VIDEO_MS` = 500 ms** sin mandar un cuadro (ya habiendo mandado alguno en la ruta), el
navegador manda STOP y queda en ERROR por falta de video. **No reanuda solo cuando el video vuelve**: el carro se queda detenido hasta que
se inicie otra ruta (`pruebas_navegador.c`: un hueco de 300 ms no frena, uno de ~800 ms sí, y con el video de vuelta 5 s después sigue en
STOP). Con la fuente de botones, o si nunca llegó un cuadro, no se vigila (no hay video que perder). La app además cancela la ruta si pasan
1000 ms sin cuadros (respaldo en `ModoSemiautonomo`).

## Los tropiezos del video

La pantalla de visión y el log `VIDEO_STATS` (cada 5 s) dan, para los últimos 10 s: **fps reales, el hueco máximo entre cuadros, cuántos
huecos pasaron de 500 ms, el hueco que está pasando ahora y por qué red va el video** (`VideoCarro.kt`, cuentas en `nucleo_c/video_stats.c`).
Con el firmware nuevo, `/stats` de la cámara también da `reconexiones` (veces que se cayó su WiFi) y `rssi_min_10s`. La app las lee al abrir el
video y en cada reintento (con el video abierto la cámara no atiende otra petición). Medidas para quitar los tropiezos: canal fijo del punto de
acceso (`WIFI_CANAL` 1) y `WiFi.setSleep(false)` en las dos placas.

## Prueba "Obstáculos: cámara" (`docs/prueba_obstaculos.md`)

Cada pasada escribe en el log (etiqueta `OBST_PRUEBA`): `marca_cuadro_ms` (hora en que llegó al teléfono el primer cuadro con el obstáculo
cerca), `ms_detectar` (de la llegada del cuadro a que el detector terminó), `ms_stop` (de la llegada de ese cuadro a que salió el STOP),
`ms_respuesta` (del STOP al PONG de un PING mandado justo después), `cercania`, `zona`, `fuente` (detector, modelo o los dos),
`retraso_total_ms` = video + ms_stop + ms_respuesta / 2, y `cm_recorridos` con la calibración. Al terminar la serie:
`RESUMEN pasadas=… retraso_promedio_ms=… cm_promedio=…`.

## Qué falta medir (PENDIENTE HARDWARE)

- La serie de pasadas de la prueba de obstáculos con el carro real (pasos en `docs/prueba_obstaculos.md`).
- Si los tropiezos del video desaparecen con el canal fijo y sin ahorro de energía (cargar los dos firmwares nuevos: `scripts\cargar.ps1 nuevo` y `camara`).
- A cuántos cm del carro queda cada cercanía.

## Valores de calibración usados (resumen)

| | 255 (13 oct) | 210 (14 oct, AUTO) |
|---|---|---|
| ms por cm | 26.3 | 51.7 |
| cm/s | 38 | 19.3 |
| frenado | 0.9 cm | 1.4 cm |
| ms por cm hacia atrás | sin medir | 44.4 (22.5 cm/s) |

La calibración está en `docs/calibracion.md`. La app usa 51.7 ms/cm y 1.4 cm si no puede leer la calibración guardada.

## Trabajo D: pasos para bajar la latencia del video (antes y después)

Regla del trabajo: se mide antes y después de cada paso; **si no gana 5 ms, se descarta**. Lo que no se pudo medir sin las placas queda como PENDIENTE HARDWARE con los pasos para medirlo; los números de «antes» son los ya medidos en este documento y en `docs/camara_prueba.md`.

| Paso | Qué se hizo (en el código) | Antes (medido) | Después | Estado |
|---|---|---|---|---|
| 1. Compresión JPEG en la cámara | Registro `JPEG_MS` (línea cada 5 s por el puerto serie: compresor, cuadros, ms medio/mín/máx, tamaño) y comparación al arrancar de **4 compresores** sobre cuadros reales: `frame2jpg` (jpge de esp32-camera, el de siempre), JPEGENC 4:2:0 calidad media, 4:4:4 y 4:2:0 calidad baja (`camara_prueba/src/JPEGENC`, **Apache-2.0**, copiada tal cual de bitbank2/JPEGENC, hashes en `LEEME.txt`). `/control?comp=esp`, `420`, `444` o `baja` cambia el compresor en vivo (vuelve a `frame2jpg` al reiniciar). | `frame2jpg` calidad 50: 105 a 107 ms por cuadro en QVGA (`docs/camara_prueba.md`); en HQVGA (medido al arrancar la cámara cargada con el firmware nuevo, 14 oct): **54.2 ms** por cuadro, JPEG medio 2386 B, 12.4 fps, edad hasta el fin de la compresión 135 ms | **PENDIENTE HARDWARE** | Compila; falta correr en la cámara |
| 2. Cuadro más reciente sin acumulación | Ya estaba: casilla única entre captura y compresión (el cuadro viejo se tira y se cuenta en `descartados`), `CAMERA_GRAB_LATEST`, `/stats` con `edad_ms` y `edad_max_ms` (edad del cuadro). Nuevo en la app: cola de 1 también entre las etapas del teléfono (paso 4) y el hilo de lectura deja siempre vacío el búfer del socket. | edad ~135 ms en HQVGA (etapa 1 de este documento) | sin cambio en la cámara | Confirmado en el código; no había nada que ganar |
| 3. `TCP_NODELAY`, `setSleep(false)`, canal fijo | Cámara: `TCP_NODELAY` en el socket del stream (ya estaba), `WiFi.setSleep(false)` (ya estaba) y canal del punto de acceso fijo (`WIFI_CANAL` 1, ya estaba). **Nuevo:** `TCP_NODELAY` también en el WebSocket del carro (`client->client()->setNoDelay(true)` al conectar, `firmware_carro.ino`) para que el STOP no espere al algoritmo de Nagle. | PING/PONG por WiFi medido: 30 ms (etapa 4 de este documento) | **PENDIENTE HARDWARE**: repetir el PING/PONG de la app con el firmware nuevo | Compila |
| 4. `VideoCarro` en hilos | La cadena del video del carro ahora son **3 hilos** (lectura del socket → decodificación → análisis) con **cola de 1** entre cada par (el cuadro viejo se tira) y objetos reutilizados (JPEG, mapa de bits `inBitmap` y arreglos de pixeles). Registro `MODO_STATS` cada 5 s con fps, hueco máximo, huecos > 300 ms, **ms de llegada a decisión**, descartados y bytes/s. | Una sola función hacía leer + decodificar + analizar: si el análisis tardaba, los cuadros se acumulaban en el búfer del socket (retraso que crece) | Con la cámara FALSA (sin cámara real): llegada→decisión 10 a 12 ms, 0 descartados a ~9 fps (`docs/latencia_modos.md`, sección 3) | La mejora real (menos retraso con una red lenta) **PENDIENTE HARDWARE**: comparar `MODO_STATS` con el carro |
| 5. Antes y después | Esta tabla. | | | La columna «Después» se llena con las pasadas de `docs/latencia_modos.md` |

### Cómo medir los pasos 1 y 3 (PENDIENTE HARDWARE)

1. Cargar la cámara (`scripts\cargar.ps1 camara`). Al arrancar, el puerto serie (monitor de `cargar.ps1`) escribe las líneas `JPEG_MS comparacion comp=... compresion=... ms jpeg_medio=... B fps=... edad_hasta_fin_de_compresion=... ms` de los 4 compresores. Anotar cada una aquí.
2. Criterio: un compresor reemplaza a `frame2jpg` solo si (a) gana **al menos 5 ms** de compresión (o de edad) y (b) la app decodifica bien sus JPEG y **el color del piso que mide el detector es el mismo** (`Piso (RGB)` en la pantalla de visión, ±3 niveles: JPEGENC lee RGB565 con el orden de bytes de la ESP32 y se supuso «primero el byte alto»; si los colores salen cambiados el supuesto es el contrario). Mientras tanto el compresor predeterminado **sigue siendo `frame2jpg`**.
3. Con el compresor elegido: `http://192.168.4.2/control?comp=420` (por ejemplo), abrir el video en la app 1 minuto y leer `JPEG_MS` y `MODO_STATS`.
4. PING/PONG del WebSocket: «Iniciar» una ruta con el firmware nuevo y comparar el `ms_respuesta` de `OBST_PRUEBA` con los ~30 ms de antes.

| Compresor | Compresión (ms) HQVGA | JPEG medio (B) | fps | Edad hasta fin de compresión (ms) | ¿La app lo decodifica? | ¿Mismo color de piso? | ¿Gana ≥ 5 ms? |
|---|---|---|---|---|---|---|---|
| frame2jpg q50 | | | | | sí | sí | — |
| JPEGENC 4:2:0 media | | | | | | | |
| JPEGENC 4:4:4 media | | | | | | | |
| JPEGENC 4:2:0 baja | | | | | | | |

# Latencia: de la cámara al STOP, y cuánto avanza el carro

Meta del proyecto: **menos de 300 ms** desde que el obstáculo aparece en la cámara hasta que el
carro recibe el STOP. Aquí se suma toda la cadena con lo medido y se dice qué es suposición.

**Velocidad del carro: 38 cm/s** (calibración real: 26.3 ms por cm) y **0.9 cm de frenado** después del STOP
(`docs/calibracion.md`, medido en el carro el 13 oct 2026). A esa velocidad, **cada 100 ms de retraso son 3.8 cm**.
La cuenta es: `cm = retraso_ms / 26.3 + 0.9` (hecha en `nucleo_c/prueba_obstaculo.c`, con pruebas en PC).

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
6. **Frenado mecánico: 0.9 cm** (medido en la calibración).

## Suma y distancia recorrida a 38 cm/s (26.3 ms/cm, frenado 0.9 cm)

- **Obstáculo cerca (el caso que importa para frenar)**: 320 + 7 + 0 + 25 (tick, promedio) + 15 (STOP por WiFi) = **367 ms**
  → el carro avanza 367 / 26.3 + 0.9 = **14.9 cm** antes de quedar detenido. En el peor caso del tick (+25 ms): 392 ms → **15.8 cm**.
- **Obstáculo de cercanía 2 a un lado** (cuadros cada 80 ms): 367 + 160 = **527 ms** → **20.9 cm**.
  Con cuadros cada 120 ms: 367 + 240 = 607 ms → **24.0 cm**. Como es lejano, hay margen.
- La meta de 300 ms **no se cumple** con esta cámara: solo el video ya son 320 ms.

Lo que sí se puede hacer:

- bajar la velocidad: la distancia es `velocidad × retraso` + frenado: a 19 cm/s serían ~8 cm; a 38 cm/s, ~15 cm;
- elegir `OBS_CERCA_MIL` y la inclinación de la cámara para que la **cercanía 3 empiece al menos a ~21 cm** del carro (15.8 cm de la
  cadena + 5 cm de margen). Hay que medir con una cinta a cuántos cm del carro queda la fila donde empieza la cercanía 3;
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

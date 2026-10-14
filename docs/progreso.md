# Progreso

## Parte A: firmware en C (lista aprobada)
- Hecho: `firmware_carro/` (firmware_carro.ino, carro.c/.h, config.h, pagina.h idéntico por SHA-256), `docs/` y `pruebas_firmware/`.
- Compilación: `arduino-cli compile --fqbn esp32:esp32:esp32 firmware_carro` sin errores (73 % de flash, 14 % de RAM).
- Pruebas de PC: 44 casos, 0 fallos, incluida la comparación con el original (2 × 300 000 pasos, 0 diferencias) y el cruce del desbordamiento de millis.
- Sin probar en hardware.
- Pendiente: TRABAJOS 1-3 de la parte C (el mensaje llegó cortado).

## TRABAJO 2: módulo de trayectoria (nucleo_c)
- Hecho: `nucleo_c/` con trayectoria.h/.c, calibracion.h, pruebas_trayectoria.c y Makefile. C99 puro, enteros, sin malloc ni math.h; el header se incluye también desde C++.
- Pruebas: `make test` en nucleo_c → 20 casos, 0 fallos (recta, derecha, izquierda, U, un punto, diagonales, giros de 180 y -135, errores y excesos de MAX_PUNTOS / MAX_PASOS).
- Decisiones: ver bitácora (diagonal calculada sobre el total, +180 para media vuelta, MAX_PASOS = 100).
- Pendiente: medir MS_POR_CM y MS_POR_GRADO con el carro (valores actuales son de ejemplo).

## TRABAJO 3: cierre
- Hecho: `README.md` en la raíz (carpetas, cómo correr pruebas y compilar, orden de lectura).
- Verificación final: pruebas de nucleo_c (20/20) y pruebas_firmware (44/44) pasan; el firmware compila (73 % flash); `pagina.h` sigue idéntico al original; `original_backup/` sin cambios.

## TRABAJO 4: ajuste de MAX_PASOS
- Hecho: `MAX_PASOS = 2 * MAX_PUNTOS` en trayectoria.h; pruebas actualizadas con un zigzag de 64 puntos (126 pasos, antes error).
- `make test` en nucleo_c: 23 casos, 0 fallos. Razón en la bitácora.

## TRABAJO 5: esqueleto de la app Android (app_android)
- Hecho: proyecto Gradle (AGP 9.4.1, Gradle 9.8.1), una Activity clásica en Kotlin, `CuadriculaView` (12x8, dedo → celdas de 8 vecinos, Borrar), botón Calcular, y NDK/CMake que compila `puente.c` + `../nucleo_c/trayectoria.c` sin copiarlo.
- `gradlew assembleDebug`: BUILD SUCCESSFUL; el APK trae `libcarro_nativo.so` para arm64-v8a, armeabi-v7a, x86 y x86_64, con las funciones JNI exportadas.
- Puente C probado en PC con un JNIEnv falso (rutas válidas, salto, vacío, impar y límites).
- Guía: `docs/instalar_app_huawei.md`. Sin probar en teléfono.

## TRABAJO 6: publicar en GitHub
- Hecho: pruebas y compilación OK, WiFi movido a `secretos.h` (ignorado) con `secretos.ejemplo.h`, `original_backup/` fuera de git, historial local limpio y verificado, remoto vacío confirmado.
- Detenido antes del push: los commits llevan un correo personal de Gmail; esperar decisión (ver `docs/resumen_sesion.md`). [RESUELTO: ver "TRABAJO 6 (cierre)" más abajo.]

## TRABAJO 7: probar en el teléfono
- `adb devices` muestra el teléfono como `unauthorized`; no se instaló. Pasos para autorizarlo en `docs/resumen_sesion.md`.

## TRABAJO 7 (repetido): probar en el teléfono
- Hecho: instalación con `adb install -r` (Success), la app abre y carga, y una ruta dibujada y calculada en el Huawei da AVANZAR 60 cm, GIRAR +90, AVANZAR 80 cm (7900 ms). Sin errores en logcat.

## TRABAJO 6 (continuación)
- Elegida la opción (b). `gh` no está instalado: falta el usuario de GitHub para armar el correo noreply y reescribir autores. Sin push todavía.

## TRABAJO 6 (cierre): publicado
- Hecho: autores reescritos al correo noreply, historial verificado, `git push -u origin main` sin `--force` aceptado y comprobado con `git ls-remote origin`. Repositorio: https://github.com/fernandosegrr/fsi_proyect

## TRABAJO 8: benchmark del detector en el Huawei
- Hecho: `BenchmarkActivity` (botón en la pantalla principal y `--ez bench_auto true` por adb), detector EfficientDet-Lite0 con TensorFlow Lite, mediciones por etapa y reporte en `docs/benchmark_ELS-NX9.md`.
- Resultado: la mejor configuración es CPU con 1 hilo: 45.4 ms por cuadro con JPEG 640x480 (43.4 ms con 320x240), unos 22 cuadros por segundo; quedan ~255 ms de los 300 ms para captura, WiFi y comando. GPU 149 ms y NNAPI 63 ms por cuadro.

## TRABAJO 9: pantalla de control y servidor falso
- Hecho: `pruebas_firmware/servidor_falso.py` (+ prueba propia de 9 casos) y, en la app, `ControlActivity` con OkHttp WebSocket, botones con heartbeat de 250 ms, PING cada 800 ms, reintento cada 2 s, estado y latencia; lógica separada en `ControlLogica` y `Enlace`.
- Probado en el Huawei con `adb reverse`: conexión, comandos al presionar (cada 251 ms de media), STOP al soltar, PING/PONG (latencia por USB: media 7,6 ms, mediana 7, p95 9, 75 muestras), reconexión y STOP al ir a segundo plano. Detalle en `docs/pruebas_control.md`.
- Pendiente con el carro: `bindProcessToNetwork`, Wi-Fi+ del Huawei y `ws://192.168.4.1/ws` real.

## TRABAJO 10: detector de obstáculos por color de piso
- Hecho: `nucleo_c/obstaculos.h/.c` (C estándar, solo enteros), `pruebas_obstaculos.c` (32 casos, todos pasan; `make test` corre también las de trayectoria), puente JNI (`ObstaculosNativa`), benchmark en el Huawei y `docs/obstaculos.md`.
- Huawei: las 4 imágenes generadas dan el resultado esperado; un cuadro completo (decodificar + convertir + analizar) tarda 1.36 ms de media (1.24 a 1.60 ms, p95 hasta 2.03 ms); solo el análisis en C, 0.59 ms. Unas 30 veces más rápido que el modelo (43.4 ms).
- Pendiente: probar con la cámara real y una foto real de pasillo, calibrar umbrales, y decidir cómo combinarlo con el modelo y con el control del carro.

## TRABAJO 11: navegador del modo semiautónomo
- Hecho: `nucleo_c/navegador.c/.h` (máquina de estados con los 7 estados pedidos), `simulador_navegador.c/.h` y `pruebas_navegador.c` (41 casos, todos pasan; con trayectoria 28 y obstáculos 32 son 101 en `nucleo_c`), `trayectoria_calcular_desde`, puente JNI (`NavegadorNativa`), `ModoSemiautonomo` y modo "Semiautónomo" en `ControlActivity` (cuadrado y zigzag, Iniciar/Cancelar, obstáculos simulados).
- Huawei + servidor falso: cuadrado y zigzag completos, esquivar (FORWARD, STOP, LEFT, FORWARD, RIGHT, continúa), cancelar, segundo plano y caída del servidor mandan/mantienen STOP. Retraso obstáculo→STOP: mediana 128 ms en la app y 131 ms en el servidor (107 a 142 ms).
- Pendiente: carro real (calibración, deriva), WiFi real, cámara real conectada al detector y al navegador. Detalle en `docs/navegador.md`.

## TRABAJO 12: cámara del Huawei como fuente de obstáculos y ajuste de umbrales en vivo
- Hecho: `ConfigObstaculos` en C (51 pruebas del detector; 120 en `nucleo_c` con trayectoria 28 y navegador 41), JNI `analizarConfig`, `CamaraObstaculos` (Camera2), `VistaDeteccion` y `VisionActivity` (superposición, fps/ms, barras con SharedPreferences, guardar cuadro, permiso en ejecución), interruptor "Obstáculos: botones / cámara" en el modo semiautónomo.
- Medido en el Huawei (60 s): 24.9 fps (el ritmo de la cámara) y 6.85 ms por cuadro de media (p95 8.18 ms) entre conversión y análisis; sin errores. Al ir a segundo plano la cámara se cierra y se envía STOP.
- Pendiente: la prueba física sobre un piso real (procedimiento de 7 pasos en `docs/vision_huawei.md`).

## TRABAJO 13: ignorar filas lejanas (FILA_MINIMA)
- Hecho: campo `fila_minima` en `ConfigObstaculos` (por defecto 12 = mitad inferior, mismo comportamiento), `OBS_ERR_CONFIG` si está fuera de 0 a 23; la cercanía sigue saliendo de la fila más baja. 20 pruebas nuevas (71 del detector; 140 en `nucleo_c` con trayectoria 28 y navegador 41). JNI con 7 enteros, tercera barra en `VisionActivity` (guardada en SharedPreferences y usada también por el modo semiautónomo), filas ignoradas en gris y línea blanca en el límite.
- Huawei, 33 s seguidos sin errores: 25.0 fps y 6.50 ms por cuadro (antes 24.9 y 6.85), así que no empeoró.
- Pendiente: ajustar inclinación y FILA_MINIMA con el cuarto real (paso 2 de `docs/vision_huawei.md`).

## PARTE 1: adaptar todo a la cámara real en HQVGA 240x176
- Hecho: cámara Freenove arranca en HQVGA (QVGA y QQVGA siguen con `/control?size=`); el detector acepta cualquier tamaño de 30x30 a 640x480 (celdas de 10x10, franja de referencia y fila mínima como proporciones en milésimas, aviso de rejilla chica); el navegador confirma en milisegundos (`MS_CONFIRMAR_LEJOS` 150, inmediato con cercanía 2 o 3 al centro); `VisionActivity` y `VistaDeteccion` usan el tamaño de cada cuadro; `docs/latencia.md`.
- Pruebas en PC: detector 144 casos (los 71 anteriores adaptados + 67 por tamaño 320x240, 240x176, 160x120 y tamaños raros), navegador 55 (con cuadros cada 50, 80 y 120 ms), trayectoria 28; todas pasan. La app compila (`assembleDebug`).
- Pendiente (hardware): cargar el firmware de la cámara con HQVGA por defecto y medir el retraso con cronómetro; probar la app en el teléfono; medir PING/PONG por WiFi.

## PARTE 2: calibración experimental del carro (sin encoders)
- Hecho: `nucleo_c/calibracion.c/.h` (promedio, mediana, promedio sin atípicos, ajuste por mínimos cuadrados de avance y de cada giro, conversiones cm→ms y grados→ms con arranque, frenado y lado, texto exportable); el navegador usa la calibración medida (con arranque, frenado y tramos continuos); en la app `CalibracionActivity` (asistente: reacción, arranque, avance, giro izquierdo y derecho, resumen, «usar estos valores», copiar y borrar), `CalibracionGuardada` y aviso en el modo semiautónomo; `docs/calibracion.md`.
- Pruebas en PC: calibración 92 casos (incluye 400 simulaciones con ruido de ±10 %), navegador 70, trayectoria 28, obstáculos 144; todas pasan. La app compila (`assembleDebug`).
- PENDIENTE HARDWARE: hacer el procedimiento con el carro real y medir la desviación de una ruta calibrada.

## PARTE 3: piso de varios colores (baldosas, vetas, alfombras)
- Hecho: el detector aprende hasta K = 4 grupos de color del piso (k-medias corto sobre la franja), una celda es piso si se parece a cualquiera (o a una mezcla entre dos); aprendizaje continuo seguro (solo con el carro avanzando y sin obstáculos, nunca con celdas de obstáculo, vecinas ni dudosas); reaprendizaje si todo queda lejos 1 s; la franja tapada ya no da error si el modelo ya aprendió el piso; señal de bordes como interruptor (apagada: empeora escenas sin objetos); `ConfigObstaculos` con K y velocidad de aprendizaje; en la app, barras de K y aprendizaje y aviso "piso nuevo aprendido"; `docs/obstaculos.md` actualizado. Además se revisó la regla de frenado del navegador (ver bitácora).
- Pruebas en PC: piso 57 casos nuevos (`pruebas_piso.c`), detector 144 (los anteriores, sin cambios), navegador 72, calibración 92, trayectoria 28; todas pasan. La app compila.
- PENDIENTE HARDWARE: probar con el piso real de baldosas/alfombra y ajustar K, umbrales y aprendizaje.

## PARTE 4: detector de color + modelo de objetos (fusión)
- Hecho: `nucleo_c/fusion.c/.h` (el detector manda en seguridad; el modelo sube lo que el detector duda; objeto lejano solo avisa; resultado del modelo caduca a 300 ms; caja → zona y cercanía; clases de interés configurables), `ModeloEnHilo` (el modelo en su propio hilo, máx. 10 por segundo, sin bloquear al detector), `FusionNativa`, integración en la cámara y en la pantalla de visión (quién decidió, ms de cada etapa, cajas del modelo), `docs/fusion.md`.
- Pruebas en PC: fusión 67 casos (modelo caducado, contradicciones, STOP inmediato por detector, línea de tiempo con cuadros cada 80 ms y modelo cada 120 ms), piso 57, detector 144, navegador 72, calibración 92, trayectoria 28; todas pasan. La app compila.
- PENDIENTE HARDWARE: instalar la app, encender el modelo en la pantalla de visión y medir en el Huawei las etapas, los fps y la temperatura; probar con objetos reales (docs/fusion.md).

## PARTE 5: giro medido con la cámara
- Hecho: `nucleo_c/movimiento_visual.c/.h` (huella de columnas de la mitad de arriba, corrimiento con subpíxel, rango de al menos un tercio del ancho, "sin confianza" si la escena es lisa, ambigua o débil), campo de visión en `CalibracionCarro` (`cal_ajustar_fov`), el navegador corta el giro cuando la cámara ve el ángulo (tiempo de respaldo, cuadros del arranque ignorados, compensación del retraso de la cámara), JNI y app (giro visual en la pantalla de visión, paso 6 opcional en la calibración), `docs/giro_visual.md`.
- Pruebas en PC: movimiento visual 60 casos (corrimientos de 1 a 80 px con y sin ruido, escena uniforme sin confianza, giro de 90° acumulado), navegador 96 (carro simulado 20 % lento: 72° sin cámara y 92° con cámara; con 320 ms de retraso), calibración 110, fusión 67, piso 57, detector 144, trayectoria 28; todas pasan. La app compila.
- PENDIENTE HARDWARE: probar el giro visual con el teléfono girado a mano, hacer el paso 6 de la calibración con el carro, medir el retraso real y poner `ponerLatenciaVisual` cuando la app reciba el video de la Freenove.

## EXTRA: firmware nuevo
- Hecho: `firmware_carro` ya no da el aviso "TWDT already initialized" (reconfigura el vigilante que el núcleo ya inició) y contesta `PONG` a `PING`. Pruebas de PC (`pruebas_firmware`): 49 casos, 0 fallos (44 anteriores + 5 de PONG); compila para la DevKit.
- PENDIENTE HARDWARE: cargar con `scripts/cargar.ps1 nuevo` y comprobar el arranque (sin el error, con "Watchdog de hardware: 5000 ms") y la latencia PING/PONG en la pantalla de control.

## PRUEBAS CON EL HARDWARE (Huawei ELS-NX9 + DevKit + Freenove, 13 oct 2026)
- Hecho y comprobado en la placa/teléfono: firmware nuevo del DevKit (sin "TWDT already initialized", watchdog 5000 ms, PONG con 30 ms de latencia desde la app); firmware de la cámara (HQVGA 240x176, 12 fps); control manual; calibración real del carro (arranque 0 ms, 26.3 ms/cm, giros 7.8 y 7.4 ms/°); giro visual con la cámara del teléfono (campo de visión 79° calculado con focal y sensor, no 60°); la app lee el video MJPEG de la cámara del carro y lo analiza (botón «Fuente del video»); pausa de 300 ms entre avanzar y girar en el navegador; carro RECTO: 3 cm de desvío en 150 cm tras ajustar `VELOCIDAD_IZQ` = 252 y arrancar 250 ms a plena potencia.
- Cuadrado de 40 cm con el carro: repetible en el avance; el giro (pivote) varía entre 90° y 120° según la esquina, por eso una corrida sale casi perfecta y otra no. Decisión del usuario: se deja así (pocas esquinas seguidas son aceptables). Opciones si se necesita más precisión: giroscopio MPU6050 en el DevKit, o giros lentos para que la cámara del carro los corrija.
- PENDIENTE: giro visual con la cámara del carro (el pivote es demasiado rápido para su ~12 cuadros por segundo y ~320 ms de retraso; el campo de visión de la Freenove sigue sin medir); modelo de objetos con objetos reales (dio un falso «refrigerator 43 %» sobre una pared lisa); temperatura del teléfono con el modelo encendido; los tropiezos de 1 a 2 s del video del carro.

## TRABAJO: estabilidad del video y prueba de obstáculos con la cámara del carro
- Hecho: canal WiFi fijo y sin ahorro de energía en las dos placas; `/stats` de la cámara con reconexiones y señal mínima; estadística del video (`video_stats.c`, log `VIDEO_STATS`, pantalla de visión) y aviso si el video no va por la red del carro; vigilancia del video en el navegador (hueco mayor a 500 ms → STOP y ERROR, no reanuda solo); instrumentación `OBST_PRUEBA` y ruta «Prueba de obstáculo (1 m)»; `docs/latencia.md` con 38 cm/s y 0.9 cm; confianza mínima del modelo en 60 %.
- Pruebas en PC: 10 suites, todas sin fallos (video 14, prueba de obstáculo 16, navegador 105, fusión 72, firmware 56). App compilada e instalada.
- PENDIENTE HARDWARE: cargar `scripts\cargar.ps1 nuevo` y `camara`; la serie de pasadas con un objeto real (`docs/prueba_obstaculos.md`); ver si los tropiezos del video desaparecen.

## ESTADO DE LA PRUEBA DE OBSTÁCULOS CON LA CÁMARA DEL CARRO (13 oct 2026)
- Video del carro estable: 11 a 12 fps, huecos máximos de 90 a 330 ms, sin huecos de más de 500 ms, 0 reconexiones de la cámara (canal fijo y sin ahorro de energía en las dos placas).
- Piso vacío: sin falsos obstáculos (0 de 270 muestras; 3 pasadas completas sin frenar).
- Objetos de ~15 cm de ancho o más (mochila): se ven desde ~80 cm y el navegador se detiene y esquiva. Con un obstáculo enorme y poco espacio la esquiva termina en ERROR (se rinde al verlo todavía muy cerca): es lo diseñado.
- LÍMITE ACEPTADO (decisión del usuario): objetos angostos (termo) se ven a ~19 cm y el carro avanza 14 a 19 cm después del primer cuadro (retraso total 350 a 480 ms: video 320 ms + 12 a 134 ms hasta enviar el STOP + mitad del ida y vuelta; velocidad 38 cm/s). Bajar la potencia no sirve (el motor solo zumba) y el modelo de objetos no reconoce el termo con esta cámara (inventa «person»/«refrigerator»): se queda apagado.
- Ajustes del detector que le funcionaron al usuario (guardados en el teléfono, no en el código): fila mínima ~440, celdas mínimas 6 a 8, umbral de color 40, modelo en NO.
- Ideas si algún día se necesita ver objetos angostos: avance a pulsos con la potencia siempre al 100 %; otra cámara con menos retraso.

## TRABAJO: esquivar objetos grandes sin rendirse y ajustes validados como valores por defecto
- Hecho: reintento de esquive (mirar con un cuadro posterior al giro, retroceder 15 cm ciegos, girar 45° y luego 90° extra, máximo 2 reintentos, si no ERROR y STOP); retroceso calibrable (paso 6 opcional de la calibración; sin calibrar usa el del avance y avisa); elección del lado con menos celdas de obstáculo; valores por defecto del detector en el código (fila mínima 440, celdas mínimas 7, umbral 40, modelo en NO, frenar desde lejos con el video del carro). Documentado en `docs/navegador.md` y `docs/obstaculos.md`.
- Pruebas en PC: todas las suites sin fallos (navegador 134, calibración 132, obstáculos 144, piso 57, fusión 72, movimiento 66, video 14, prueba de obstáculo 16, firmware 56). App compilada e instalada.
- EXPERIMENTO: firmware con velocidad de crucero 210 (arranque a plena potencia y luego 210) cargado en el DevKit; falta comprobar en el carro que arranca, va recto y no se detiene, y recalibrar avance y frenado.
- PENDIENTE HARDWARE: prueba «Esquivar objeto grande (1.4 m)» con la mochila a mitad de camino (`docs/prueba_obstaculos.md`); medir el retroceso en la calibración (opcional, paso 6).

## LOTE 4 (lotes `docs/trabajos/lote4_1.md` a `lote4_6.md`)

### Trabajo A: perfiles de velocidad MANUAL (255) y AUTO (210) — hecho
- Firmware con dos perfiles (`MODO_MANUAL`/`MODO_AUTO`, `ACK_*`, STOP si cambia en movimiento, AUTO al arrancar/reconectar/watchdog); la app abre en manual, las rutas esperan el `ACK_AUTO` 1 s, la calibración siempre en AUTO. Detalle en `docs/perfiles_velocidad.md`.
- Calibración medida a 210 (leída del teléfono): 51.7 ms/cm, frenado 1.4 cm, retroceso 44.4 ms/cm; `docs/latencia.md` recalculado: 8.5 cm hasta frenar con el obstáculo cerca (14.9 a 255).
- Pruebas en PC: firmware 87, servidor falso 14. PENDIENTE HARDWARE: rectas de 150 cm en los dos perfiles; repetir el giro izquierdo de la calibración (valor sospechoso).

### Trabajo B: corredor de trayectoria — hecho (apagado por defecto)
- `corredor.c/.h`: la huella del carro proyectada por perspectiva; solo frena lo que cae DENTRO o en el BORDE (en la duda, frena); lo de FUERA solo avisa. Interfaz común `CajaImagen` para el detector, el modelo de objetos y el flujo. Detalle en `docs/obstaculos.md`.
- Pruebas en PC: `pruebas_corredor` 59 casos, navegador 143, el resto sin fallos. PENDIENTE HARDWARE: medir ancho, altura, inclinación y campo de visión del carro y comprobar con una caja.

### Trabajo E: pisos negros y de 2 colores — hecho
- Reproducción en PC con cuadros sintéticos y arreglos (brillo oscuro, tira ancha con apoyo mínimo, reajuste de brillo, piso fijo en rutas); botón «Aprender piso» (la ruta lo exige o confirma el último); registro `PISO_DIAG`. Detalle y números en `docs/obstaculos.md` («Pisos negros y de 2 colores»).
- Pruebas en PC: pisos difíciles 24, obstáculos 146, piso 58. PENDIENTE HARDWARE: pasos y números a reportar en `docs/obstaculos.md` (casa: tramo de 2 colores y tramo negro; universidad: piso real).

### Trabajo F: modo «cualquier objeto» — hecho (apagado por defecto)
- `cualquiera.c/.h` + regla de fusión (el color manda) + registro `OBJ_ANY` + `scripts/analizar_obj_any.py` + benchmark Lite0/1/2 (tabla en `docs/modelo_objetos.md`).
- Pruebas en PC: `pruebas_cualquiera` 64 casos. PENDIENTE HARDWARE: pasadas sin obstáculo y con termo/caja/mochila en piso liso, negro y de 2 colores (pasos en `docs/modelo_objetos.md`, sección 7).

### Trabajo G: flujo del piso — hecho (apagado por defecto)
- `movimiento_piso.c/.h` + `fusion_aplicar_flujo` + registro `FLUJO_DIAG` + botón «Flujo del piso» (detalle y límites medidos en `docs/flujo_piso.md`).
- Pruebas en PC: `pruebas_movimiento_piso` 54 casos. PENDIENTE HARDWARE: medir ms del análisis y pasadas sobre piso de 2 colores, con caja con dibujo y sobre piso negro/liso (pasos en `docs/flujo_piso.md`, sección 5).

### Trabajo D: latencia del video — hecho en código, medidas PENDIENTE HARDWARE
- JPEG_MS + comparación `frame2jpg`/JPEGENC al arrancar, `TCP_NODELAY` en el WebSocket, cadena de 3 hilos con cola de 1 y `MODO_STATS` (tabla antes/después en `docs/latencia.md`).

### Trabajo I: modos de video raw y celdas — hecho en código, comparación PENDIENTE HARDWARE
- Modos jpeg/raw/celdas en la cámara y en la app, entrada por celdas del detector (`pruebas_celdas` 40 casos), fallback a jpeg, probado en el teléfono con una cámara falsa. Tabla vacía y pasos de las 9 pasadas en `docs/latencia_modos.md`.

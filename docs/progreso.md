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

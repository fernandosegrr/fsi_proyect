# Resumen de la sesión

## Hecho
- **Firmware** (`firmware_carro/`): lógica en C (`carro.c`), `.ino` delgado, `config.h`, `pagina.h` idéntico. Compila con el core 3.3.12. Sin probar en la placa.
- **Pruebas del firmware** (`pruebas_firmware/`): 44 casos, 0 fallos, con comparación contra el original y cruce del desbordamiento de `millis`.
- **Trayectoria** (`nucleo_c/`): 23 casos, 0 fallos. `MAX_PASOS = 2 * MAX_PUNTOS` (128). La calibración (`MS_POR_CM` = 50, `MS_POR_GRADO` = 10) es de ejemplo.
- **App Android** (`app_android/`): `gradlew assembleDebug` termina bien; el APK incluye la biblioteca nativa para 4 arquitecturas. Probada en el Huawei (ver TRABAJO 7).
- **Documentación**: `README.md`, `docs/instalar_app_huawei.md`, `docs/pendientes_firmware.md`, `docs/bitacora.md`, `docs/progreso.md`.

## TRABAJO 6: publicado en GitHub
- Repositorio: https://github.com/fernandosegrr/fsi_proyect (rama `main`).
- Antes de publicar: pruebas (23/23 y 44/44) y `arduino-cli compile` OK; el binario pesa lo mismo que antes del cambio de WiFi (969402 bytes).
- El nombre y la contraseña del WiFi **no están en el repositorio**: viven en `firmware_carro/secretos.h` (ignorado por git, con los valores de hoy) y `secretos.ejemplo.h` (versionado). `original_backup/` tampoco se publica (contiene la contraseña) y sigue intacto en el disco (SHA-256 igual al de antes).
- Revisión de todos los commits: sin contraseña, tokens, claves, correos en el contenido, `local.properties`, APK, `.so`, ejecutables ni `build/`. Solo hay rutas con tu usuario de Windows en `docs/instalar_app_huawei.md`.
- Autor y committer de todos los commits: `fernandosegrr <223138626+fernandosegrr@users.noreply.github.com>` (configurado solo en este repositorio; la configuración global no se tocó). Mensajes y fechas conservados.
- Push con `git push -u origin main` (sin `--force`): aceptado. `git ls-remote origin` muestra `main` y `HEAD` en el mismo commit que el local. Los cambios posteriores (solo documentación) se subieron con un push normal.
- Nota: los mensajes de commit llevan la línea `Co-Authored-By` con `noreply@anthropic.com`.
- Verificación independiente sobre un clon nuevo del remoto: 12 commits, todos con autor y committer `fernandosegrr` (correo noreply); sin contraseña, claves ni otros correos (solo el noreply de GitHub y el `noreply@anthropic.com` de las líneas `Co-Authored-By`); `secretos.h` y `original_backup/` no están en ningún commit remoto (solo `secretos.ejemplo.h`).

## TRABAJO 7: probar la app en el teléfono (hecho)
- `adb devices` mostró el Huawei (ELS-NX9, Android 10) como `device`; `adb install -r` dio `Success`; `am start` abrió `com.fsi.carro/.MainActivity` (arranque en frío de 251 ms) y no hubo errores en logcat.
- La captura muestra la pantalla completa: cuadrícula de 12x8, "Puntos: 0 / 64", botones Calcular y Borrar.
- Prueba de punta a punta con eventos táctiles de `adb`: ruta de 3 celdas arriba y 4 a la derecha (8 puntos); Calcular devolvió AVANZAR 60 cm (3000 ms), GIRAR +90 grados (900 ms), AVANZAR 80 cm (4000 ms), total 7900 ms. Correcto con la calibración de ejemplo.

## TRABAJO 8: benchmark del detector en el Huawei (hecho)
- Reporte completo: `docs/benchmark_ELS-NX9.md` (datos crudos de dos corridas en `docs/datos_benchmark/`). Se lanzó todo con `adb` (`am start ... --ez bench_auto true`), sin tocar el teléfono.
- Detector: EfficientDet-Lite0 (ejemplos oficiales de TensorFlow Lite, 4.5 MB, SHA-256 en la bitácora) con TensorFlow Lite 2.17.0 sin Play services. El modelo de MediaPipe se descartó porque no trae postproceso y no permitía medirlo.
- Mejor configuración: **CPU con 1 hilo**. Un cuadro completo tarda **45.4 ms con JPEG 640x480** (decodificar 2.7 + preparar 3.6 + inferencia 39.1 + postproceso 0.04) y **43.4 ms con 320x240**; unos 22 cuadros por segundo. Es el 15 % de los 300 ms: **quedan ~255 ms** para captura, WiFi y comando (no medidos aún).
- Otras configuraciones: 2 y 4 hilos no mejoran (hasta 54 ms), GPU 149 ms (abre en 1.05 s), NNAPI 63 ms. La GPU y NNAPI funcionan sin errores. Las dos corridas coinciden.
- El modelo funciona: detecta 7 personas en la foto de prueba (CC0 de Wikimedia Commons, no está en git); no detectó sillas ni mochilas con confianza de 0.4 o más.
- Recomendación: enviar JPEG de 320x240 (21 KB contra 65 KB, casi el mismo tiempo de cómputo). Para repetir en el OnePlus y la tableta, ver la última sección del reporte.

## TRABAJO 9: pantalla de control y servidor falso (hecho)
- `pruebas_firmware/servidor_falso.py` imita al carro (comandos, PONG, watchdog de 1000 ms) y `app_android` tiene `ControlActivity` (botones, heartbeat 250 ms, PING 800 ms, reintento 2 s). Probado en el Huawei con `adb reverse`; detalle y resultados en `docs/pruebas_control.md`.
- Latencia PING/PONG por USB: media 7,6 ms, p95 9 ms (no es WiFi). Pendiente con el carro: `bindProcessToNetwork`, Wi-Fi+ del Huawei y `ws://192.168.4.1/ws` real; el firmware aún no contesta `PONG`.

## TRABAJO 10: detector de obstáculos por color de piso (hecho)
- `nucleo_c/obstaculos.c/.h` (32 pruebas en PC, todas pasan), puente JNI y benchmark en el Huawei: un cuadro completo tarda 1.36 ms de media (p95 hasta 2.03 ms; solo el análisis en C 0.59 ms), unas 30 veces menos que el modelo (43.4 ms). Resultado correcto en las 4 imágenes generadas. Detalle en `docs/obstaculos.md` y al final de `docs/benchmark_ELS-NX9.md`.
- Pendiente: cámara real de la ESP32-CAM, foto real de pasillo, calibrar umbrales.

## Pendiente / para decidir
- Medir captura, WiFi y comando con la ESP32-CAM para cerrar el presupuesto de 300 ms, y repetir el benchmark en el OnePlus y la tableta.
- Medir `MS_POR_CM` y `MS_POR_GRADO` con el carro real.
- Probar el firmware en la placa y atacar de uno en uno los puntos de `docs/pendientes_firmware.md`.
- Si cambias la contraseña del WiFi, edítala solo en `firmware_carro/secretos.h`.
- Si el Huawei usa HarmonyOS NEXT, no ejecuta APK (este modelo, con Android 10, sí funcionó).
- Siguiente paso natural de la app: WebSocket hacia el carro y reproducir los pasos (AVANZAR/GIRAR) con los tiempos calculados; el watchdog del firmware frena a los 1000 ms sin comando, así que hay que repetir el comando cada 250 ms.

## Bloqueos
- Ninguno.

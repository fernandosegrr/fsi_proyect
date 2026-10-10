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

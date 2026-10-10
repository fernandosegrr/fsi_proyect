# Progreso

## Parte A: firmware en C (lista aprobada)
- Hecho: `firmware_carro/` (firmware_carro.ino, carro.c/.h, config.h, pagina.h idéntico por SHA-256), `docs/` y `pruebas_firmware/`.
- Compilación: `arduino-cli compile --fqbn esp32:esp32:esp32 firmware_carro` sin errores (73 % de flash, 14 % de RAM).
- Pruebas de PC: 44 casos, 0 fallos, incluida la comparación con el original (2 × 300 000 pasos, 0 diferencias) y el cruce del desbordamiento de millis.
- Sin probar en hardware.
- Pendiente: TRABAJOS 1-3 de la parte C (el mensaje llegó cortado).

## TRABAJO 2: módulo de trayectoria (nucleo_c)
- Hecho: `nucleo_c/` con trayectoria.h/.c, calibracion.h, pruebas_trayectoria.c y Makefile. C99 puro, enteros, sin malloc ni math.h; el header se incluye también desde C++.
- Pruebas: `make test` en nucleo_c → 19 casos, 0 fallos (recta, derecha, izquierda, U, un punto, diagonales, giros de 180 y -135, errores y excesos de MAX_PUNTOS / MAX_PASOS).
- Decisiones: ver bitácora (diagonal calculada sobre el total, +180 para media vuelta, MAX_PASOS = 100).
- Pendiente: medir MS_POR_CM y MS_POR_GRADO con el carro (valores actuales son de ejemplo).

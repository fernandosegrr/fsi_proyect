# Progreso

## Parte A: firmware en C (lista aprobada)
- Hecho: `firmware_carro/` (firmware_carro.ino, carro.c/.h, config.h, pagina.h idéntico por SHA-256), `docs/` y `pruebas_firmware/`.
- Compilación: `arduino-cli compile --fqbn esp32:esp32:esp32 firmware_carro` sin errores (73 % de flash, 14 % de RAM).
- Pruebas de PC: 44 casos, 0 fallos, incluida la comparación con el original (2 × 300 000 pasos, 0 diferencias) y el cruce del desbordamiento de millis.
- Sin probar en hardware.
- Pendiente: TRABAJOS 1-3 de la parte C (el mensaje llegó cortado).

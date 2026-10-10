# Resumen de la sesión

## Hecho
- Firmware (`firmware_carro/`): lógica en C (`carro.c`), `.ino` delgado, `config.h`, `pagina.h` idéntico. Compila con core 3.3.12. Sin probar en la placa.
- Pruebas de PC (`pruebas_firmware/`): 44 casos, 0 fallos, incluida comparación con el original y cruce de desbordamiento de millis.
- Módulo de trayectoria (`nucleo_c/`): 20 casos, 0 fallos. Calibración de ejemplo (`MS_POR_CM` = 50, `MS_POR_GRADO` = 10): hay que medirla.
- Documentación: `README.md`, `docs/pendientes_firmware.md`, `docs/bitacora.md`, `docs/progreso.md`.
- Git local: commits por hito, sin push. `original_backup/` no se tocó.

## Pendiente / para decidir
- Medir `MS_POR_CM` y `MS_POR_GRADO` con el carro real.
- Probar el firmware en la placa y atacar de uno en uno los puntos de `docs/pendientes_firmware.md`.
- Cambiar SSID y contraseña (`config.h`) antes de publicar el repositorio.
- Sin bloqueos: no hizo falta preguntar nada.

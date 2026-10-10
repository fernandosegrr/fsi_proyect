# Resumen de la sesión

## Hecho
- **Firmware** (`firmware_carro/`): lógica en C (`carro.c`), `.ino` delgado, `config.h`, `pagina.h` idéntico. Compila con el core 3.3.12. Sin probar en la placa.
- **Pruebas del firmware** (`pruebas_firmware/`): 44 casos, 0 fallos, con comparación contra el original y cruce del desbordamiento de `millis`.
- **Trayectoria** (`nucleo_c/`): 23 casos, 0 fallos. `MAX_PASOS = 2 * MAX_PUNTOS` (128), así una ruta válida nunca desborda. La calibración (`MS_POR_CM` = 50, `MS_POR_GRADO` = 10) es de ejemplo.
- **App Android** (`app_android/`): `gradlew assembleDebug` termina bien y el APK incluye la biblioteca nativa para 4 arquitecturas. El puente JNI en C se probó en PC con un entorno falso. No se probó en teléfono ni en emulador.
- **Documentación**: `README.md`, `docs/instalar_app_huawei.md`, `docs/pendientes_firmware.md`, `docs/bitacora.md`, `docs/progreso.md`.
- **Git**: commits locales por hito, sin push. `original_backup/` no se tocó.

## Pendiente / para decidir
- Medir `MS_POR_CM` y `MS_POR_GRADO` con el carro real.
- Probar el firmware en la placa y atacar de uno en uno los puntos de `docs/pendientes_firmware.md`.
- Probar la app en el Huawei con la guía. Si es HarmonyOS NEXT, no ejecuta APK.
- Cambiar SSID y contraseña (`config.h`) antes de publicar el repositorio.
- Siguiente paso natural de la app: WebSocket hacia el carro y reproducir los pasos (AVANZAR/GIRAR) enviando `FORWARD`/`LEFT`/`RIGHT`/`STOP` con los tiempos calculados. Ojo: el watchdog del firmware frena a los 1000 ms sin comando, así que habrá que repetir el comando cada 250 ms como hace la página web.

## Bloqueos
- Ninguno que obligara a preguntar. Decisiones de versiones (Gradle 9.8.1, AGP 9.4.1) en `docs/bitacora.md`.

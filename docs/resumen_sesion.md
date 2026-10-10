# Resumen de la sesión

## Hecho
- **Firmware** (`firmware_carro/`): lógica en C (`carro.c`), `.ino` delgado, `config.h`, `pagina.h` idéntico. Compila con el core 3.3.12. Sin probar en la placa.
- **Pruebas del firmware** (`pruebas_firmware/`): 44 casos, 0 fallos, con comparación contra el original y cruce del desbordamiento de `millis`.
- **Trayectoria** (`nucleo_c/`): 23 casos, 0 fallos. `MAX_PASOS = 2 * MAX_PUNTOS` (128). La calibración (`MS_POR_CM` = 50, `MS_POR_GRADO` = 10) es de ejemplo.
- **App Android** (`app_android/`): `gradlew assembleDebug` termina bien; el APK incluye la biblioteca nativa para 4 arquitecturas. El puente JNI se probó en PC con un entorno falso.
- **Documentación**: `README.md`, `docs/instalar_app_huawei.md`, `docs/pendientes_firmware.md`, `docs/bitacora.md`, `docs/progreso.md`.

## TRABAJO 6: publicar en GitHub (preparado, falta el push)
- Antes de tocar nada se corrieron las pruebas (23/23 y 44/44) y `arduino-cli compile` (mismo tamaño: 969402 bytes).
- El nombre y la contraseña del WiFi salieron de git: viven en `firmware_carro/secretos.h` (ignorado, con los valores de hoy) y `secretos.ejemplo.h` (versionado). El firmware sigue igual en tu equipo.
- `original_backup/` ya no se versiona porque el `.ino` y el PDF contienen la contraseña; sigue intacto en el disco (SHA-256 igual al de antes).
- Historial local reescrito (7 commits, mensajes conservados). Revisión final de todos los commits: sin contraseña, tokens, claves, correos en el contenido, `local.properties`, APK, `.so`, ejecutables ni `build/`. Rutas con tu usuario de Windows en `docs/instalar_app_huawei.md` (solo se reportan).
- Rama `main` y remoto `origin` configurados; el remoto sigue vacío. **No se hizo push.**
- **Detenido por la regla del correo**: los 8 commits tienen como autor y committer un correo personal de Gmail, y quedaría público. Tu configuración de git trae otro correo (el del colegio). Falta que decidas cuál usar (ver "Pendiente").

## TRABAJO 7: probar la app en el teléfono (sin completar)
- `adb devices` ve un teléfono, pero con estado `unauthorized`. No se instaló nada.
- Qué hacer en el teléfono: desbloquéalo, desconecta y vuelve a conectar el cable, y en el cuadro **¿Permitir depuración USB?** pulsa **Permitir** (marca "Permitir siempre desde este equipo"). Si el cuadro no aparece: Ajustes → Sistema y actualizaciones → Opciones de desarrollador → **Revocar autorizaciones de depuración USB**, y vuelve a conectar. Luego avísame y repito la instalación (`adb install -r`, abrir la app y captura de pantalla).

## Pendiente / para decidir
- **Correo de los commits** (necesario antes del push). Opciones: (a) dejar el de Gmail; (b) reescribir los autores a la dirección privada de GitHub (`<id>+<usuario>@users.noreply.github.com`, la ves en GitHub → Settings → Emails); (c) usar el correo de tu configuración de git. Con (b) o (c) se reescribe otra vez el historial local, que sigue sin publicarse.
- Autorizar la depuración USB en el Huawei y repetir el TRABAJO 7.
- Medir `MS_POR_CM` y `MS_POR_GRADO` con el carro real.
- Probar el firmware en la placa y atacar de uno en uno los puntos de `docs/pendientes_firmware.md`.
- Si el Huawei usa HarmonyOS NEXT, no ejecuta APK.
- Siguiente paso natural de la app: WebSocket hacia el carro y reproducir los pasos (AVANZAR/GIRAR) con los tiempos calculados; el watchdog del firmware frena a los 1000 ms sin comando, así que hay que repetir el comando cada 250 ms.

## Bloqueos
- Correo de los commits (punto de arriba) y teléfono sin autorizar. Ninguno afecta al código.

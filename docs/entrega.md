J14 — Entrega del proyecto Carro FSI

27 de octubre de 2026.

Esta carpeta y dist/ contienen lo necesario para reconstruir, instalar y comprobar la entrega de J14. El reporte completo de pruebas y límites está en docs/reporte_J14.md.

1. Contenido de dist/

- carro_fsi_0.4-699fcc5.apk
  APK de la app. Versión APP:0.4-699fcc5, versionName 0.4, versionCode 3. Tamaño 14,500,144 bytes.
  SHA-256: C50A9757DE59B1B604680BE9731D8C610F9EC4A66780EB18ECEB558E6E00312A.

- SHA256SUMS.txt
  El hash de la APK en formato sha256sum, con el nombre de archivo.

- LEEME.txt
  Resumen corto de versiones y comandos.

- FIRMWARE_LOCAL.txt
  Explica que el binario del firmware no se publica y da su versión, tamaño y hash.

El binario del firmware, firmware_carro_453ed20-20261027.bin, NO se publica en GitHub porque contiene la contraseña del WiFi del carro como texto. Solo se conserva en la copia local de dist/. Su versión es FW:453ed20-20261027, pesa 1,031,712 bytes y su SHA-256 es 9FD1BB5103D1AD66652ADEA68B009D54D30B5C5B6F07F8CA7488A4FBF274048E. El repositorio publica el código fuente y la APK; el firmware se reconstruye con firmware_carro/secretos.h.

Los archivos de dist/ pesan menos de 15 MB. No se incluye firmware_carro/secretos.h ni ninguna contraseña. Para compilar el firmware hace falta crear secretos.h a partir de secretos.ejemplo.h.

2. Verificar la entrega

En Windows, desde la carpeta del proyecto:

    Get-FileHash dist\carro_fsi_0.4-699fcc5.apk -Algorithm SHA256

En Linux, Git Bash o WSL:

    cd dist
    sha256sum -c SHA256SUMS.txt

El hash debe coincidir exactamente con el de la sección 1. Si no coincide, no instalar la APK.

El firmware no se publica. Si se conserva la copia local, su hash se comprueba con:

    Get-FileHash dist\firmware_carro_453ed20-20261027.bin -Algorithm SHA256

y debe coincidir con el de FIRMWARE_LOCAL.txt.

3. Instalar la APK

Con el teléfono desbloqueado, depuración USB activada y el cable de datos conectado:

    adb install -r dist\carro_fsi_0.4-699fcc5.apk

La instalación debe decir Success y conservar los datos. No se debe desinstalar la app ni borrar sus preferencias. Si Android pide confirmación, aceptar solo la instalación de esta APK.

Para comprobar la versión instalada:

    adb shell dumpsys package com.fsi.carro

Debe aparecer versionName=0.4 y versionCode=3. En la pantalla Sensores de la app debe verse App 0.4 (699fcc5).

4. Cargar el firmware

El binario del firmware no se publica en GitHub porque contiene la contraseña del WiFi del carro como texto. La copia local de entrega lo conserva, pero para cargarlo se recomienda reconstruirlo desde el código fuente con firmware_carro/secretos.h. Ese archivo no se publica.

Desde la carpeta del proyecto:

    .\scripts\cargar.ps1 nuevo -Puerto COM6

El script compila el firmware desde firmware_carro/, busca la placa, carga y escucha el puerto serie. No cambia la red WiFi de la PC y oculta la contraseña en su salida. Si no hay puerto COM, revisar el cable de datos y el driver CP210x.

Para comprobar la versión por USB:

    pruebas_firmware\.venv\Scripts\python.exe scripts\verificar_version.py --puerto COM6

Debe reportar FW:453ed20-20261027. La comparación completa con la APK instalada también puede usar --apk y --revision.

5. Configuración del teléfono y del carro

- El teléfono debe estar unido a la red CarroFSI del carro. No hace falta escribir la contraseña si la red ya está guardada.
- El carro debe estar encendido, con pilas y powerbank, y la cámara debe responder.
- Abrir la app y entrar a Calibración → Sensores. Comprobar FW:453ed20-20261027, App 0.4 (699fcc5), giroscopio y láser funcionando, y la Autoridad del láser encendida.
- En Ajustes → Red, restablecer las direcciones si el teléfono o el carro cambiaron de red.
- Aprender el piso en el lugar de la demo antes de ejecutar una ruta.
- Dejar libre la zona de prueba y probar primero una recta corta y el botón PARO.

6. Checklist antes de la demo

- Cambiar la contraseña del WiFi del carro.
- Restablecer las direcciones en Ajustes → Red.
- Volver a configurar el PIN.
- Poner pilas nuevas.
- Borrar la copia de OneDrive solo después de confirmar que el push a GitHub terminó bien.
- Confirmar en Sensores el FW y la APK instalados.
- Revisar batería del teléfono, cable USB y respuesta de la cámara.
- Tener el área despejada y el carro quieto antes de moverlo.

7. Límites que se deben decir al usuario

- B7, B8, B9 y B2 no se hicieron por decisión del dueño. No están validados físicamente.
- B5 no tiene una medición numérica del dueño; el error lateral de ~20 a 24 cm es una estimación de navegación.
- En B6, la corrida 2 probablemente tocó la caja y la corrida 4 quedó ~40 cm a la derecha. La reincorporación después de esquivar sigue siendo un fallo real.
- La Autoridad del láser siempre encendida es una decisión del dueño, no una conclusión pareada. El láser ve un punto y puede no ver un objeto central fuera de su haz.
- J10c está corregido solo en simulación. Falta una caja ancha real.
- La verificación final de FW por Sensores quedó pendiente porque al cierre el punto de acceso del carro no estaba visible. El APK instalado sí se verificó por hash.
- El binario del firmware no se publica en GitHub porque contiene la contraseña del WiFi como texto. Se conserva solo en la copia local de dist/; el repositorio publica el código fuente y la APK. Para cargarlo hay que crear firmware_carro/secretos.h y reconstruirlo.
- La cámara puede dejar de responder en el puerto HTTP 80 aunque el ping siga. Apagar y encender la cámara y esperar unos 15 s.

8. Documentos relacionados

- docs/reporte_J14.md: resultados completos, decisiones e incidentes.
- docs/progreso_J14.md: bitácora de la sesión J14.
- docs/validacion_sensores.md: sección J14 de la validación física.
- docs/limites_conocidos.md: límites vigentes.
- docs/bitacora.md: decisiones acumuladas.
- docs/instalar_app_huawei.md: instalación de la app en el Huawei.
- docs/hardware_cargar.md: carga del firmware por USB.

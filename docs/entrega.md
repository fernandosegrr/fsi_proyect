J15 — Entrega y estado de validación del proyecto Carro FSI

27 de octubre de 2026, hora de Ciudad de México.

La APK con la corrección de pose está instalada y verificada. La prueba física del esquive queda NO MEDIDA: el dueño la continuará con otro modelo. Este paquete no acredita que el carro ya cumpla el criterio físico. Reporte completo: docs/reporte_J15.md; antecedente: docs/reporte_J14.md.

1. Contenido publicado de dist/

carro_fsi_0.4-074c9fd.apk
Versión: APP:0.4-074c9fd, versionName 0.4, versionCode 3.
Tamaño: 14,852,386 bytes.
SHA-256: 36CE38F2ABD11C1030A7DE7B8A23009A865A01365900192E69CED9CDDE9513BA.

SHA256SUMS.txt contiene solo el hash y nombre de esa APK.
LEEME.txt resume instalación y estado de validación.
FIRMWARE_LOCAL.txt identifica el firmware conservado y explica por qué no se publica.

El binario del firmware NO se publica porque contiene la contraseña del WiFi. Tampoco se publica secretos.h ni original_backup/. Todos los archivos publicados quedan por debajo de 15,000,000 bytes. El APK anterior de J14 se conserva en el historial y en v1.0, no como APK vigente de dist/.

2. Verificar la APK publicada

Desde la carpeta del proyecto, en PowerShell:

    Get-FileHash dist\carro_fsi_0.4-074c9fd.apk -Algorithm SHA256

Debe coincidir con el SHA-256 de la sección 1. SHA256SUMS.txt también admite sha256sum -c desde dist/ en un entorno que tenga esa herramienta.

3. Instalación ya realizada y comprobación

La etapa 4 de J15 instaló con adb install -r, conservando datos. No hace falta reinstalar para continuar las pruebas si la versión sigue siendo la indicada. Nunca desinstalar ni borrar las preferencias.

Comando de instalación, si se necesita repetirla con la configuración 1B autorizada:

    adb install -r dist\carro_fsi_0.4-074c9fd.apk

Comando de verificación sin placa serie:

    pruebas_firmware\.venv\Scripts\python.exe scripts\verificar_version.py --solo-apk --apk dist\carro_fsi_0.4-074c9fd.apk --revision HEAD

Ese comando ya dio PASA: APK instalada idéntica a la publicada y revisión de fuentes correcta. Deja explícitamente el FW pendiente. Mantener la pantalla encendida hasta que termine de leer la APK y solo después restaurarla. En Sensores debe verse App 0.4 (074c9fd).

4. Firmware conservado

J15 no cambió firmware_carro ni requirió carga. Versión esperada por el registro anterior: FW:453ed20-20261027.
Copia local: firmware_carro_453ed20-20261027.bin, 1,031,712 bytes.
SHA-256: 9FD1BB5103D1AD66652ADEA68B009D54D30B5C5B6F07F8CA7488A4FBF274048E.

No se comprobó la versión actual en el DevKit durante J15, porque se usó la configuración 1B (solo teléfono). Leerla en Sensores antes de mover el carro; no confundir un hash local con una lectura de la placa. No hace falta conectar ni cargar el DevKit para continuar con este mismo firmware.

5. Pruebas y validación

Últimas pruebas de software realizadas: núcleo 21 programas/1598 casos, firmware 447, servidor falso 46 y verificador 13; cero fallos. Estrés de 100000 comandos sin STOP perdido y hash de página correcto. Sanitizadores success en el último commit con código, 074c9fd. No se volvieron a ejecutar pruebas durante el cierre documental, por indicación del dueño.

Simulación B6: error lateral medio 13.2 a 1.9 cm, p50 13.5 a 1.7 cm y p95 21.9 a 4.6 cm. Después: 400/400 sin contacto y con error menor de 20 cm. No reproduce exactamente los 40 cm ni las esquivas múltiples de J14 y no demuestra el resultado físico.

Prueba física J15: NO MEDIDA, cero corridas. Pendiente el criterio de al menos 5/6 sin contacto y a menos de 25 cm. No se creó v1.1; v1.0 sigue en eb88151.

6. Continuación de las pruebas con otro modelo

Usar la configuración 2 y las condiciones de parada de docs/guia_conexion_J15.md. Antes de mover: verificar FW, APP, sensores, ANT, Autoridad del láser encendida y aprender el piso despejado. Continuar con dos rectas de 200 cm sin caja y seis con la misma caja de B6 a aproximadamente 100 cm, registrando contacto, desvío real, lado y diagnósticos. El reporte no inicia ni autoriza por sí mismo una ruta.

La distancia sigue por tiempo; el giroscopio no observa deslizamiento lateral puro. No se ha demostrado que la corrección resuelva toda la desviación real de B6. Los límites de percepción y de orientación al final de ruta siguen descritos en docs/limites_conocidos.md.

7. Antes de la demo

Restablecer direcciones en Ajustes → Red.
Poner pilas nuevas.
Aprender el piso en el lugar, con la zona despejada.
Volver a poner el PIN al teléfono.
Confirmar versiones y sensores con el carro conectado y realizar la validación física pendiente.
Conservar la contraseña del WiFi y secretos.h por decisión del dueño. No modificar original_backup/ ni la copia antigua de OneDrive.

8. Documentos relacionados

Reporte y límites: docs/reporte_J15.md y docs/limites_conocidos.md.
Registro por fase: docs/progreso_J15.md.
Conexión y parada: docs/guia_conexion_J15.md.
Resultados físicos, con estado NO MEDIDO en J15: docs/validacion_sensores.md.
Antecedentes B4/B5/B6: docs/reporte_J14.md y docs/progreso_J14.md.

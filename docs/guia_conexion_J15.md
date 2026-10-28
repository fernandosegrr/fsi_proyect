GUÍA DE CONEXIÓN Y PARADA — J15

La corrección usa el firmware existente. Solo se instala APP:0.4-074c9fd. No se carga firmware; se conserva FW:453ed20-20261027. Los resultados de simulación no son resultados físicos.

CONFIGURACIÓN 1B: instalar la APK sin movimiento.
1. Conecta únicamente el teléfono por USB a la PC y selecciona Transferir archivos; el carro no se toca y no hace falta conectar ninguna placa.
2. Deja el teléfono desbloqueado, temporalmente sin PIN ni bloqueo de pantalla y con depuración USB activada.
3. Si aparece el permiso de depuración USB, autorízalo para esta PC; no escribiré PIN, contraseñas ni cuentas.
4. Mantén el teléfono en su WiFi normal; la PC también conserva su red normal.
5. Se comprobará el teléfono con: & "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" devices; debe aparecer como device. Si dice unauthorized, acepta el permiso en el teléfono.
6. Guardaré stay_on_while_plugged_in y el estado de la pantalla antes de tocar el teléfono; instalaré solo con adb install -r y conservaré los datos.
7. Verificaré versión y bytes con scripts\verificar_version.py --solo-apk --apk dist\carro_fsi_0.4-074c9fd.apk --revision HEAD. El firmware quedará pendiente de lectura en Sensores durante la configuración 2.
8. En esta etapa no se manda movimiento; cerraré la app con am force-stop y restauraré los ajustes de pantalla.

CONFIGURACIÓN 2: pruebas con el carro en movimiento, después de terminar la instalación.
1. Deja el DevKit SIN USB y aliméntalo con las pilas AA y la powerbank del carro.
2. Alimenta la cámara con su propia powerbank; ninguna de las placas debe estar conectada por USB a la PC.
3. Prepara un área despejada de al menos 2.5 m por 1.5 m y una cinta recta de 2 m alineada con el frente del carro.
4. Coloca el carro en el piso al inicio de la cinta y mantén libre la parte de atrás y los costados.
5. Ten la misma caja de B6 disponible y fuera de la ruta al inicio; indicaré dónde colocarla antes de cada prueba y esperaré LISTO cuando haga falta colocar algo.
6. Conecta el teléfono por USB a la PC, con batería cargada y cargando, desbloqueado y con la pantalla sin apagarse.
7. Une el teléfono a CarroFSI; la PC conserva su red normal y no se une a CarroFSI.
8. Permanece presente durante todas las corridas; antes de comenzar habrá comprobación de versiones, sensores, ANT, Autoridad del láser encendida y aprendizaje del piso despejado.
9. Si la cámara no responde, apágala y enciéndela y espera aproximadamente 15 s; no se inicia una ruta sin video.
10. Ante un problema pulsa PARO; yo detendré la ruta y cerraré la app con am force-stop, que deja actuar también al watchdog de 1 s.
11. Se aborta todo por giro desviado más de 25°, giroscopio MAL persistente tras recalibrar, pérdida de CarroFSI mayor de 30 s, salida del área, choque con algo que no sea la caja, batería menor de 5 % o 90 minutos de pruebas; se pausa por batería menor de 10 % sin cargar. Máximo 20 corridas.
12. Al terminar cada etapa quedará la app cerrada; al finalizar las pruebas restauraré stay-on, pantalla y WiFi del teléfono.

ETAPA 3 TERMINADA. Escribe CONECTADO 1 cuando estés en la configuración 1 (o 1B).

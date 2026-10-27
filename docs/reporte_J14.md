J14 — Reporte final del cierre de proyecto FSI

27 de octubre de 2026, hora de Ciudad de México.

1. Resumen

El dueño decidió detener las pruebas físicas que faltaban y pidió el reporte final. B0 a B6 se hicieron; B7, B8, B9 y B2 no se realizaron por decisión del dueño. B10 (Autoridad del láser siempre encendida) también es una decisión del dueño, no el resultado de una comparación pareada.

El producto queda con firmware y APK cargados, pruebas de PC en verde y una entrega en dist/. No se presenta como validado lo que no se midió: en particular, no hay validación física de B7, B8, B9 ni B2, y J10c solo está corregido en simulación.

2. Versiones y artefactos

Firmware:
- Versión: FW:453ed20-20261027.
- Compilado a partir del commit 453ed20.
- Tamaño: 1,031,712 bytes.
- SHA-256: 9FD1BB5103D1AD66652ADEA68B009D54D30B5C5B6F07F8CA7488A4FBF274048E.
- El binario NO se publica en GitHub porque contiene la contraseña del WiFi del carro como texto. Solo se conserva en la copia local de dist/.
- Se cargó al DevKit en la Etapa 4 de J14. La comprobación final por Sensores no se pudo repetir al cierre porque el punto de acceso del carro no estaba visible y no había puerto COM disponible. El binario local conserva la versión y el hash registrados en la Etapa 4.

APK:
- Versión: APP:0.4-699fcc5.
- versionName 0.4, versionCode 3.
- Tamaño: 14,500,144 bytes.
- SHA-256: C50A9757DE59B1B604680BE9731D8C610F9EC4A66780EB18ECEB558E6E00312A.
- Instalada con adb install -r: Success, conservando datos.
- La APK instalada se extrajo del teléfono y su SHA-256 coincide con la APK de dist/.
- La pantalla Sensores muestra App 0.4 (699fcc5), los dos interruptores de sensores encendidos y la Autoridad del láser encendida. El FW aparece como «sin versión (firmware anterior)» porque el teléfono no estaba conectado al carro en ese momento; ese texto no demuestra por sí solo que el firmware sea viejo.

Entrega publicada en el repositorio:
- dist/carro_fsi_0.4-699fcc5.apk
- dist/SHA256SUMS.txt (solo la APK)
- dist/LEEME.txt
- dist/FIRMWARE_LOCAL.txt (hash del firmware y explicación de por qué no se publica)

Copia local, no publicada:
- dist/firmware_carro_453ed20-20261027.bin (contiene la contraseña del WiFi)

Git:
- La entrega se publica en la rama main y se marca con el tag v1.0.
- Los commits principales de J14 van de da745af (etapa 0) a 699fcc5 (README con el estado final), más el cierre documental y de dist/.
- El APK se compiló desde 699fcc5. Los commits posteriores son solo documentación y artefactos de entrega, que el verificador acepta sin ocultar cambios de producto.

3. Pruebas de PC y de la app

- Núcleo en C: 20 programas, 1577 casos, 0 fallos.
- Firmware: 152 + 246 + 15 + 34 = 447 casos, 0 fallos.
- Estrés de 2 hilos: 100000 comandos con GIRAR mezclado; 12472 STOP enviados, 0 STOP perdidos, 0 giros terminados solos y 0 watchdogs. La cola se mantuvo coherente y el estado final de los motores coincidió con el último comando.
- Servidor falso: 46 casos, 0 fallos.
- Verificador de versiones: 8 pruebas, 0 fallos. Se añadió el caso que acepta dist/ como artefacto de entrega.
- Hash de la página del carro: correcto.
- APK: compilación correcta, instalación correcta y hash instalado igual al de dist/.
- Las suites de C se volvieron a correr al final y siguen en verde; los últimos cambios fueron de texto de la app, README, verificador y documentación, y no tocan el código C. La app se recompiló e instaló después del cambio de texto. No se hizo push con pruebas rojas.

4. Pruebas físicas realizadas

B0, 12:43, sin movimiento:
- Teléfono reconectado a la red guardada del carro sin introducir la contraseña.
- FW:453ed20-20261027 y APP:0.4-453ed20 visibles en Sensores.
- Ambos sensores responden; ANT:126,144,20,20 ms; autoridad del láser apagada en ese momento.
- Batería del teléfono al 9 %, cargando por USB.

B1, sin movimiento:
- Tres bloques de cuatro giros derechos de 90°: 12/12 dentro de ±3°.
- Sumas de bloque: 360.7°, 360.8° y 356.6°.
- ANT AUTO derecha pasó de 144 a 142 ms; los otros valores siguieron en 126, 20 y 20 ms.
- El dueño confirmó que el frente quedó prácticamente como al inicio y paralelo a la cinta; no dio un ángulo numérico.

B2, NO MEDIDA:
- La calibración instalada no publica los ángulos de cada giro hasta terminar los ocho, así que no se pudo vigilar a tiempo el límite de aborto de ±25° entre giros.
- Se intentó una protección experimental en una rama local. No pasó tres comprobaciones: un caso de inconsistencia seguía en rojo. Se restauró el último commit bueno; el parche quedó en %TEMP%/fsi_j14_b2_seguridad_experimental.patch y no se instaló ni se publicó.
- B2 no se validó físicamente.

B3, sin movimiento:
- Dos bloques de cuatro giros por lado, usando la calibración anterior porque B2 quedó sin medir.
- Izquierda: 88.5°, 89.1°, 89.5°, 90.0° y 92.0°, 91.4°, 91.6°, 88.5°; 8/8.
- Derecha: 88.8°, 90.1°, 90.6°, 90.6° y 90.7°, 89.2°, 90.4°, 89.7°; 8/8.
- Los 16 giros terminaron dentro de ±3°; el carro quedó quieto.

B4, recta de 150 cm, 4 corridas:
- Autoridad apagada: desvío lateral de ~15 cm y ~20 cm; 150 cm recorridos; 5 s; 0 esquivas; sin paros ni eventos STOP/TOF.
- Autoridad encendida: desvío de ~5.5 cm y ~18 cm; 150 cm recorridos; 5 s; 0 esquivas; sin paros.
- 4/4 rutas terminadas y 0 falsos paros en piso vacío.
- Antes de la corrida 3 y de la corrida 4 la cámara falló: el puerto HTTP 80 dejó de responder aunque el ping seguía. Se apagó y encendió la cámara, se esperó unos 15 s y se repitió. El carro no se movió en los intentos fallidos.

B5, ruta en L de 100 cm, giro de 90° a la derecha y 100 cm, 3 corridas válidas:
- Con giroscopio: 7 s y 8 s, 0 esquivas, giros de ~91° y ~90°. Posición final estimada x=1406 mm, y=209 mm, rumbo 91°; y x=1389 mm, y=203 mm, rumbo 90°.
- Sin giroscopio: 7 s, 0 esquivas, giro por tiempo de ~93°. Posición final estimada x=1394 mm, y=244 mm, rumbo 93°.
- Las 3 corridas terminaron. El dueño no dio una cifra de error final; contra el objetivo y=0, las estimaciones de navegación dejan un error lateral de ~20 a 24 cm. Su valoración cualitativa fue «relativamente bien».
- Una corrida previa con un sillón al final de la L hizo una esquiva y terminó con rumbo 324°; no se cuenta como corrida limpia.

B6, caja delante, ruta Recta 200 cm, 4 corridas:
- Con la caja a 40 cm la app no arrancó: el aprendizaje del piso detectó la caja («Hay algo frente al carro», piso muy variado) y pidió despejar. El carro no se movió. Es una protección de la app y una desviación del método.
- La caja quedó finalmente a ~100 cm del frente. El dueño aclaró que todas las corridas siguientes fueron a ~100 cm, no a 70 cm.
- Autoridad apagada, corrida 1: 23 s, 2 esquivas, sin tocar la caja; terminó en x=394 mm, y=5 mm, rumbo 117°. Al reincorporarse detectó un obstáculo lejano y no volvió limpio a la línea.
- Autoridad apagada, corrida 2: 56 s, 3 esquivas; terminó en «Necesito ayuda: toma el control» en el punto 3/10, con x=659 mm, y=780 mm, rumbo 306°. El dueño cree que sí tocó la caja.
- Autoridad encendida, corrida 3: 26 s, 2 esquivas, sin tocar; x=393 mm, y=-12 mm, rumbo 119°. Quedó cerca del objetivo pero mal orientada.
- Autoridad encendida, corrida 4: 15 s, 1 esquiva, sin tocar; x=409 mm, y=6 mm, rumbo 0°. El dueño la vio unos 40 cm a la derecha de la línea recta.
- El algoritmo de esquiva no hizo el rodeo limpio que el dueño esperaba (90°, 50 cm, 90°, 50 cm, 90° y volver a la línea): durante la reincorporación se enredó con obstáculos lejanos. Es un fallo real de la vuelta a la ruta, no falta de espacio.

B7, B8, B9 y B2:
- B7 no se ejecutó como prueba separada. El dueño indicó que las corridas de B6 ya se hicieron con la caja a ~100 cm y que no hacía falta repetirlas; no se hicieron las tres corridas con Autoridad encendida que pedía el plan.
- B8 (caja lateral a 25 cm) y B9 (corte de WiFi) no se ejecutaron: el dueño decidió que ya no eran necesarias.
- B2 (calibración nueva por bloques) no se ejecutó: queda NO MEDIDA.
- Por eso no se puede afirmar que el watchdog de WiFi, la caja lateral o la calibración nueva por bloques estén validados físicamente en J14.

B10, Autoridad del láser:
- El dueño decidió que quede siempre encendida. No es una conclusión de una comparación pareada B4-B8; es una decisión del dueño y así se registra.
- El valor por defecto de la app se cambió a verdadero. El núcleo en C conserva 0 por defecto para sus pruebas.
- La comparación pareada y la validación con objetos fuera del haz del láser quedan pendientes.

5. Cambios de código y configuración

- Ajustes.kt y ControlLogica.kt: la Autoridad del láser queda encendida por defecto.
- ConfigVision.kt: REF_ANCHO_MIL_APP=600 y GRUPOS_PISO_APP=3. El núcleo en C conserva 800 y 4. El grupos_piso=3 guardado en el teléfono es una configuración de prueba; el valor por defecto de la app ahora también es 3.
- build.gradle.kts: runner y dependencias de instrumentación para pulsar vistas por resource-id, sin coordenadas fijas, y compresión de bibliotecas nativas con useLegacyPackaging = true para que el APK quede bajo 15 MB.
- puente.c: se añadió memset(&cfg, 0, sizeof cfg) para dejar la configuración limpia.
- ControlRutaTest.kt: arnés de instrumentación para las pruebas físicas por ADB.
- textos_sensores.xml: el texto de la Autoridad del láser ahora dice que el dueño decidió dejarla siempre encendida y que la comparación pareada quedó pendiente.
- .gitignore: se permite versionar el APK de dist/.
- verificar_version.py y probar_version.py: el verificador acepta dist/ como artefacto de entrega, además de docs/.
- J10c se corrigió en simulación en el commit bce7b37 (regla de segundo color de piso para obstáculo ancho). No se probó con una caja ancha real.

6. Límites y pendientes reales

- No hay validación física de B7, B8, B9 ni B2. No se debe afirmar que el watchdog de WiFi, la caja lateral o la calibración nueva por bloques funcionan en el carro.
- B5 no tiene una medición numérica del dueño; el error lateral de ~20 a 24 cm es una estimación de navegación. B6 corrida 2 probablemente tocó la caja y la corrida 4 quedó ~40 cm a la derecha. La reincorporación después de esquivar sigue siendo un fallo real.
- La Autoridad del láser siempre encendida es una decisión del dueño, no una conclusión pareada. El láser ve un punto: puede ignorar un objeto central que la cámara ve pero queda fuera del haz. La cámara conserva la elección del lado y los obstáculos fuera del carril.
- J10c está corregido solo en simulación. Una caja muy ancha a más de 50 cm podría confundirse con un tapete lejano durante el aprendizaje; acercarse no garantiza que el detector de color la recupere. El láser podría verla si está habilitado y su haz la alcanza, pero falta comprobarlo.
- La verificación final de FW y APK en Sensores con el carro encendido y conectado quedó pendiente. El APK instalado se verificó por hash; el FW se conserva con la versión y el hash de la Etapa 4. Al cierre el punto de acceso del carro no estaba visible y no había puerto COM.
- El binario del firmware no se publica porque contiene la contraseña del WiFi. El repositorio publica el código fuente y la APK; para cargar el firmware hay que crear firmware_carro/secretos.h y reconstruirlo.
- La cámara puede dejar de responder en el puerto HTTP 80 aunque el ping siga funcionando. La recuperación observada fue apagar y encender la cámara y esperar unos 15 s.
- Los picos del loop de J13 no se volvieron a medir en J14. Siguen pendientes de aislar.
- El modelo de objetos y los modos opcionales conservan sus límites y pruebas de PC; no se validaron en esta sesión.

7. Incidentes y transparencia

- Dos capturas de la pantalla de rutas incluyeron por error la vista en vivo de la cámara. Se borraron inmediatamente y no se guardaron ni se publicaron. No se vieron ni se guardaron imágenes reales de la cámara; las capturas conservadas son solo de Sensores y Calibración.
- El intento de B6 a 40 cm no arrancó por la protección de aprendizaje del piso; se documenta como desviación del método, no como fallo del carro.
- El parche experimental de B2 no se instaló ni se publicó; B2 sigue NO MEDIDA.
- La verificación final de FW por Sensores no se pudo hacer por falta de conexión con el carro. No se inventa un resultado.

8. Estado del teléfono y del carro al cierre

- App cerrada con am force-stop.
- stay_on_while_plugged_in restaurado y comprobado en 2.
- Pantalla encendida como al inicio; WiFi habilitado; batería al 82 % y cargando por USB.
- El teléfono quedó en la red normal INFINITUM porque el punto de acceso CarroFSI no estaba visible; no se introdujo ninguna contraseña.
- El carro quedó quieto y apagado. No se dejaron movimientos, ni ayudantes de reset, ni procesos de prueba.
- No se escribió ni se guardó ningún PIN, contraseña o dato de desbloqueo.

9. Checklist antes de la demo

- Cambiar la contraseña del WiFi del carro.
- Restablecer las direcciones en Ajustes → Red.
- Volver a configurar el PIN.
- Poner pilas nuevas.
- Borrar la copia de OneDrive solo después de confirmar que el push a GitHub terminó bien.
- Encender el carro, conectar el teléfono a CarroFSI y comprobar en Sensores FW:453ed20-20261027 y App 0.4 (699fcc5).
- Aprender el piso en el lugar de la demo, con la zona despejada, y confirmar que la Autoridad del láser está encendida.
- Revisar batería del teléfono, cable y que la cámara responda antes de mover el carro.

10. Revisión de seguridad y del paquete

- firmware_carro/secretos.h no está versionado y no aparece en el historial; original_backup/ tampoco. Los dos están en .gitignore.
- No hay archivos versionados mayores de 15 MB en el árbol actual ni en los blobs del historial. Los archivos de dist/ también quedan por debajo de 15 MB.
- Todos los commits usan la identidad noreply de GitHub; no hay correo personal en el historial.
- La búsqueda literal de la clave actual encontró dos coincidencias incidentales dentro de un alfabeto hexadecimal que se usa para validar cadenas. No son una publicación de la contraseña, pero la clave actual es corta y debe cambiarse antes de la demo. No se reescribió el historial.
- El binario del firmware contiene la contraseña del WiFi como cadena independiente junto a CarroFSI. Por eso se retiró del commit de entrega antes del push: el repositorio publica el código fuente y la APK, no ese binario. La copia local de dist/ sí lo conserva y no se versiona.
- El dist/ publicado no incluye secretos.h, contraseñas ni datos de desbloqueo. El LEEME y la entrega explican que secretos.h se crea a partir de secretos.ejemplo.h.

11. Cierre

J14 queda cerrado con lo medido, lo no medido y los límites declarados. La entrega está en dist/ y el detalle de instalación y verificación está en docs/entrega.md. El dueño decidió detener las pruebas restantes; esas pruebas no se presentan como aprobadas.

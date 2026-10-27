J14 — Progreso del cierre de proyecto FSI

Inicio: 21 de octubre de 2026 (hora de Ciudad de México). Base: commit previo limpio en main.
Condiciones: DevKit conectado por USB (COM6) sin pilas AA y sin powerbank (carro sin capacidad de movimiento por hardware). Teléfono Huawei P40 Pro conectado por USB (modo transferencia de archivos), desbloqueado y con WiFi unido a CarroFSI. Autorización para carga de firmware e instalación de APK desatendida en Etapa A, sin comandos de movimiento.
Reglas permanentes respetadas: original_backup/ intacto; sin secretos ni archivos > 15 MB; identidad Git noreply; sin tocar PATH ni variables globales. Pruebas siempre en verde.

Fase A1: terminada.
- Calibración de giro en bloques del mismo lado: implementada en nucleo_c/calgiro.h y calgiro.c. Ahora ejecuta un bloque de 4 giros seguidos a la izquierda (el primero es calentamiento y no cuenta para la recta), una pausa con aviso en pantalla (CG_PAUSA = 7, CG_PAUSA_MS = 2000 ms), y un bloque de 4 giros a la derecha (el primero es calentamiento).
- Recta ajustada, descarte controlado, registro CALGIRO de los pares (ms, grados), guardado automático, Deshacer y ajuste fino de 90° restaurado a 1000 se conservan.
- Adaptación de giros posteriores actualizada: el giro 1 adapta los giros 2 y 3; el giro 5 adapta los giros 6 y 7.
- En app Android (CalGiroNativa.kt, CalibrarGiroActivity.kt, textos_calibrar_giro.xml y puente.c): soporte de estado PAUSA, aviso visual de pausa entre bloques, indicador de calentamiento en giros 0 y 4, y corrección de indexación de plan_ms en puente.c (l * CG_MEDICIONES_POR_LADO + i).
- Una estimación anterior anotó 61.0 cm de desplazamiento máximo y 42.9 cm al terminar. No se encontró el cálculo que sustente esas cifras para los ángulos reales de la calibración (40, 40, 70 y 100 grados por bloque), así que no se consideran verificadas. Falta medir el desplazamiento máximo de la secuencia nueva en el piso real.
- Pruebas en nucleo_c/pruebas_calgiro.c: 37 casos, 0 fallos. Cubre datos buenos, atípico al inicio de un bloque (giros 0 y 4), atípico en medio (giro 2 descartado), giros inconsistentes (rechazados con pares conservados) y cancelación a mitad de un bloque (estado CANCELADO con STOP inmediato). Todo nucleo_c verde (20 programas).

Fase A2: terminada.
- ANT no aprende de giros inválidos: implementado en firmware_carro/giro.c. El firmware descarta la actualización de anticipación si el giro terminó por timeout (emite ANT_IGNORADO:timeout), fue interrumpido por STOP/PARO (emite ANT_IGNORADO:interrumpido), quedó a más de 20° del pedido (emite ANT_IGNORADO:error_grande), o duró más de 1.5 veces la mediana reciente de ese lado y perfil con al menos 3 giros de historial (emite ANT_IGNORADO:giro_largo).
- Valor de ANT agregado a GIRO_OK/GIRO_TIMEOUT/GIRO_ERROR y propagado a través de puente.c, NavegadorNativa.kt, ControlLogica.kt, ModoSemiautonomo.kt y SensoresActivity.kt.
- Cada línea de registro TURN_DIAG incluye el campo ant=%d (en nucleo_c/navegador.c y CalibracionActivity.kt).
- Pruebas en simulador del firmware (pruebas_firmware/pruebas_sensores.c): 5 casos nuevos que validan giro normal que sí aprende, timeout, interrumpido, error grande y giro largo (> 1.5x mediana). Total pruebas_sensores: 246 casos, 0 fallos.
- Pruebas completas: nucleo_c (20 programas / 1570 casos), firmware (152+246+15+34 = 447 casos), estrés 100k comandos sin STOPs perdidos, servidor falso (42 casos) y verificador de versiones (7 casos). Todo verde.

Fase A3: terminada.
- Pantalla Sensores estable: se reservó altura fija en sens_resultado con android:minLines="2" en activity_sensores.xml para evitar saltos visuales al mostrar diagnósticos de giros.
- contentDescription e identificadores estables añadidos para automatización desatendida por adb:
  * Sensores (activity_sensores.xml): sens_recalibrar, sens_cero, sens_girar_izq, sens_girar_der, sens_paro (los switches ya contaban con contentDescription).
  * Calibración de giro (activity_calibrar_giro.xml): cg_empezar, cg_deshacer, cg_paro.
  * Iniciar / parar rutas: ej_iniciar, ej_paro, ej_cancelar en fragment_ejecutar.xml; rutas_ejecutar en fragment_rutas.xml.
- Compilación de APK Android verificada exitosamente; todas las suites de pruebas siguen en verde.

Fase A4: terminada.
- Loop del firmware documentado en docs/limites_conocidos.md sin tocar código de firmware.
- Se documentaron las mediciones de J13: reposo sin cliente 1.7 ms (0 excesos), en el aire con motores 3.25 ms (0 excesos), app conectada 6.8 a 8.4 ms (2 a 3 excesos), y arranque 12.8 a 12.9 ms (pico inicial transitorio con 1 exceso).
- Se documentó la hipótesis de que los excesos provienen del tráfico asíncrono y serialización de WebSocket de la app (PING, MODO_AUTO, telemetría SENS) y que los giros no se ven afectados, manteniendo la precisión requerida de +-3°.

Fase A5: terminada.
- J10c reproducido y corregido:
  * Se reprodujo con prueba sintética en nucleo_c/pruebas_pisos_dificiles.c donde una caja muy ancha en filas cercanas (filas 5 a 7, ancho casi completo) era aceptada indebidamente como segundo color de piso (grupos=3, descartado=0 %, muy_variado=0), haciendo que el detector no la viera como obstáculo.
  * Se implementó la corrección mínima en nucleo_c/obstaculos.c (obstaculos_aprender_terminar): un segundo color de piso solo se acepta como franja continua si ninguna de sus celdas toca las filas cercanas al carro (filas >= 7 de 10).
  * Con la corrección, la caja ancha se descarta (> 5 %, muy_variado = 1) y en detección cuenta como obstáculo en la zona centro. Los casos legítimos de dos colores (tapete lejos en filas 4 a 6) siguen pasando con 0 falsos positivos (32 casos, 0 fallos).
  * Compromiso y riesgos documentados en docs/limites_conocidos.md: obstáculos anchos a más de 50 cm que no toquen filas cercanas podrían asimilarse a tapetes lejanos si el láser está apagado (se resolverán al avanzar); y tapetes reales que comiencen exactamente en fila 7 sin tocar filas 8-9 exigirán recolocar el carro.

Fase A6: terminada.
- Modelo de objetos encendido por defecto:
  * En app_android/app/src/main/java/com/fsi/carro/ConfigVision.kt se fijó usarModelo por defecto en true (preferencias.getBoolean("usar_modelo", true)).
  * En CamaraObstaculos.kt se protegió activarModelo() para que cualquier fallo al instanciar o arrancar el hilo capture la excepción, registre el aviso/error y permita que el detector de color continúe operando transparentemente sin interrumpir la cámara ni la navegación.
  * Documentación actualizada en docs/fusion.md, docs/obstaculos.md y docs/limites_conocidos.md.

Fase A7 & A8: terminada.
- Rutas de prueba predefinidas en la app listas para ejecución directa y automatización desatendida por adb:
  * "Recta 150 cm": base de 8 tramos con override nativo a 1500 mm (navegador_poner_largo_recta_mm) para recorrido exacto de 150 cm.
  * "Recta 200 cm": 10 celdas de 20 cm = 200 cm exactos.
  * "Ruta en L (100 cm, giro 90° a la derecha, 100 cm)": 5 celdas adelante (100 cm), giro 90° derecha, 5 celdas a la derecha (100 cm).
  * "Prueba de obstáculo (1 m)": confirmada existencia (RUTA_PRUEBA_OBSTACULO = 5 celdas = 100 cm con freno inmediato y cancelación al detectar).
- Identificadores estables y automatización adb:
  * Botones en fragment_rutas.xml con IDs estables y contentDescription: rutas_predef_recta_150, rutas_predef_recta_200, rutas_predef_l, rutas_predef_obstaculo.
  * Lanzamiento por Intent ADB: --es ruta_predefinida <recta_150|recta_200|ruta_l|obstaculo> en MainActivity abre directamente EjecutarRutaFragment con la ruta elegida. Soporte opcional de --ez auto_iniciar true.
  * Control de parada por ADB: --es accion parar dispara SesionCarro.pararTodo("Paro por comando externo (ADB)").
  * Botones de parada y cancelación en EjecutarRutaFragment: ej_paro (contentDescription="Parar ruta") y ej_cancelar cancelan limpiamente enviando STOP inmediato y deteniendo el temporizador semiautónomo.
- Autoridad del láser (A8): mantenida apagada por defecto en Ajustes.kt (autoridad_laser = false) y ControlLogica.kt (autoridadLaser = false). J14: al final el dueño decidió dejarla siempre encendida; el valor por defecto de la app se cambió a true (ver cierre de Etapa 5).
- Pruebas unitarias de la recta predefinida añadidas en nucleo_c/pruebas_navegador.c (validando override de 1500 mm, tiempo con cal_cm_a_ms_tramo, término y distancia). Todas las pruebas en verde (380 casos nucleo_c).

Fase A9: terminada.
- Carga de firmware y verificación sin movimiento:
  * Firmware compilado para HEAD 69fbd1b y flasheado al DevKit en COM6 con scripts\cargar.ps1 nuevo (verify-flash habilitado y NVS intacta).
  * APK compilada en debug con versión APP:0.4-69fbd1b e instalada en el Huawei P40 Pro con adb install -r (sin desinstalar).
  * Ejecución de scripts\verificar_version.py: resultado PASA (Git HEAD 69fbd1b, Firmware 69fbd1b y APK 69fbd1b coinciden).
  * Sensores y hardware verificados en reposo absoluto sin movimiento: giroscopio MPU6050 ok (IMU=1, estado calibrado), sensor láser VL53L0X ok (TOF=1, listo), y anticipación ANT intacta en memoria no volátil (AUTO izq 126 ms, AUTO der 144 ms, MANUAL 20 ms).
  * Pantalla Sensores abierta por adb y validada; app cerrada con am force-stop y stay_on_while_plugged_in restaurado.
- Etapa A completa con 100 % de pruebas unitarias y suites en verde; sin ningún comando de movimiento enviado.

Auditoría J14, 27 de octubre de 2026 (base 96aede7):
Fase | Estado | Evidencia en código y commit
A1 | hecha | calgiro.c, pruebas_calgiro.c, CalibrarGiroActivity.kt; dfeab24.
A2 | hecha en firmware; integración falsa pendiente | giro.c, pruebas_sensores.c y navegador.c; 76f1d5f. El servidor falso no implementa ANT ni el formato actual de giro.
A3 | parcial | identificadores en activity_sensores.xml, activity_calibrar_giro.xml y fragment_ejecutar.xml; 5995775. minLines no fija la altura máxima del resultado.
A4 | hecha | limites_conocidos.md; 1877058.
A5 | hecha en simulación; pendiente prueba real | obstaculos.c y pruebas_pisos_dificiles.c; bce7b37.
A6 | hecha | ConfigVision.kt, CamaraObstaculos.kt y ModeloEnHilo.kt; 4364d27.
A7 | hecha en código; pendiente prueba real | RutasPredefinidas.kt, RutasFragment.kt y pruebas_navegador.c; 69fbd1b.
A8 | hecha | Ajustes.kt y ControlLogica.kt; 69fbd1b. La preferencia guardada puede prevalecer sobre el valor por defecto.
La carga de A9 es un registro histórico; no hay dispositivos conectados y no se conoce su estado actual.
Los registros CTest sin seguimiento se guardaron en la rama local j14-wip-20261027, commit af0bfb5.

A3, revisión del 27 de octubre: el resultado de Sensores reserva exactamente cuatro líneas y limita el texto al espacio disponible; los botones ya no cambian de posición cuando crece el mensaje. La APK compiló; falta comprobar el diseño en el teléfono.

Etapa 2, integración: servidor falso actualizado para CAPS con campos actuales, ANT?, ANT_BORRAR, ANT en las respuestas de giro y ANT_IGNORADO:interrumpido. La simulación mantiene ANT fijo y no reproduce el aprendizaje ni timeout; esos filtros se prueban en pruebas_sensores.c. Servidor falso: 46 casos, 0 fallos (antes 42).

A6, revisión del 27 de octubre: DetectorTflite carga el modelo en su constructor. Se movió su construcción dentro del try del hilo para registrar también un fallo al leer el archivo y mantener el detector de color. APK compilada; falta comprobar el caso en el teléfono.

A4, revisión del 27 de octubre: se precisa que la causa de los picos del loop es una hipótesis y que la ausencia de impacto se limita a lo observado en J13; no se cambió el firmware.

A5, revisión del 27 de octubre: la prueba sintética pasa, pero no se ha probado la caja real. Se corrigió el límite: si el color de una caja lejana ya se aprendió como piso, acercarse no garantiza que el detector de color la recupere.

A2, revisión del 27 de octubre: la app consulta ANT? al recibir CAPS y registra en SENS_DIAG tanto ANT como ANT_IGNORADO. Así se pueden leer esos datos sin USB en las pruebas físicas. APK compilada; falta verificarlo con los dispositivos.

Etapa 2, comprobaciones sin hardware del 27 de octubre:
- Núcleo: 20 programas, 1577 casos, 0 fallos (J13: 1570).
- Firmware en PC: 152 + 246 + 15 + 34 = 447 casos, 0 fallos (J13: 442). Estrés de 100000 comandos: 0 STOP perdidos; hash de página correcto.
- Servidor falso actualizado: 46 casos, 0 fallos (antes 42). Verificador de versiones: 7, 0 fallos.
- Compilación de Android: correcta; falta compilar de nuevo y calcular hashes del último commit. La compilación del firmware sigue en curso.
- Seguridad: secretos.h está ignorado y existe secretos.ejemplo.h. La búsqueda literal de la contraseña local encontró dos secuencias dentro de alfabetos hexadecimales en código, no asignaciones de credencial; la condición literal de cero coincidencias no se cumple. El nombre local de la red aparece en 14 archivos actuales y en cambios de 10 commits. El historial ya existía así en origin; no se reescribió y no se hizo push. Correos del historial: solamente noreply. Ningún archivo versionado supera 15 MB.
- Constantes: anticipación AUTO inicial 100 ms, máximo 250 ms, obstáculo láser 400 mm, watchdog 1000 ms y freno TOF 120 mm. TOF_USO:0 desactiva el freno del firmware según el comportamiento anterior; el interruptor de autoridad es independiente.
- Pendiente hardware: versiones y credenciales efectivas, NVS/ANT, sensores, pantalla, modelo ante fallo, rutas y las pruebas físicas B0-B10. No se ha cargado ni instalado nada en esta sesión.

A1, revisión del 27 de octubre: la estimación previa de 61 cm no tiene cálculo verificable para los ángulos realmente programados; falta medir la secuencia nueva en el carro.

Etapa 3: guía de conexión y parada preparada en docs/guia_conexion.md; COM6 es referencia histórica y se verificará la identidad de la única placa. No se ha conectado hardware.

Etapa 4, 27 de octubre de 2026: terminada sin movimiento.
- Una sola placa serie COM6; esptool confirmó que su identidad corresponde al DevKit registrado antes de cargar. Teléfono autorizado por ADB, pantalla encendida y stay_on_while_plugged_in=2 antes de la operación.
- Fuentes 453ed20: compilados FW:453ed20-20261027 y APP:0.4-453ed20. Firmware cargado con scripts/cargar.ps1 nuevo; verify-flash confirmó bootloader, particiones, boot_app0 y aplicación, sin borrado completo de flash ni ANT_BORRAR.
- SHA-256 firmware: 9FD1BB5103D1AD66652ADEA68B009D54D30B5C5B6F07F8CA7488A4FBF274048E. SHA-256 APK: 1FB3264EF0D9D8D91759F4A658B2763068B3AA0DE3DEA6E44DAF6A6DE0FA44C4.
- adb install -r: Success sin diálogos. verificar_version.py: firmware, APK instalada y HEAD 453ed20 coinciden; APK instalada idéntica al archivo compilado, fuentes versionadas limpias. Este registro posterior solo cambia documentos.
- ANT antes y después: 126,144,20,20 ms. CAPS IMU=1, TOF=1; IMU lista, sesgo -0.45 grados/s; TOF libre a 1200 mm (no es una prueba de distancia con referencia).
- La pantalla Sensores confirmó versiones, ambos sensores y autoridad láser apagada; los interruptores de giroscopio y distancia estaban encendidos. La app registró ANT por WiFi. Se reconectó el teléfono a la red guardada del carro tras el reinicio, sin contraseña.
- App cerrada, teléfono despierto y stay-on conservado en 2. Batería del teléfono 7 %, cargando por USB: cargar antes de pruebas físicas. No se mandó ningún comando de movimiento.

Etapa 5 B0, 27 de octubre, 12:43: pre-vuelo sin movimiento. DevKit sin USB; teléfono por USB y cargando con 9 % de batería. El teléfono estaba inicialmente en otra red; se reconectó a la red guardada del carro sin introducir contraseña. DevKit y cámara respondieron a ping. Pantalla Sensores: FW:453ed20-20261027 y APP:0.4-453ed20 (revisión del producto; HEAD posterior solo documentación), ambos sensores funcionan, interruptores de giroscopio y distancia encendidos, autoridad del láser apagada. ANT:126,144,20,20 ms. Pendiente colocación y LISTO antes de B1; no se ha movido el carro.

Etapa 5 B1, 27 de octubre: tras tres minutos en reposo, tres bloques de cuatro giros de 90° a la derecha, con al menos 20 s entre bloques y sin mover manualmente el carro entre ellos. Todos con resultado OK y dentro de ±3°.
- Bloque 1: 88.8°, 91.3°, 89.2°, 91.4°; suma 360.7°.
- Bloque 2: 90.7°, 89.3°, 90.7°, 90.1°; suma 360.8°.
- Bloque 3: 89.5°, 89.8°, 89.8°, 87.5°; suma 356.6°.
- ANT AUTO derecha: 144 ms antes y 142 ms después; los otros valores continuaron en 126, 20 y 20 ms. Relectura por la app: ANT:126,142,20,20; ambos sensores siguen respondiendo.
- Pendiente respuesta del dueño sobre el paralelismo del frente con la cinta. Carro quieto al concluir; ningún ángulo excedió el límite de aborto.
- Confirmación del dueño tras B1: el frente quedó prácticamente como al inicio y paralelo a la cinta; no pudo estimar un ángulo numérico.
- B2 quedó pendiente: la calibración instalada no publica los ángulos de cada giro hasta terminar los ocho, por lo que no se puede comprobar a tiempo el límite de aborto de ±25° entre giros. El carro permanece quieto y la app cerrada.
- Intento de protección adicional en una rama local: tras tres comprobaciones, las pruebas de calibración quedaron con un caso de inconsistencia en rojo. Se restauró el último commit bueno; la propuesta no validada se guardó en %TEMP%/fsi_j14_b2_seguridad_experimental.patch y no se instaló ni se publicó. B2 queda NO MEDIDA; se continúa con las comprobaciones que permiten vigilar cada giro.

Etapa 5 B3, 27 de octubre: giros de verificación con la calibración anterior, ya que B2 quedó sin medir. Dos bloques de cuatro por lado; los 16 terminaron OK y dentro de ±3°.
- Izquierda, bloque 1: 88.5°, 89.1°, 89.5°, 90.0°; bloque 2: 92.0°, 91.4°, 91.6°, 88.5°. Total 8/8.
- Derecha, bloque 1: 88.8°, 90.1°, 90.6°, 90.6°; bloque 2: 90.7°, 89.2°, 90.4°, 89.7°. Total 8/8.
- Carro quieto al terminar; sin giros por tiempo ni calibración B2.

Etapa 5 B4, corrida 1, 27 de octubre, 13:53: Autoridad APAGADA. Ruta «Recta 150 cm» terminada en 5 s, 0 esquivas, sin paros ni eventos STOP/TOF; el carro quedó quieto. Medición del dueño: 150 cm recorridos, desvío lateral ~15 cm a la derecha. A las 13:57 el dueño amplió el límite de 90 min; nuevo límite ~15:27.
- Corrida 2, 14:02, Autoridad APAGADA: 150 cm recorridos, desvío lateral ~20 cm; 5 s, 0 esquivas, sin paros.
- Intento de corrida 3, 14:04: no arrancó porque la cámara no respondió en la preparación; el carro no se movió. Reintento a las 14:05 correcto.
- Corrida 3, 14:05, Autoridad ENCENDIDA: 5 s, 0 esquivas, sin paros ni eventos STOP/TOF; medición del dueño: 5.5 cm de desvío, distancia correcta (150 cm).
- Corrida 4, intentos 14:08 y 14:10: no arrancó; la cámara dejó de entregar video y luego su servidor HTTP (puerto 80) dejó de responder aunque el ping seguía. El carro no se movió. Se pidió apagar y encender la cámara.
- Corrida 4, 14:12, Autoridad ENCENDIDA: 5 s, 0 esquivas, sin paros ni eventos STOP/TOF; medición del dueño: ~18 cm de desvío, distancia correcta (150 cm).
- B4 completo: APAGADA 15 y 20 cm; ENCENDIDA 5.5 y 18 cm; 4/4 rutas terminadas, 0 falsos paros, 0 esquivas.

Etapa 5 B5, intento 1, 14:15: la ruta en L llegó al punto 6/10 (100 cm y giro de 90° hecho) pero el arnés de prueba cortó al leer «Detenido», un estado pasajero entre tramos; no cuenta como corrida. Se corrigió el arnés para no cortar por ese texto y esperar a Terminado/Error o 1.5 s con la ruta parada.
- Intento 2, 14:20: el arnés cortó durante la cuenta 3-2-1; el carro no se movió. Se corrigió para no contar la cuenta como ruta parada.
- Corrida 1 válida, 14:21, giroscopio ENCENDIDO: terminó en 17 s, 1 esquiva, giro derecho ~90°, 4 giros por giroscopio. El dueño indica que al final de la L había un sillón; la esquiva fue por ese obstáculo. Posición final estimada x=1418 mm, y=189 mm, rumbo=324°; el dueño la vio «relativamente bien», sin cifra. Se repite con la zona despejada.
- Corrida 1 limpia, 14:24, giroscopio ENCENDIDO: terminó en 7 s, 0 esquivas, giro derecho pedido 90° hecho ~91°; posición final estimada x=1406 mm, y=209 mm, rumbo=91°. Falta la medición del dueño.
- Corrida 2, 14:26, giroscopio ENCENDIDO: terminó en 8 s, 0 esquivas, giro ~90°; posición final estimada x=1389 mm, y=203 mm, rumbo=90°. Falta la medición del dueño.
- Corrida 3, 14:27, giroscopio APAGADO: terminó en 7 s, 0 esquivas, giro por tiempo pedido 90° hecho ~93°; posición final estimada x=1394 mm, y=244 mm, rumbo=93°. Falta la medición del dueño.

Etapa 5 B6, intento a 40 cm, 14:31: la app no arrancó; el aprendizaje del piso detectó la caja («Hay algo frente al carro», piso muy variado) y pidió despejar. El carro no se movió. Es una protección de la app; se repite con la caja más lejos y se anota la desviación del método. El dueño aclara después que en todas las corridas siguientes la caja estuvo a ~100 cm del frente, no a 70 cm.
- Corrida 1, 14:33, Autoridad APAGADA, caja a ~100 cm (aclarado por el dueño): el carro no la embistió; hizo 2 esquivas, terminó los 200 cm en 23 s y quedó en x=394 mm, y=5 mm, rumbo=117°. El dueño confirma que no la tocó, pero vio que al volver a la ruta detectó un obstáculo lejano y no logró reincorporarse.
- Corrida 2, 14:35, Autoridad APAGADA, caja a ~100 cm (aclarado por el dueño): 3 esquivas y terminó en «Necesito ayuda: toma el control» en el punto 3/10 (x=659 mm, y=780 mm, rumbo=306°), sin completar la ruta. El dueño cree que sí tocó la caja.
- Corrida 3, 14:38, Autoridad ENCENDIDA, caja a ~100 cm (aclarado por el dueño): 26 s, 2 esquivas, ruta terminada; posición final estimada x=393 mm, y=-12 mm, rumbo=119°. El dueño confirma que no tocó la caja; quedó cerca del objetivo pero mirando hacia abajo-izquierda (rumbo final ~119° en vez de 0°).
- Corrida 4, 14:40, Autoridad ENCENDIDA, caja a ~100 cm (aclarado por el dueño): 15 s, 1 esquiva, ruta terminada; posición final estimada x=409 mm, y=6 mm, rumbo=0°. El dueño confirma que no tocó la caja, pero quedó unos 40 cm a la derecha de la línea recta.
- B6 completo (4 corridas): APAGADA 2 (corrida 1 sin tocar; corrida 2 probable toque y terminó en «Necesito ayuda»), ENCENDIDA 2 (sin tocar; 2 y 1 esquivas). El dueño decidió que a partir de ahora todas las pruebas y la entrega serán con la Autoridad del láser ENCENDIDA; no se harán más corridas con ella apagada. Se anota como decisión del dueño y desviación del plan B7/B10.
- Observación del dueño sobre la esquiva: el carro tenía espacio para hacer el rodeo limpio (90°, 50 cm, 90°, 50 cm, 90° y volver a la línea), pero en la corrida 2 se enredó detectando obstáculos lejanos durante la reincorporación. Es un fallo real del algoritmo de vuelta a la ruta, no falta de espacio.

Etapa 5, cierre por decisión del dueño, 27 de octubre de 2026, ~14:50:
- B7 no se ejecutó como prueba separada. El dueño indicó que las corridas de B6 ya se hicieron con la caja a ~100 cm y que no hacía falta repetirlas; no se hicieron las 3 corridas con Autoridad encendida que pedía el plan.
- B8 (caja lateral a 25 cm) y B9 (corte de WiFi) no se ejecutaron: el dueño decidió que ya no eran necesarias.
- B2 (calibración nueva por bloques) no se ejecutó: queda NO MEDIDA.
- B10: el dueño decidió que la Autoridad del láser quede siempre ENCENDIDA. No hubo una comparación B4-B8 con ella apagada y encendida suficiente para decidir por datos; la decisión es del dueño y así se documenta. Se cambió el valor por defecto de la app a encendido.
- Resumen físico J14: B4 4 corridas, B5 3, B6 4 = 11 corridas; B0-B3 sin movimiento; B7-B9 y B2 no realizadas. El carro quedó quieto, la app cerrada y el WiFi del teléfono reactivado.
- El dueño no dio cifras de error final para B5; se conservan las estimaciones de navegación (x=1406/1389/1394 mm, y=209/203/244 mm, rumbo 91/90/93°) y su valoración cualitativa.

Etapa 6, cierre documental y entrega, 27 de octubre de 2026, ~15:10:
- Se corrigió el texto de la pantalla Sensores: la Autoridad del láser ya no dice «apagado por defecto»; ahora explica que el dueño decidió dejarla siempre encendida y que la comparación pareada quedó pendiente.
- La APK final se compiló desde el commit 699fcc5, después de un build limpio. Versión APP:0.4-699fcc5, 14,500,144 bytes, SHA-256 C50A9757DE59B1B604680BE9731D8C610F9EC4A66780EB18ECEB558E6E00312A. Se instaló con adb install -r: Success. La APK extraída del teléfono tiene el mismo hash. Sensores muestra App 0.4 (699fcc5), los dos sensores encendidos y la Autoridad del láser encendida.
- El firmware de entrega es FW:453ed20-20261027, 1,031,712 bytes, SHA-256 9FD1BB5103D1AD66652ADEA68B009D54D30B5C5B6F07F8CA7488A4FBF274048E. Se conserva el binario de la Etapa 4. La lectura final por Sensores no se pudo repetir: el punto de acceso CarroFSI no estaba visible y no había puerto COM. No se inventa ese resultado.
- Se actualizaron README.md, docs/reporte_J14.md, docs/entrega.md, docs/limites_conocidos.md, docs/validacion_sensores.md, docs/progreso.md y docs/bitacora.md. El reporte final dice expresamente que el dueño decidió detener B7, B8, B9 y B2.
- Se creó dist/ local con el binario del firmware, el APK, SHA256SUMS.txt, LEEME.txt y FIRMWARE_LOCAL.txt. Los archivos pesan menos de 15 MB. El APK de dist/ se versiona con una excepción en .gitignore. El binario del firmware NO se publica porque contiene la contraseña del WiFi como texto: se retiró del commit de entrega antes del push y queda solo en la copia local. SHA256SUMS.txt cubre solo la APK; FIRMWARE_LOCAL.txt tiene el hash del firmware y explica cómo reconstruirlo.
- Pruebas finales: núcleo 20 programas / 1577 casos / 0 fallos; firmware 152 + 246 + 15 + 34 = 447 / 0; estrés de 100000 comandos sin STOP perdido; servidor falso 46 / 0; verificador de versiones 8 / 0; hash de página correcto. La app compila y la APK instalada coincide con dist/.
- El teléfono quedó con la app cerrada por am force-stop, stay_on_while_plugged_in=2, pantalla encendida, WiFi habilitado y batería al 82 % cargando por USB. Quedó en la red normal INFINITUM porque CarroFSI no estaba visible; no se escribió ninguna contraseña ni PIN.
- El carro quedó quieto y apagado. No se dejaron ayudantes de reset ni procesos de prueba.
- Revisión de seguridad: firmware_carro/secretos.h y original_backup/ no están versionados ni aparecen en el historial; no hay archivos >15 MB en el árbol ni en los blobs; todos los commits usan identidad noreply. La búsqueda literal de la clave actual encontró dos coincidencias incidentales dentro de un alfabeto hexadecimal de validación; no se publica la contraseña, pero es corta y debe cambiarse antes de la demo. No se reescribió el historial.
- Push a main: publicado en origin/main en el commit c593910. GitHub Actions «sanitizadores», ejecución 11: success. El tag v1.0 se crea sobre el commit de cierre documental posterior a c593910.

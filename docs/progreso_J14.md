J14 — Progreso del cierre de proyecto FSI

Inicio: 21 de octubre de 2026 (hora de Ciudad de México). Base: commit previo limpio en main.
Condiciones: DevKit conectado por USB (COM6) sin pilas AA y sin powerbank (carro sin capacidad de movimiento por hardware). Teléfono Huawei P40 Pro conectado por USB (modo transferencia de archivos), desbloqueado y con WiFi unido a CarroFSI. Autorización para carga de firmware e instalación de APK desatendida en Etapa A, sin comandos de movimiento.
Reglas permanentes respetadas: original_backup/ intacto; sin secretos ni archivos > 15 MB; identidad Git noreply; sin tocar PATH ni variables globales. Pruebas siempre en verde.

Fase A1: terminada.
- Calibración de giro en bloques del mismo lado: implementada en nucleo_c/calgiro.h y calgiro.c. Ahora ejecuta un bloque de 4 giros seguidos a la izquierda (el primero es calentamiento y no cuenta para la recta), una pausa con aviso en pantalla (CG_PAUSA = 7, CG_PAUSA_MS = 2000 ms), y un bloque de 4 giros a la derecha (el primero es calentamiento).
- Recta ajustada, descarte controlado, registro CALGIRO de los pares (ms, grados), guardado automático, Deshacer y ajuste fino de 90° restaurado a 1000 se conservan.
- Adaptación de giros posteriores actualizada: el giro 1 adapta los giros 2 y 3; el giro 5 adapta los giros 6 y 7.
- En app Android (CalGiroNativa.kt, CalibrarGiroActivity.kt, textos_calibrar_giro.xml y puente.c): soporte de estado PAUSA, aviso visual de pausa entre bloques, indicador de calentamiento en giros 0 y 4, y corrección de indexación de plan_ms en puente.c (l * CG_MEDICIONES_POR_LADO + i).
- Desplazamiento máximo nuevo documentado: en la secuencia alternada anterior el centro del carro se alejaba hasta 97.8 - 103.4 cm (acumulando deriva en x); con la secuencia nueva en bloques de 4 giros del mismo lado, los giros cierran sobre sí mismos y luego se revierten, reduciendo el desplazamiento máximo a 61.0 cm (en el giro 7) y terminando a 42.9 cm de la partida.
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
- Autoridad del láser (A8): mantenida apagada por defecto en Ajustes.kt (autoridad_laser = false) y ControlLogica.kt (autoridadLaser = false).
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

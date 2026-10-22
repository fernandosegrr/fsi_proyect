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

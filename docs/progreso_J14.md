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

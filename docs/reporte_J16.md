Reporte J16 — Precisión de navegación (solo software)

31 de octubre de 2026. Rama j16-precision (no fusionada a main, sin etiquetas). Nada se probó en el carro: todo lo de este reporte está SIMULADO o revisado en el código. El carro no tiene encoders: siempre habrá patinaje y variación por pilas; ningún cambio elimina el error.

1. Errores encontrados

Corregidos:
- ALTA, app (app_android/app/src/main/cpp/puente.c y ModoSemiautonomo.kt): el largo fijo de la «Recta 150 cm» nunca llegaba al núcleo: cargarRuta llama a navegador_iniciar, que lo ponía en 0. La ruta hacía las 8 celdas: 160 cm. Probablemente así corrieron las rectas B4 de J14.
- ALTA (latente, aparece al arreglar el anterior), núcleo (navegador.c, planificar): con largo fijo, cualquier plan nuevo con índice mayor que 1 terminaba la ruta (por ejemplo, al volver de esquivar a mitad de la recta). Al corregirlo apareció otro caso: tras el tramo de 1500 mm la pose (sin el frenado) quedaba a 1465 mm y se planeaba hacia un punto ya pasado (giro de 135° de regreso). Ahora se cuenta el frenado, se saltan los puntos pasados y el avance se recorta al largo fijo.
- ALTA (falta de diseño), firmware (carro.c): ningún control de rumbo en las rectas; las asimetrías de los lados se convierten en curva. Se agregó RUMBO (sección 2).
- MEDIA, firmware (giro.c): el filtro «giro_largo» de la anticipación mezclaba giros de cualquier tamaño. Tras tres giros chicos (correcciones de 5 a 29° o reintentos de 45°) todo giro de 90° parecía largo y la anticipación dejaba de aprender hasta reiniciar.
- MEDIA, núcleo (navegador.c, calcular_fin_obstaculo): la holgura al terminar de pasar el obstáculo (100 mm) es poca para el giro final de la vuelta: en la simulación la cola del carro (24 cm) barría la cara de la caja al pivotar en 0.9 % de los esquives con J15 y 4.9 % con J14 (B6-2: «probablemente tocó la caja»).
- MEDIA, núcleo (navegador.c, navegador_aporte_giro y navegador_aporte_imu): cuando un giro por ángulo cae a tiempo ENTRE dos ticks (GIRO_ERROR sin datos o respuesta parcial a IMU?), el tick siguiente contaba 50 ms de giro que no ocurrieron (la app manda el LEFT/RIGHT en ese tick): giro ~5 a 9° corto sin que la pose lo supiera.
- MEDIA, app (ModoSemiautonomo.kt): las rutas predefinidas se reconocían por el nombre: una ruta propia con «150» recibía el largo fijo y una con «obstáculo» apagaba «pedir ayuda» y se cancelaba al primer frenado. Ahora tienen que coincidir las celdas.
- BAJA, núcleo (navegador.c, calcular_avance_medido): la pose medida con el giroscopio usaba grados enteros: un rumbo de 0.4° no movía la x (hasta ~1 cm en 150 cm).
- BAJA, app (ControlLogica.kt): la respuesta a IMU? se pasaba al navegador aunque el giroscopio no estuviera listo.
- BAJA, app (ModoSemiautonomo.kt): el comando del tick (también el STOP) se mandaba después de hasta 16 llamadas de registro (1 a 5 ms).
- Documental: los «20 a 24 cm» de error de la ruta en L (bitácora, límites, progreso, reporte J14, validación) no son una medición: comparaban la pose estimada (y = 209, 203 y 244 mm) contra y = 0, pero el último tramo de la L está en y = 200 mm. El error estimado era de 0.3 a 4.4 cm y el físico sigue NO MEDIDO. Se agregaron notas de corrección.
- Documental: protocolo.md tenía mal el formato de CAPS, el orden de IMU? y la regla de descarte de la cola; corregido.
- Simulador (fidelidad): trasladaba con el rumbo truncado a grados enteros; corregido en el modelo J16. Durante el trabajo se corrigieron también errores de mi propia extensión (LEFT/RIGHT que se filtraban durante un GIRAR con latencia, estado del pivote no reiniciado entre dos GIRAR seguidos, tiempo de la respuesta a IMU? distinto del de la app, dependencias del Makefile).

Encontrados y NO cambiados (documentados en limites_conocidos.md):
- MEDIA, firmware: STOP y el fin de un giro dejan las ruedas libres (no hay freno activo del TC1508A); ~15° de cada giro de 90° son inercia que corrige la anticipación. Cambiarlo invalidaría todas las calibraciones sin medición.
- MEDIA, firmware: el arranque de 250 ms a 255/255 ignora el balance (patada de rumbo en cada arranque). Con RUMBO se compensa (también en el arranque); sin RUMBO queda como antes.
- MEDIA, firmware: el balance solo baja el lado izquierdo, es el mismo en reversa y tiene resolución de una cuenta de PWM (~4 cm en 150 cm). RUMBO corrige en los dos sentidos.
- MEDIA, firmware: el watchdog no avisa a la app (la pose sigue sumando por tiempo).
- MEDIA, app: nadie vigila el silencio del enlace (sin tiempo de lectura ni ping); con WiFi degradado la pose sigue integrando.
- MEDIA, app (puente.c): la rejilla del último cuadro (ultimo_*) la escribe el hilo de análisis y la lee el tick sin candado; afecta decisiones de obstáculos, no la pose.
- MEDIA, app: TOF_PARO no corta el heartbeat y el uso del láser se decide una vez por ruta mientras TOF_USO sigue al interruptor.
- BAJA, firmware: no hay calibración de escala del giroscopio (±3 % típico); pesa en los giros de 180°.
- BAJA, firmware: el cierre del giro va a saltos de una muestra (~1.3 a 1.7° a 130 °/s); el watchdog lo renuevan también MODO_*, BAL_* y texto desconocido (las pruebas lo fijan así).

2. Cambios, banderas y cómo revertir

- RUMBO en el firmware (firmware_carro/rumbo.c y rumbo.h, carro.c, config.h, sensores.c): control proporcional-integral del rumbo al avanzar con el giroscopio. Mensajes RUMBO:ON, RUMBO:OFF, RUMBO:<n> y RUMBO? (protocolo.md, sección 4b). Apagado al arrancar y en cada conexión; solo actúa con RUMBO:ON, el giroscopio listo y avanzando. CAPS anuncia RUMBO=1. Constantes en config.h (RUMBO_KP_MIL_GRADO 10, RUMBO_TI_MS 1500, RUMBO_MAX_MIL 150), elegidas en simulación. Bandera: el interruptor «Sostener el rumbo al avanzar» de la pantalla Sensores (encendido por defecto, Ajustes.usarRumbo); la app solo manda RUMBO si el firmware lo anuncia. Revertir: apagar el interruptor, o no cargar el firmware nuevo.
- Objetivos de rumbo del núcleo (navegador.c: navegador_poner_rumbo_fw, navegador_tomar_rumbo_fw, rumbo_deseado_x10): sobre la línea apunta a un punto 40 cm adelante (NAV_RUMBO_MIRA_MM) sin pasar de 12° (NAV_RUMBO_CORR_MAX); en las diagonales tras un pivote grande apunta al punto de parada; en el paso en paralelo, el rumbo de la línea; en la vuelta a la línea solo sostiene (rumbo_en_vuelta = 0). Con RUMBO, las correcciones de hasta 8° (NAV_RUMBO_FW_SIN_GIRO) se hacen avanzando, sin el giro parado. Banderas: la misma de RUMBO (sin ella nada de esto actúa); sin_volver_a_linea = 1 deja solo el rumbo del tramo; NAV_RUMBO_FW_SIN_GIRO = 4 vuelve al umbral de antes.
- Recta con largo fijo (navegador.c y puente.c): arreglo de los dos errores de arriba; con rumbo medido se planea por celdas (así se corrige el rumbo en cada una). Sin bandera (corrige un error); revertir con git revert.
- Holgura al pasar el obstáculo de 100 a 150 mm (NAV_PASO_MARGEN_MM). Bandera: sin_margen_paso_j16 = 1 vuelve a 100.
- dt tras caer a giro por tiempo entre ticks. Bandera: sin_arreglo_dt_giro = 1.
- Pose medida con décimas de grado. Bandera: sin_trig_fino = 1.
- Filtro giro_largo por grado (giro.c). Bandera: GIRO_LARGO_POR_GRADO 0 en config.h.
- App: rutas predefinidas por celdas; STOP antes del registro; IMU? solo con el giroscopio listo. Sin bandera (correcciones); revertir con git revert.
- Probado y descartado (no quedó en el código): descontar el retraso del video y el frenado de la distancia a la cara del obstáculo. Acortaba el paso en paralelo y empeoraba el esquive simulado sin ganar precisión.
- Las banderas sin_* son campos del Navegador (navegador_iniciar los pone en 0); se cambian en el código. Volver a todo J15: git switch main (main no tiene J16), la APK de J15 del historial (carro_fsi_0.4-074c9fd.apk) y el firmware 453ed20.

3. Resultados en simulación (1000 corridas por escenario y configuración, semilla fija, corridas pareadas; error en mm: media / p95 / máximo)

Métrica: desvío lateral final en rectas y esquives; distancia al punto final en las rutas con giros. «ANTES» es el software actual (J15) con el firmware de siempre; «sin firmware» es J16 con el firmware de siempre; «con RUMBO» es J16 con el firmware nuevo. «J14» es lo que corrió en las pruebas físicas, para la calibración.

- Recta 100 cm: ANTES 46 / 73 / 95; sin firmware 45 / 73 / 95; con RUMBO 9 / 23 / 33.
- Recta 150 cm: ANTES 80 / 126 / 161 (y 11.2 cm de error de largo medio, porque hacía 160 cm); sin firmware 74 / 117 / 151 (largo: 3.4 cm); con RUMBO 13 / 34 / 50 (largo: 3.1 cm). J14: 137 / 232 / 279.
- Recta 200 cm: ANTES 99 / 161 / 218; sin firmware 99 / 161 / 218; con RUMBO 19 / 52 / 90. J14: 210 / 347 / 410.
- Giro 45°: ANTES 56 / 107 / 145; sin firmware 56 / 107 / 145; con RUMBO 36 / 69 / 94.
- Giro 90°: ANTES 44 / 82 / 122; sin firmware 44 / 83 / 122; con RUMBO 32 / 64 / 84.
- Giro 180°: ANTES 45 / 79 / 107; sin firmware 45 / 81 / 107; con RUMBO 43 / 92 / 139. La cola empeora: tras la media vuelta el rumbo estimado queda hasta ~2.5° mal (escala del giroscopio de ±1 %) y RUMBO dirige con precisión hacia una línea mal estimada. Falta calibrar la escala del giroscopio.
- Ruta en L: ANTES 44 / 86 / 132; sin firmware 44 / 87 / 132; con RUMBO 36 / 72 / 100. J14: 146 / 233 / 284.
- Cuadrado de 60 cm: ANTES 35 / 80 / 127; sin firmware 35 / 80 / 126; con RUMBO 28 / 63 / 93.
- Esquive caja 20 x 20: ANTES 37 / 92 / 137 con 9 toques; sin firmware 33 / 78 / 137 con 0 toques; con RUMBO 21 / 56 / 84 con 0 toques. J14: 200 / 324 / 408 con 49 toques.
- Esquive caja 40 x 20: ANTES 36 / 86 / 141 con 8 toques; sin firmware 33 / 79 / 123 con 0 toques; con RUMBO 21 / 55 / 82 con 0 toques.
- Rumbo final (giros): con RUMBO el error medio de rumbo al terminar baja de 3.5° a 1.1° (giro 90°) y de 3.0° a 1.1° (L).
- Todas las configuraciones terminan 1000 de 1000 (salvo J14 en la caja ancha: 998 y 2 ayudas).
- Calibración (configuración J14): los desvíos medidos en B4 caen en los percentiles 9, 57, 73 y 82 del simulador; los ~40 cm de B6-4, en el percentil 99. Detalles en docs/precision/simulador.md, datos en docs/precision/monte_carlo_1000.txt y calibracion_j14.txt.

4. Pruebas de software

- Núcleo: 22 programas, 1630 casos, 0 fallos (los 21 de antes con sus 1598 casos más pruebas_precision_j16 con 32: réplica de RUMBO contra rumbo.c en 20000 pasos, 5 propiedades de la pose y comprobaciones de no empeorar con 150 corridas por escenario).
- Firmware: 509 casos, 0 fallos (los 447 de antes más pruebas_rumbo con 62: controlador, propiedades con 20000 casos, integración con carro.c y el filtro giro_largo). Estrés de 2 hilos y hash de la página: correctos. En pruebas_sensores se actualizaron 3 casos que comparan el texto exacto de CAPS (ahora lleva RUMBO=1).
- Verificador: 18 pruebas (13 + 5 del modo --solo-archivo).
- UBSan local (modo trampa) sobre el banco J16, el navegador y pruebas_rumbo: sin fallos. ASan/TSan en GitHub Actions (sanitizadores.yml, extendido a las pruebas nuevas): ver el estado de la ejecución en la rama.

5. Validación física pendiente

Nada de J16 está medido en el carro. Plan en docs/precision/plan_pruebas.md: línea base con el firmware de siempre, carga del firmware nuevo, rectas pareadas con y sin RUMBO, escala del giroscopio, L, cuadrado y esquives. Los criterios de aceptación salen de la simulación con margen.

6. Versiones y estado

- Rama j16-precision publicada en origin, sin fusionar a main y sin etiquetas. v1.0 sigue en eb88151.
- Código: commit d823a63. APK: APP:0.4-d823a63, 14,508,012 bytes, SHA-256 76DB84867344808E140EDA7328600B5F9F55CE93DDDAFD23004880C7AC80484D (dist/), compilada desde ese commit con build limpio; verificar_version.py --solo-archivo: PASA. No instalada.
- Firmware: FW:d823a63-20261031, 1,033,312 bytes, SHA-256 D955B69D0ACE616B0E591789250B8B0054ECD705E3EA3DEAB4898FBE98849FA8, compilado en la PC; NO cargado ni publicado (contiene la contraseña del WiFi; copia local en dist/, excluida de Git).
- Volver a v1.0: git switch --detach v1.0 (o git switch main) y cargar el firmware 453ed20.

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
- MEDIA, firmware: el watchdog no avisa a la app (la pose sigue sumando por tiempo). Atendido en la segunda ronda (sección 7).
- MEDIA, app: nadie vigila el silencio del enlace (sin tiempo de lectura ni ping); con WiFi degradado la pose sigue integrando. Atendido en la segunda ronda (sección 7).
- MEDIA, app (puente.c): la rejilla del último cuadro (ultimo_*) la escribe el hilo de análisis y la lee el tick sin candado; afecta decisiones de obstáculos, no la pose. Atendido en la segunda ronda (sección 7).
- MEDIA, app: TOF_PARO no corta el heartbeat y el uso del láser se decide una vez por ruta mientras TOF_USO sigue al interruptor. El corte del heartbeat y la parada de la ruta cuando el navegador no atiende el TOF_PARO se agregaron en la segunda ronda (sección 7).
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

Cifras de la primera ronda (código d823a63). La segunda ronda solo cambió la configuración con RUMBO: sus cifras están en la sección 7 y en docs/precision/monte_carlo_1000.txt.

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
- UBSan local (modo trampa) sobre el banco J16, el navegador y pruebas_rumbo: sin fallos. GitHub Actions «sanitizadores» (extendido a las pruebas nuevas), ejecución 41224593460 sobre 3c9ebcf: success en todos los pasos (ThreadSanitizer con el estrés de 2 hilos; AddressSanitizer + UndefinedBehaviorSanitizer de sensores y carro, navegador, RUMBO J16, precisión J16 con 300 corridas y reincorporación J15): https://github.com/fernandosegrr/fsi_proyect/actions/runs/41224593460

5. Validación física pendiente

Nada de J16 está medido en el carro. Plan en docs/precision/plan_pruebas.md: línea base con el firmware de siempre, carga del firmware nuevo, rectas pareadas con y sin RUMBO, escala del giroscopio, L, cuadrado y esquives. Los criterios de aceptación salen de la simulación con margen.

6. Versiones y estado

- Rama j16-precision publicada en origin, sin fusionar a main y sin etiquetas. v1.0 sigue en eb88151.
- Primera ronda: commit d823a63, APK APP:0.4-d823a63 (14,508,012 bytes, SHA-256 76DB84867344808E140EDA7328600B5F9F55CE93DDDAFD23004880C7AC80484D) y firmware FW:d823a63-20261031 (1,033,312 bytes, SHA-256 D955B69D0ACE616B0E591789250B8B0054ECD705E3EA3DEAB4898FBE98849FA8). Los reemplaza la segunda ronda (sección 7); la APK queda en el historial de Git.
- Vigente: ver sección 8 (tercera ronda); la segunda ronda está en la sección 7.
- Volver a v1.0: git switch --detach v1.0 (o git switch main) y cargar el firmware 453ed20.

7. Segunda ronda (31 de octubre de 2026, misma rama)

Pedido del dueño: revisar qué más se puede mejorar en la navegación autónoma o en el software. Sigue siendo solo software: nada se probó en el carro ni en el teléfono.

Errores encontrados y corregidos:
- MEDIA, núcleo (navegador.c, fijar_pose, calcular_pose y sumar_frenado): con RUMBO, cada comando nuevo empezaba con el rumbo redondeado a grados enteros: hasta 0.5° de error al empezar cada plan, que RUMBO después sostenía con precisión. Ahora la pose lleva rumbo_x10 (décimas, 0 a 3599) y rumbo_grados es su redondeo. Solo con RUMBO activo; sin RUMBO la pose sigue en grados enteros como en J15 (lo fija la propiedad B7).
- MEDIA, app (ControlLogica.kt): tras TOF_PARO el heartbeat seguía repitiendo el FORWARD. El firmware ya había frenado, así que el siguiente heartbeat lo arrancaba otra vez contra el obstáculo y el láser lo frenaba de nuevo (tirones). Ahora se deja de repetir el FORWARD.
- MEDIA, app (ModoSemiautonomo.kt): si el láser frenaba el carro y el navegador no lo atendía (una ruta que empezó sin láser y el láser volvió a funcionar a mitad), la ruta seguía contando un avance que no ocurría. Ahora se detiene con un mensaje.
- MEDIA, firmware y app (sensores.c, carro.c, ModoSemiautonomo.kt): el watchdog frenaba sin avisar; la ruta seguía sumando por tiempo y el siguiente heartbeat arrancaba el carro desde otro lugar. El firmware nuevo avisa WD:<n> y la ruta se detiene.
- MEDIA, app (ModoSemiautonomo.kt): nadie vigilaba el silencio del enlace (OkHttp sin tiempo de lectura). Ahora, con un firmware que contestó CAPS, 1.5 s sin SENS detienen la ruta.
- MEDIA, app (puente.c): la rejilla del último cuadro (ultimo_*) y la máscara de visión se copiaban entre el hilo de la cámara y el ciclo de 50 ms sin candado; el navegador las usa para decidir (el carril del láser, las columnas del borde al esquivar). Ahora un pthread_mutex protege solo las copias.
- BAJA, app (CalibracionGuardada.kt, EjecutarRutaFragment.kt, Mensajes.kt): si el navegador rechazaba la calibración guardada solo quedaba una línea en el registro y la ruta usaba otros valores. Ahora la pantalla de la ruta lo avisa antes de salir.
- BAJA, pruebas del firmware (pruebas_carro.c): la prueba de estrés de 2 hilos fallaba de vez en cuando: 8 de 120 corridas con la PC ocupada, también con el código original. Dos causas del arnés, no del firmware: esperaba 500 ms el aviso del último giro (de cuando el asentado duraba 200 ms; ahora GIRO_ASENTAR_MS = 750) y en «paso a paso» el productor colaba varias órdenes en una misma vuelta, sus GIRO_ERROR:ocupado llenaban la cola de salida (16) y se tiraba algún GIRO_OK. Tras el arreglo: 0 fallos en 300 corridas en paralelo.

Diagnóstico para las pruebas físicas:
- Al terminar o cancelar una ruta con RUMBO, la app pregunta RUMBO? antes de apagarlo. SENS_DIAG registra la respuesta (firmware: RUMBO:<encendido>,<en marcha>,<objetivo x10>,<corrección milésimas>,<aprendido milésimas>) y cuántos objetivos se mandaron (ruta: rumbo_objetivos_enviados=N). La corrección aprendida es el desbalance de los lados que encontró el firmware: es el dato que falta medir.
- La pantalla Sensores dice si el firmware tiene RUMBO, si las rutas lo van a usar y el último estado leído.

Cambios, banderas y cómo revertir:
- Rumbo con décimas entre comandos: sin_trig_fino = 1 (todo en grados enteros) o RUMBO apagado (interruptor de Sensores).
- Corte del heartbeat con TOF_PARO: ControlLogica.CORTAR_HEARTBEAT_EN_TOF_PARO = false.
- Detener la ruta sin SENS, con WD o con un TOF_PARO no atendido: ModoSemiautonomo.VIGILAR_CARRO_J16 = false (el umbral es SILENCIO_SENS_MS = 1500).
- Aviso WD del firmware: sin bandera (una app anterior lo ignora); revertir con git revert. El servidor falso también lo manda con --sensores.
- Candado de puente.c, aviso de calibración rechazada, RUMBO? al terminar y arreglo de la prueba de estrés: sin bandera (correcciones); git revert.

Resultados en simulación (1000 corridas, la misma semilla; media / p95 / máximo en mm; antes = primera ronda). Solo cambia la configuración con RUMBO; las otras 30 líneas (J14, ANTES y sin firmware) quedaron idénticas:
- Recta 100 cm: 9 / 23 / 33 → 8 / 19 / 27.
- Recta 150 cm: 13 / 34 / 50 → 10 / 26 / 35 (error de largo medio 3.1 → 3.2 cm).
- Recta 200 cm: 19 / 52 / 90 → 14 / 33 / 49.
- Giro 45°: 36 / 69 / 94 → 34 / 66 / 83.
- Giro 90°: 32 / 64 / 84 → 31 / 61 / 90 (el máximo sube 6 mm).
- Giro 180°: 43 / 92 / 139 → 41 / 88 / 139.
- Ruta en L: 36 / 72 / 100 → 35 / 71 / 93.
- Cuadrado de 60 cm: 28 / 63 / 93 → 26 / 59 / 78.
- Esquive caja 20 x 20: 21 / 56 / 84 → 17 / 40 / 63, 0 toques.
- Esquive caja 40 x 20: 21 / 55 / 82 → 17 / 40 / 60, 0 toques.
- Rumbo final medio: giro 90° de 1.1° a 0.9°; L de 1.1° a 0.8°.
- Todas terminan 1000 de 1000, sin ayudas. Es SIMULADO: no prueba que el carro mejore.
- Lo nuevo de la app (enlace mudo, WD, TOF_PARO, candado) no está en el banco: el simulador no modela la app ni el WiFi. NO MEDIDO.

Pruebas de software:
- Núcleo: 22 programas, 1633 casos, 0 fallos. El banco J16 suma 3 propiedades: B6 (con RUMBO la pose guarda las décimas, ±0.1°), B7 (sin RUMBO, grados enteros como J15, en cada tick) y B8 (con el giroscopio paseando, rumbo_x10 en 0..3599 y a medio grado o menos de rumbo_grados). Se comprobó que B6 falla con la función apagada.
- Firmware: 515 casos, 0 fallos (6 nuevos del aviso WD: un giro largo no lo dispara, avisa WD:1 una vez a la app y al puerto serie, no se repite parado, WD:2 en el segundo freno, reiniciar no inventa un aviso). Estrés de 2 hilos: 0 fallos en 300 corridas en paralelo (antes 8 de 120).
- Servidor falso: 48 casos (2 nuevos: WD con --sensores y sin él no). Verificador: 18.
- UBSan local (modo trampa) sobre pruebas_sensores, pruebas_carro, el banco J16 y pruebas_navegador: sin fallos.
- GitHub Actions «sanitizadores», ejecución 41243986227 sobre 0d1d4c1: success en todos los pasos (ThreadSanitizer con el estrés de 2 hilos ya arreglado; AddressSanitizer + UndefinedBehaviorSanitizer de sensores y carro, navegador, RUMBO, precisión J16 y reincorporación J15): https://github.com/fernandosegrr/fsi_proyect/actions/runs/41243986227
- La APK compila (build limpio). No hay pruebas automáticas de Kotlin en la PC: lo nuevo de la app está revisado en el código, NO probado en el teléfono.

Versiones (vigentes):
- Código: commit 097008d.
- APK: APP:0.4-097008d, 14,511,016 bytes, SHA-256 F0A037CB37C203A8F44279B91DD8D2061665ECC03DAC45FD347AD523AE78842A (dist/), compilada desde ese commit con build limpio; verificar_version.py --solo-archivo: PASA. No instalada.
- Firmware: FW:097008d-20261031, 1,033,440 bytes, SHA-256 D7CEC2A9A07F3597798C7AF8603A007D4555D3A28B3DB12756D3D88A9AA64F2C, compilado en la PC con cargar.ps1 nuevo -SoloCompilar; NO cargado ni publicado (copia local en dist/, excluida de Git).
- Rama j16-precision sin fusionar a main y sin etiquetas; v1.0 sigue en eb88151.

8. Tercera ronda (31 de octubre de 2026): escala del giroscopio

Pedido del dueño: corregir también la escala del giroscopio, lo que dejaba la cola del giro de 180° con RUMBO. Solo software; nada probado en el carro ni en el teléfono.

Qué había: el MPU6050 promete la escala con ±3 %. Un giroscopio que lee 1 % de más deja cada giro de 180° unos 1.8° corto y el rumbo estimado mal en todo lo que sigue; con RUMBO el carro sigue con precisión esa línea mal estimada.

Cambios:
- Firmware (imu.c, sensores.c, config.h, hw_sensores.h, firmware_carro.ino): la velocidad ya sin sesgo se multiplica por (1 + ppm/1 000 000) antes de integrar; con 0 ppm el resultado es idéntico al de antes (todas las pruebas anteriores pasan sin cambios, salvo las 3 que comparan el texto exacto de CAPS). IMU_ESCALA:<ppm> (de -50000 a 50000, solo con el carro detenido y sin giro en curso) la guarda en la memoria no volátil; IMU_ESCALA? la informa; CAPS la anuncia como IMUS=<ppm>. La corrección vale para el giro por ángulo, SENS y RUMBO a la vez.
- Núcleo (calgiro.c): calgiro_escala calcula la corrección con lo que midió el giroscopio en vueltas completas alineadas con una línea, compuesta con la que ya tenía el firmware. Rechaza menos de una vuelta, lo que no cuadra con vueltas enteras (más del 5 %) y correcciones de más de ±5 %.
- App: tarjeta «Escala del giroscopio» en Sensores. Empezar (guarda el ángulo con el carro quieto), girar dos vueltas completas al mismo lado (a mano sin levantarlo, o con los botones de 90°), alinear otra vez y Terminé; muestra la corrección y «Guardar en el carro» la manda y espera la confirmación. «Quitar la corrección» vuelve a 0. Todo queda en SENS_DIAG (escala_giroscopio ...). Solo con un firmware que anuncia IMUS.
- Simulador: el error de escala se lleva en partes por millón (fw_escala_ppm); con 0 los números de siempre salen idénticos (40 de 40 líneas del Monte Carlo). Configuración nueva «con RUMBO y escala calibrada»: el ±1 % se cambia por un residuo de ±0.3 % (SUPUESTO: ±1° al alinear al empezar y al terminar dos vueltas), sacado de la semilla para conservar el emparejamiento.
- Servidor falso: con --sensores anuncia IMUS, entiende IMU_ESCALA y SIM_ANGULO (para probar la tarjeta sin el carro).

Banderas y cómo revertir: con la corrección en 0 todo es como antes («Quitar la corrección» en Sensores o IMU_ESCALA:0); un firmware anterior no la tiene y la app no le manda nada. El resto, con git revert.

Resultados en simulación (1000 corridas, la misma semilla; media / p95 / máximo en mm; «con RUMBO» contra «con RUMBO y escala calibrada»):
- Giro 180°: 41 / 88 / 139 → 35 / 68 / 100.
- Cuadrado de 60 cm: 26 / 59 / 78 → 21 / 43 / 69 (rumbo final medio 2.3° → 1.3°).
- Giro 90°: 31 / 61 / 90 → 29 / 59 / 86. Giro 45°: 34 / 66 / 83 → 33 / 65 / 81.
- Ruta en L: 35 / 71 / 93 → 34 / 68 / 94.
- Rectas y esquives: prácticamente igual (recta 150 cm 10 / 26 / 35 → 10 / 26 / 37; esquive caja ancha 17 / 40 / 60 → 17 / 39 / 65).
- Todas 1000 de 1000, sin toques ni ayudas. Es SIMULADO y depende del residuo supuesto: cuánto ayuda en el carro depende de qué tan bien se alinee al medir (NO MEDIDO).

Pruebas de software:
- Firmware: 540 casos (25 nuevos: la escala sobre el ángulo y la velocidad, el recorte, que reiniciar el chip no la borra, los mensajes y sus rechazos, que no se cambia en marcha ni en un giro, que no renueva el watchdog, la memoria no volátil al encender y GIRAR:180 con un giroscopio 2 % alto: 176.0° reales sin corregir y 180.8° con la corrección, en el modelo de prueba).
- Núcleo: 22 programas, 1655 casos (11 de calgiro_escala, con una propiedad de 20000 casos; 11 comprobaciones nuevas en el banco J16).
- Servidor falso: 54 casos. Verificador: 18. UBSan local sin fallos.
- La app compila (build limpio); la tarjeta nueva está revisada en el código, NO probada en el teléfono.

Versiones (vigentes):
- Código: commit 873bfb9.
- APK: APP:0.4-873bfb9, 14,520,520 bytes, SHA-256 50179013BCB18029F6C1BCBFE737F59A9B14B49B7BE848E002D67C119433C15B (dist/), compilada desde ese commit con build limpio. No instalada.
- Firmware: FW:873bfb9-20261031, 1,034,112 bytes, SHA-256 5B00529D21999F1BB8441D265D673D12CD163D3A3F59F5E1EFBB468C88AA6D6A, compilado en la PC con cargar.ps1 nuevo -SoloCompilar; NO cargado ni publicado (copia local en dist/, excluida de Git).
- Rama j16-precision sin fusionar a main y sin etiquetas; v1.0 sigue en eb88151.

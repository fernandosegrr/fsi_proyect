J13 — Reporte de las pruebas físicas con movimiento

21 de octubre de 2026, 20:03 a 20:25, hora de Ciudad de México. Firmware FW:adbdd13-20261021, app APP:0.4-adbdd13. Carro en el piso con pilas y powerbank, DevKit sin USB, teléfono controlado por adb. Solo se pulsaron 90° izq., 90° der. y PARO en la pantalla Sensores (perfil AUTO 210). Sin cámara, sin video, Autoridad del láser apagada. Los ángulos son los del giroscopio del carro; no se verificaron con cinta.

Resumen
- Calibración de giros (corrida 1, corrida 2, Deshacer): NO MEDIDO. No se ejecutó: la secuencia alterna lados y el desplazamiento calculado llega a 61.8 cm tras el giro 6 y a 97.8 cm tras el giro 8 (límite 60 cm). Desplazamientos acumulados por giro, en cm: 6.2, 16.9, 23.1, 33.9, 43.9, 61.8, 74.5, 97.8.
- Giros a la izquierda: PASA. 12 de 12 con estado OK; 10 de 10 y 12 de 12 dentro de ±3°. Media 90.16°, desviación 1.06°, mínimo 88.5°, máximo 91.8°, sobregiro +0.16°. Bloques: 360.1°, 360.3°, 361.5°.
- Giros a la derecha: PASA con observación. 12 de 12 con estado OK; 9 de los primeros 10 y 11 de 12 dentro de ±3° (el giro 21 dio 94.0°). Media 90.32°, desviación 1.71°, mínimo 88.4°, máximo 94.0°, sobregiro +0.32°. Bloques: 357.6°, 361.3°, 364.9°.
- Primer giro tras reposo: no atípico en esta prueba. Izquierda 91.5° (los otros 11: media 90.04°); derecha 89.2° (los otros 11: media 90.42°). Un solo giro por reposo y signos contrarios: no concluyente.
- Duración: izquierda 696 ms de media (638 a 748); derecha 810 ms (758 a 870).
- Comparación con J12: la derecha pasó de una media de 91.8° (sin el primer giro) a 90.42°. Posible causa, sin confirmar: tope de anticipación de 250 ms y arranque en 100 ms.
- LOOP: NO MEDIDO desde teléfono (requiere USB). Anticipación (ANT): NO MEDIDO (la app no la registra); ANT? por USB queda pendiente.
- Conexión: sin pérdidas, PING de 6 a 30 ms.

Datos por giro
- Giro 1, izquierda, bloque 1: 91.5 grados, 718 ms (primero tras reposo de 3 min). Anticipación: no aparece en el log.
- Giro 2, izquierda, bloque 1: 90.2 grados, 672 ms. Anticipación: no aparece en el log.
- Giro 3, izquierda, bloque 1: 88.5 grados, 667 ms. Anticipación: no aparece en el log.
- Giro 4, izquierda, bloque 1: 89.9 grados, 729 ms. Anticipación: no aparece en el log.
- Giro 5, izquierda, bloque 2: 89.5 grados, 638 ms (primero de bloque tras la pausa). Anticipación: no aparece en el log.
- Giro 6, izquierda, bloque 2: 90.6 grados, 683 ms. Anticipación: no aparece en el log.
- Giro 7, izquierda, bloque 2: 90.6 grados, 686 ms. Anticipación: no aparece en el log.
- Giro 8, izquierda, bloque 2: 89.6 grados, 748 ms. Anticipación: no aparece en el log.
- Giro 9, izquierda, bloque 3: 91.8 grados, 747 ms (primero de bloque tras la pausa). Anticipación: no aparece en el log.
- Giro 10, izquierda, bloque 3: 89.5 grados, 644 ms. Anticipación: no aparece en el log.
- Giro 11, izquierda, bloque 3: 91.4 grados, 726 ms. Anticipación: no aparece en el log.
- Giro 12, izquierda, bloque 3: 88.8 grados, 699 ms. Anticipación: no aparece en el log.
- Giro 13, derecha, bloque 1: 89.2 grados, 764 ms (primero tras reposo de 3 min). Anticipación: no aparece en el log.
- Giro 14, derecha, bloque 1: 90.7 grados, 806 ms. Anticipación: no aparece en el log.
- Giro 15, derecha, bloque 1: 88.5 grados, 850 ms. Anticipación: no aparece en el log.
- Giro 16, derecha, bloque 1: 89.2 grados, 793 ms. Anticipación: no aparece en el log.
- Giro 17, derecha, bloque 2: 89.3 grados, 758 ms (primero de bloque tras la pausa). Anticipación: no aparece en el log.
- Giro 18, derecha, bloque 2: 89.3 grados, 820 ms. Anticipación: no aparece en el log.
- Giro 19, derecha, bloque 2: 91.3 grados, 870 ms. Anticipación: no aparece en el log.
- Giro 20, derecha, bloque 2: 91.4 grados, 828 ms. Anticipación: no aparece en el log.
- Giro 21, derecha, bloque 3: 94.0 grados, 758 ms (primero de bloque tras la pausa). Anticipación: no aparece en el log.
- Giro 22, derecha, bloque 3: 90.0 grados, 808 ms. Anticipación: no aparece en el log.
- Giro 23, derecha, bloque 3: 88.4 grados, 858 ms. Anticipación: no aparece en el log.
- Giro 24, derecha, bloque 3: 92.5 grados, 812 ms. Anticipación: no aparece en el log.

Desviaciones del plan
- Pausa entre bloques de la izquierda: unos 2 min (bloque 1 a 2) y 1 min (2 a 3) en vez de 20 s, por las interrupciones de mi guion de apoyo. A la derecha: 26 s y 25 s. Reposo de 3 min antes de la izquierda (20:05 a 20:08) y de 3 min entre lados (20:14:48 a 20:17:51), como se pidió.
- El giro 6 lo pulsé a mano; el giro 17 se leyó del logcat (el guion no lo vio). Ambos terminaron con estado OK.
- Mi guion tuvo tres fallos de herramienta (no del producto): filtro de logcat con dos niveles del mismo tag; toques que no se registraron porque el texto de resultado desplaza los botones unos 80 px; conteo de líneas de logcat sobre un buffer circular. Efecto: PARO más cierre de la app tras los giros 1, 4 y 5 (el carro ya estaba quieto) y reapertura de Sensores. Nunca se envió un giro con otro en curso, y la numeración va de 1 a 24 sin repetidos. Corregido en el guion con deslizar al fondo y logcat -c antes de cada toque.

Anomalías, causa más probable y prueba que la confirma (sin arreglar nada)
- Botones que se mueven al crecer el texto de resultado (de una línea a dos). Causa: diseño de la pantalla sin altura reservada en el texto. Se confirma con la captura antes y después, ya hecha, y con minLines fijo.
- Derecha más larga y más dispersa que la izquierda. Causa: más inercia a la derecha, como en J12. Se confirma con ANT? por USB y diez giros a la derecha con cinta.
- Bloques de la derecha creciendo (357.6°, 361.3°, 364.9°). Causa: anticipación AUTO adaptándose durante la prueba. Se confirma leyendo ANT? antes y después de 12 giros.
- Giro 21 a 94.0°, primero de un bloque tras la pausa. Causa no determinada (puede ser ruido; los otros primeros de bloque dieron 89.5, 91.8 y 89.3). Se confirma repitiendo con más muestras.

Qué falta
- Calibración de giros con un área libre mayor, o reposicionando el carro entre giros alternados, o con otra secuencia en bloques del mismo lado (cambio de diseño, pendiente).
- Medir con cinta el ángulo real y la deriva del carro; no se midió dónde quedó el carro tras los 24 giros.
- LOOP con el carro en movimiento y ANT? por USB.

Cierre: PARO, am force-stop com.fsi.carro, stay_on_while_plugged_in=2 (valor original) comprobado, pantalla encendida como al inicio, sin wake-locks, sin procesos de la app. Sin video ni imágenes de cámara; solo capturas de la pantalla Sensores, en el directorio temporal de la sesión y no versionadas. No se cambió código ni firmware ni se instaló nada.

Lectura por USB posterior a los giros, 21 de octubre de 2026, 20:36 a 20:38 (actualiza los puntos «ANT» y «LOOP» de arriba).
- El dueño conectó el DevKit a la PC con un cable corto; por eso el carro no se movió y solo hice consultas de lectura por COM6 (única placa serie), con DTR y RTS apagados. La placa sí se reinició al conectar el USB: ARR cambió y el LOOP volvió a mostrar el pico de arranque.
- 20:36:41: CAPS:IMU=1,TOF=1,TOFR=0,IMUR=0,IMUM=0,IMUE=2,FW:adbdd13-20261021; IMU:0,-47,2,104,0 (sesgo -0.47 grados/s, sin cambio); TOF:1200,1 (libre); ANT:126,189,20,20; ARR:5,2,11000,0,5,0,00000,0; LOOP:12856,1.
- ANT (AUTO izquierda, AUTO derecha, MANUAL izquierda, MANUAL derecha): antes de los giros 121,150,20,20; ahora 126,189,20,20. La izquierda subió 5 ms y la derecha 39 ms; MANUAL no cambió (no se usó) y ninguno llegó al tope de 250 ms. La derecha ya salió del tope anterior de 150 ms, lo que apoya (no demuestra) que el sobregiro derecho bajó de 1.8° a 0.3° por eso. No explica que las sumas por bloque a la derecha crecieran (357.6°, 361.3°, 364.9°): con más anticipación se esperaría menos sobregiro. Solo tengo el valor final guardado, no su trayectoria durante la prueba: esa anomalía sigue sin causa determinada.
- LOOP: el de los 24 giros NO se pudo leer, porque el reinicio al conectar el USB borró los contadores (viven en RAM). El pico inicial de 12.856 ms con un exceso de 5 ms aparece en los tres arranques observados (12.858, 12.935 y 12.856 ms): es del arranque, no de la marcha. Tras LOOP_CERO, en reposo, con la app cerrada y sin cliente WiFi: 1684 us a los 15 s, 1717 us a los 30 y 60 s, cero excesos. LOOP con el carro en marcha y la app conectada: NO MEDIDO; hace falta conectar el USB antes de mover el carro (cable largo, o carro con las ruedas en el aire) y no reiniciarlo.
- Sobre el ángulo real: el dueño indica que el giroscopio es muy exacto; los ángulos de arriba se toman como reales, sin que yo los haya comprobado con cinta.
- Sin cambios en firmware ni en NVS. LOOP_CERO solo pone a cero dos contadores en RAM. Sin movimiento.

LOOP con el carro en el aire y la app conectada, 21 de octubre de 2026, 20:48 a 20:51 (actualiza el punto «LOOP en marcha» de arriba). El dueño dejó el carro sobre un soporte con las ruedas en el aire y el USB conectado; solo se pulsaron 90° izq. y 90° der. de la pantalla Sensores (giro por ángulo).
- Fase A, app conectada y carro en reposo, 60 s tras LOOP_CERO: LOOP:8431,2 (máximo 8.431 ms, dos excesos de 5 ms). Antes, con la app conectada, ya se había visto 8.000 ms y cuatro excesos; sin cliente WiFi, en reposo: 1.717 ms y cero excesos.
- Fase B, 36 s con la app conectada y cinco giros (cuatro a la izquierda y uno a la derecha): LOOP:3251,0 (máximo 3.251 ms, cero excesos) con los motores encendidos unos 9 s en total. Lectura final: la misma, LOOP:3251,0.
- Giros 1 a 4 (izquierda): GIRO_TIMEOUT a 1850 ms, con 1.1, 0.3, 0.4 y 0.0 grados medidos, como se espera con las ruedas en el aire. Giro 5 (derecha): GIRO_TIMEOUT a 1850 ms pero con 71.3 grados medidos: el carro sí giró. Causa no determinada (puede haber rotado sobre su soporte o una rueda tocó algo; el giro izquierdo no lo hizo). Abortó el guion: PARO, am force-stop. Faltan los giros 6 a 8 y el movimiento real del soporte está por revisar por el dueño.
- ANT no cambió (126,189,20,20): GIRO_TIMEOUT no corrige la anticipación, solo GIRO_OK. IMU:211,-47,2,104,0 al final (ángulo 21.1 grados, el giroscopio sigue vivo). Las consultas ANT? y LOOP? del inicio de esta prueba no recibieron respuesta (None) y luego todo respondió; causa no determinada, no se repitió.
- Lectura: el pico de LOOP (8 ms, dos excesos por minuto) aparece con la app conectada y el carro en reposo, y en los 36 s con motores encendidos no apareció. Es una muestra corta, así que el límite de 5 ms NO se da por pasado con la app conectada. Causa más probable: el tráfico WebSocket de la app (PING y MODO_AUTO cada pocos segundos, flujo SENS), no los motores. Se confirmaría con ventanas largas (10 min) con la app conectada en reposo contra el mismo tiempo con motores, y con WiFi sin cliente (ya medido: 1.7 ms).
- Cierre: app cerrada con force-stop, stay_on_while_plugged_in=2 comprobado, sin cambios en firmware ni NVS.

LOOP con el carro en el piso y el cable USB de 1 m, 21 de octubre de 2026, 20:54 a 20:56 (actualiza el punto «LOOP en marcha» de arriba). Carro en el piso con 1 m despejado, DevKit por USB a la PC, app conectada en Sensores, LOOP_CERO antes de empezar. Bloque de cuatro giros a la izquierda y después cuatro a la derecha (el segundo bloque no se llegó a hacer).
- Giros a la izquierda: 85.9, 94.4 y 91.6 grados (642, 706 y 674 ms), todos OK. Giro 4: GIRO_TIMEOUT a 1850 ms con solo 24.3 grados medidos de 90: el carro no completó el giro. Causa más probable: el cable USB se atoró o se tensó (el carro se movió más lejos o se enredó), o una rueda quedó trabada. Se confirmaría revisando cómo quedó el cable y repitiendo con más holgura. Se abortó por la regla de 25 grados: PARO y am force-stop. Los motores estuvieron como máximo 1850 ms atascados por el límite del firmware.
- LOOP tras 3 giros OK y 1 giro atascado, con la app conectada: LOOP:6775,3 (máximo 6.775 ms, tres excesos de 5 ms) en unos 40 s con la app conectada y motores encendidos unos 8 s. Con el carro en el aire: 3.251 ms y cero excesos; en reposo con la app conectada: 8.431 ms y dos excesos. Con tan pocos datos no se puede atribuir los excesos a los motores, a la app o al cable; el límite de 5 ms NO pasa en esta prueba.
- ANT sin cambios (126,189,20,20) tras los tres giros OK; IMU:3635,-47,2,104,0 (el ángulo acumulado del giroscopio, sin importancia). Las consultas ANT? e IMU? de una pasada no recibieron respuesta y a la siguiente sí; ya había pasado antes, causa no determinada.
- Cierre: app cerrada con force-stop, stay_on_while_plugged_in=2, sin cambios en firmware ni NVS. No se repitió el giro 4 ni se hizo el bloque de la derecha. Pendiente: que el dueño revise la posición del carro y del cable.

Segundo intento en el piso con el cable USB, 21 de octubre de 2026, 20:57 a 21:00 (sustituye la causa probable del giro atascado de arriba). El dueño había acomodado el cable y confirma que durante el bloque de la derecha agarraba el carro con la mano.
- Bloque a la izquierda, con LOOP_CERO antes: 90.3, 88.0, 91.3 y 86.5 grados (661, 631, 673 y 695 ms), todos OK; suma 356.1. LOOP tras ese bloque: LOOP:6783,2 (máximo 6.783 ms, dos excesos de 5 ms, unos 35 s con la app conectada y motores encendidos unos 3 s).
- Bloque a la derecha: giro 5 OK con 86.8 grados pero en 1196 ms (lo normal es unos 800 ms) y giro 6 GIRO_TIMEOUT con 79.1 grados: ambos NO son válidos como medida del giro porque el carro estaba sujeto con la mano. Se abortó por la regla de estado distinto de OK. El LOOP no cambió después de la izquierda (6783,2).
- Efecto en la anticipación: el giro 5 terminó con GIRO_OK y corrigió ANT con un dato falso: ANT pasó de 126,189,20,20 a 126,144,20,20 (la derecha AUTO bajó 45 ms). La izquierda quedó igual tras sus cuatro giros válidos. Esa anticipación derecha de 144 ms es un valor contaminado, menor que los 189 ms aprendidos con giros válidos; los siguientes giros a la derecha se pasarán más hasta que se reaprenda. No se tocó (ANT_BORRAR escribiría en NVS y no se autorizó). Se corregiría con dos o tres bloques de cuatro giros válidos a la derecha, sin tocar el carro.
- Con esta explicación, el atasco del primer intento (giro 4, 24.3 grados) también pudo ser por manipular el carro o el cable; no se confirmó cuál.
- LOOP en el piso, resumen de las tres mediciones con la app conectada: carro en el aire 3.251 ms y cero excesos; reposo 8.431 ms y dos excesos; piso 6.783 ms y dos excesos. Siguen siendo pocos datos: el límite de 5 ms NO pasa y la causa se mantiene como el tráfico de la app (sin demostrar).
- Cierre: app cerrada con force-stop, stay_on_while_plugged_in=2. Sin cambios en firmware; la NVS cambió solo por el aprendizaje normal de ANT.

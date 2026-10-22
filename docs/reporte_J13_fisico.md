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

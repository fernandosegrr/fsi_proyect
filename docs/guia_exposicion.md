# Guía de exposición — Carro FSI v1.2.5 (1 de diciembre de 2026)

- **Versión:** v1.2.5 — autónomo v3 con correcciones del 26-nov; validación física parcial; arranque con sombras por láser (probado en el carro, 3 corridas); demostración con pilas nuevas sin frenado validado a esa velocidad. Firmware en el carro `FW:b396390` (`SEG=3`, `TOFP=150`, `TOF_BLOQUEO=180`); cámara `cam-v2`; app `0.4-1792ab0` (`navegador.c`, `ManualFragment.kt` y segunda opinión del láser en `aprenderPiso`, umbral `600 mm`); 1 fallo conocido en `pruebas_sim_ruta`.
- **Decisión para la exposición (Opción 2: demostración con pilas AA nuevas y calibración nueva, incluyendo obstáculos):** el 27-nov-2026 el equipo decidió exponer el modo autónomo v3 con pilas AA alcalinas nuevas y la calibración nueva, incluyendo obstáculos blandos. Con cronómetro se midió que el carro recorre `1.5 m` en unos `3 s` (`46` a `50 cm/s`) sin pasarse. **Plan B:** el mismo firmware v3 con el autónomo apagado (modo manual o semiautónomo); si el carro va a golpear, se pasa a manual desde la app o se apaga. **No usar v1.1 como respaldo seguro:** su paro a 120 mm deja solo 1.5 a 3.5 cm de margen y puede golpear el sensor.
- **Condiciones operativas de la demostración:** pilas AA alcalinas nuevas y un juego de repuesto; piso con **al menos 60 cm libres al frente según el láser** (`OBS_APRENDER_TOF_LIBRE_MM = 600` mm en `aprenderPiso`); obstáculo blando (caja de cartón o cojín), nunca algo frágil; encender primero la *powerbank* con el carro quieto 3 s para calibrar el giroscopio y luego conectar las pilas; revisar cables antes de salir; confirmar la recta de `1.5 m` en unos `3 s`. **Frenado a 46–50 cm/s — NO MEDIDO:** el frenado validado (`10` de `10` sin contacto, margen de `4.5` a `6.5 cm` desde la punta del láser) se midió a unos `28 cm/s` con pilas ya gastadas; a `46–50 cm/s` la distancia de frenado crece más que proporcionalmente y ese margen puede no alcanzar (posible toque suave; el 27-nov se midió `esquive a 46–50 cm/s: 3 corridas, 1 con toque`, lo cual no es validación completa).
- **Cómo usarla:** léanla completa una vez. El guion (sección 1) se dice en voz alta, las preguntas (sección 2) están agrupadas por quién las responde según el reparto de la sección 3, y la sección 4 es para cuando algo falle en vivo. Cada respuesta cierra con `(Manual: X.n)`, la sección del manual que la explica.
- **Manual:** [`manual/Manual_Proyecto_Carro_FSI.pdf`](manual/Manual_Proyecto_Carro_FSI.pdf). Lo que sigue abierto está en [`pendientes_confirmar.md`](pendientes_confirmar.md).
- **Etiquetas:** **VALIDADO EN EL CARRO**, **SIMULADO**, **NO MEDIDO** y **PENDIENTE** significan lo mismo que en el manual; la sección 5 las resume.

## 1. Guion de 3 a 5 minutos

Para decir en voz alta. Las marcas de tiempo son una guía: si el profesor interrumpe, sigan desde el bloque donde se quedaron.

**0:00 · Qué es y para qué**

Somos el equipo del Carro FSI (LIDIA), de la materia Fundamentos de los Sistemas de Información, en la Universidad de Guanajuato, DICIS. Es un carro de cuatro ruedas mecanum que debe seguir una ruta dibujada en un teléfono Android y rodear obstáculos. Lo hicimos con hardware de bajo costo: sin LiDAR industrial y sin encoders. Hoy explicamos cómo funciona, cómo lo probamos y qué todavía no hemos probado.

**0:30 · Los tres nodos**

El sistema tiene tres partes. El carro es una ESP32 DevKit: mueve los motores, lee un giroscopio y un láser, y crea la red WiFi «CarroFSI». La cámara es otra placa, una Freenove ESP32-WROVER, y solo manda imágenes. El teléfono lleva la app en Kotlin y un núcleo de navegación en C, que la app llama por JNI.

**1:00 · La red y quién manda**

La red «CarroFSI» la crea el carro, no usa internet, y la cámara y el teléfono se unen a ella. El teléfono decide y manda las órdenes por un WebSocket. El carro obedece y se protege solo. La cámara manda video, unos doce cuadros por segundo. Lo separamos así para que frenar nunca dependa del teléfono.

**1:30 · Cómo ve**

Usamos dos detectores. El de color aprende en un segundo cómo se ve el piso libre y marca lo que ya no se parece. Una red neuronal, EfficientDet-Lite0, dibuja cajas sobre objetos: es un modelo ya entrenado de los ejemplos de TensorFlow Lite y solo lo ejecutamos. Pedimos que coincidan los dos, porque se equivocan distinto.

**2:00 · Cómo decide**

El láser VL53L0X mide la distancia al frente y confirma un obstáculo a menos de 400 milímetros. Con todo eso decide el navegador, escrito en C. Sigue la ruta; si algo estorba, se detiene, rodea y vuelve a la línea. Si no puede rodear con seguridad, no insiste: se queda quieto y nos pasa el control con una cruceta.

**2:30 · Cómo se protege (1)**

El freno de emergencia es del láser, no de la cámara, porque el video llega unos 320 milisegundos tarde y la luz lo engaña. Si dos lecturas seguidas marcan menos de 150 milímetros, la ESP32 corta los motores sin esperar al teléfono, y no deja arrancar bajo 180 milímetros. Eso es lo que tiene grabado el carro hoy (`FW:b396390`, `SEG=3`), medido en 10 de 10 frenados sin contacto.

**3:00 · Cómo se protege (2)**

Si pasa un segundo sin órdenes, el carro apaga los motores solo; la app repite la orden cada 250 milisegundos. La app cancela la ruta si falta el video o los datos del carro, y hay un botón rojo de PARO. El interruptor del portapilas corta la energía de los motores.

**3:30 · Cómo lo probamos**

Probamos en tres niveles. Pruebas automáticas en PC y un simulador que repite mil corridas por escenario con semillas fijas. Y en el carro real el 25 y 26 de noviembre con `FW:b396390`: medimos velocidad en AUTO (28.7 cm/s, `n=5`), frenado láser (10/10 sin contacto), deriva del giroscopio (0.0° en 10 min) y, tras las correcciones del 26-nov, mochila negra (3/3 sin contacto) y objeto bajo de 6 cm (2/2 tras el ajuste).

**4:00 · Lo que NO hemos probado**

Lo importante: las corridas buenas tras las correcciones son con `n` pequeño (`n=3`) en los mismos escenarios donde falló, no una validación independiente. Siguen pendientes o sin medir: patas reales (M2), objeto con 0 % de eco IR, velocidad en Manual (M8), bloques de luz cambiante, ruta en L y cuadrado en piso, y `RUMBO:ON` en modo Manual.

**4:30 · Cierre**

Por eso etiquetamos cada resultado: validado en el carro, simulado, no medido o pendiente. Lo validado es poco y lo decimos. Lo demás está escrito, con su protocolo de prueba física listo. Gracias; con gusto respondemos preguntas.

## 2. Preguntas probables (agrupadas por quién las responde)

Respuestas cortas y honestas, agrupadas según el reparto de exposición. Cifras de pruebas en PC: las de la corrida del 24-nov-2026 al armar la v1.2 (en el núcleo coinciden con [`estado_20261116.md`](estado_20261116.md)); [`exposicion.md`](exposicion.md) conserva cifras anteriores.

### Jaime Fernando Guerrero Segoviano — Hardware, firmware y conexión

**P:** Si las ruedas son mecanum, ¿por qué el carro no se mueve de lado?  
**R:** Según el equipo, se usaron ruedas mecanum por agarre en terrenos con fricción donde otras ruedas costaban. El driver TC1508A (entradas INT1 a INT4) tiene solo dos canales: las ruedas de cada lado reciben la misma señal, así que el carro es una tracción diferencial y no usa movimiento lateral. (Manual: A.2)

**P:** ¿Por qué hay fuentes de energía separadas para los motores y la electrónica?  
**R:** Los motores TT dan picos de corriente y ruido, y la ESP32 se reinicia si su tensión baja demasiado; por eso las 4 pilas AA Energizer alimentan solo el driver y comparten la tierra, mientras que la cámara **comparte la *powerbank* del DevKit**. Corrientes y tensiones bajo carga: **NO MEDIDO**. (Manual: A.3)

**P:** ¿Por qué un giroscopio y no encoders en las ruedas?  
**R:** Los engranes de plástico de los motores TT tienen holgura y las llantas patinan: un encoder mide el eje del motor, no el patinaje; el giroscopio MPU6050 mide cuánto gira el chasis. A cambio, la distancia sigue siendo por tiempo, y los ángulos de las pruebas los midió el propio giroscopio, sin cinta (**NO MEDIDO**). (Manual: A.3)

**P:** ¿Cómo se hablan el teléfono y el carro, y qué órdenes entiende?  
**R:** El teléfono abre un WebSocket (`ws://192.168.4.1/ws`) y manda texto plano: `FORWARD`, `REVERSE`, `LEFT`, `RIGHT`, `STOP`, `GIRAR:<grados>`, `RUMBO:ON`. Mientras un botón está apretado repite la orden cada 250 ms; el carro responde con `SENS` (ángulo y distancia) cada 100 ms. Es texto y no JSON para no gastar ciclos ni memoria de la ESP32. (Manual: B.2)

**P:** ¿Por qué una red propia (`CarroFSI`) y no el WiFi de la escuela?  
**R:** Así el teléfono, la cámara y el carro se hablan sin router externo ni internet (no hubo otra razón registrada además de no depender de un router externo); procesar video en la nube o en una PC se descartó por latencias de más de 300 ms. La red `CarroFSI` opera en canal 1 fijo, con IP `.1` (carro) y `.2` (cámara) y DHCP `.10–.20` para el teléfono, así la cámara no busca canal tras una caída. (Manual: B.3, C.2)

**P:** ¿Cómo va recto el carro y cómo gira un ángulo exacto?  
**R:** En recta usa `RUMBO`, un control proporcional-integral sobre el giroscopio, limitado a ±15 % de diferencia entre lados. Los giros usan `GIRAR`, que frena cuando el ángulo medido más la inercia aprendida llega al objetivo. En v1.1 (10-nov) 21 de 23 giros quedaron a ±3° según el propio giroscopio (**VALIDADO EN EL CARRO**); cuánto mejora `RUMBO`, **NO MEDIDO**. (Manual: B.4)

**P:** ¿`STOP` tiene prioridad sobre las demás órdenes?  
**R:** No se salta la fila: todo entra a una cola de 32 lugares que se vacía en orden. Su prioridad es que, con la cola llena, no se descarta; se tira primero un `PING` viejo. En PC, 100,000 comandos con dos hilos no perdieron ningún `STOP` (**SIMULADO**). (Manual: B.2)

**P:** ¿Qué firmware tiene el carro hoy y qué cambia en el v3?  
**R:** Ya tiene grabado el firmware v3 `FW:b396390` (`SEG=3`, `TOFP=150`, `TOF_BLOQUEO=180`), que sube el paro a 150 mm, bloquea el arranque bajo 180 mm y tolera un «sin eco» suelto. En M5 (`n=10`) frenó 10/10 sin contacto (margen del chasis 8.0–10.0 cm). v1.1 (120 mm) **no** es respaldo seguro porque deja solo 1.5–3.5 cm de margen. (Manual: B.3)

### Vitto Eduardo Rangel Fortuna — App Android, el reto y demostración

**P:** ¿Cuál es el reto del proyecto y qué modos tiene la app?  
**R:** El reto es guiar un carro de bajo costo —sin LiDAR industrial y sin *encoders*— para que siga una ruta y rodee obstáculos en interiores. La app ofrece tres modos: **manual** (cruceta directa mientras se mantiene pulsado el botón), **semiautónomo** (asistencia y paro/vigilancia mientras se opera) y **autónomo** (ruta dibujada que el navegador en C sigue y corrige solo). (Manual: 1.1, D.2)

**P:** ¿Qué decide la app y qué decide el carro?  
**R:** La app no decide cuándo girar ni cuánto avanzar: eso lo hace el navegador en C, al que la app le da cada 50 ms el último cuadro de la cámara y los datos del carro. La app manda el comando que él devuelve y vigila (`SENS`, video, `WD`, `TOF_PARO`). El carro ejecuta y tiene su propio watchdog y su freno por láser. (Manual: D.2)

**P:** ¿Qué cambió en la interfaz de la v1.2 y cómo saben que funcionó?  
**R:** Solo cuándo y cuántas veces se redibuja la pantalla: las sesiones avisaban hasta 20 veces por segundo y cada aviso la rehacía. En un teléfono (Huawei ELS-NX9, Android 10) los cuadros con una pasada lenta de «medir y acomodar» bajaron de 19.2 % a 3.3 % o menos, con carro y cámara falsos (**SIMULADO**). Con el carro real, **PENDIENTE**. (Manual: D.4)

**P:** ¿Qué guarda la caja negra y se publica?  
**R:** Por cada ruta, un archivo de texto en el teléfono con la configuración, el estado del navegador en cada tick, las decisiones, lo que mandó el carro y números de la visión: nunca imágenes. Se queda en el teléfono; `cajas_negras/` está en `.gitignore`, así que no se publica. (Manual: D.2)

### Melanie Contreras Solis — Cámara, láser y cómo decide el navegador

**P:** ¿Por qué la cámara es otra placa y no va junto a los motores?  
**R:** El sensor GC0308 no comprime JPEG: entrega imagen cruda RGB565 y la ESP32 la comprime por software (`frame2jpg`, HQVGA 240×176), unos 56 ms por cuadro. En la placa de los motores eso podría causar pausas de más de 100 ms en `loop()`; es el razonamiento de `exposicion.md`, no una medición. Una cámara con JPEG por hardware lo evitaría, pero no la probamos. (Manual: C.3)

**P:** ¿Cuánto retraso tiene el video y qué pasa si deja de mandar imágenes?  
**R:** Medimos 320 ms con cronómetro, una sola vez; la meta era 300 ms y no se cumple (por eso guarda solo el último cuadro y tira el anterior, para no acumular retraso). El navegador manda STOP si pasan más de 700 ms sin cuadro y queda en ERROR sin reanudar solo; la app cancela la ruta a los 1,000 ms. El 15-nov, con v1.1, una recta perdió el video 720 ms y terminó en ERROR (causa 5) con el carro detenido (**VALIDADO EN EL CARRO**; app sin versión anotada). El reinicio de la placa de la cámara (`recuperacion.c`) solo está probado en PC (**SIMULADO**). (Manual: C.2, C.5)

**P:** ¿Por qué láser y cámara, y cómo decide el carro qué es obstáculo?  
**R:** Se complementan. La cámara ve ancho y lejos, pero llega unos 320 ms tarde y la luz la engaña; el láser mide milímetros desde el propio carro, pero es un solo haz de unos 25°. El detector de color aprende el piso libre (1 s, quieto) y marca celdas 10×10 distintas; `EfficientDet-Lite0` (de los ejemplos oficiales de TensorFlow Lite, confianza ≥ 0.6; entrenamiento y licencia Apache 2.0 en fuente primaria **no verificados**) dibuja cajas. Con las correcciones del 26-nov (`solo_laser`), cuando el ToF está fresco **el láser manda en el centro** y solo él confirma obstáculo frontal; la cámara sigue pudiendo iniciar un vistazo lateral. (Manual: F.2, F.3, G.3)

**P:** ¿Cómo calculan la distancia con una sola cámara?  
**R:** No la calculan en centímetros: usan qué tan abajo llega el obstáculo en la imagen y lo reparten en tres bandas de cercanía. El «corredor» sí convierte filas en centímetros, pero está apagado: la altura de la cámara se midió en **9 cm** (simulador 11.5 cm, corredor 10 cm), mientras inclinación y campo de visión siguen **NO MEDIDO**. La distancia medida la da el láser. (Manual: F.2)

**P:** ¿Cómo sabe el carro dónde está si no tiene encoders, y por qué C y JNI?  
**R:** Estima la posición multiplicando el tiempo de motor por la calibración y usa el giroscopio en los giros (en B6 v1.1 el error lateral fue de 0 a 10 cm, a ojo; la deriva general es **NO MEDIDO**). Se eligió C porque se vio C en clase (FSI); además el mismo código C99 compila en PC para pruebas y en el teléfono por JNI (`puente.c`, 145 funciones). No medimos si es más rápido que Kotlin (**NO MEDIDO**). (Manual: E.2, E.3)

**P:** ¿Qué estados tiene el navegador y qué pasa si tocan «Continuar» tras una AYUDA sin mover el carro?  
**R:** Tiene ocho estados: `ESPERANDO`, `SIGUIENDO`, `DETENIDO_OBSTACULO`, `ESQUIVANDO`, `REINCORPORANDO`, `TERMINADO`, `ERROR` y `AYUDA` (el ciclo: ve, compara, decide, pide ayuda). En `ERROR` solo sale con ruta nueva; en `AYUDA` cede la cruceta y pasa a `ERROR` a los 3 minutos si nadie contesta. Si tocan «Continuar» sin mover el carro, el navegador no comprueba que la causa ya no esté: tras `AYUDA` por giro (causas 16–19), v3 ignora cuadros del centro hasta el siguiente giro y solo queda el freno láser. Regla: mover el carro con la cruceta antes de «Continuar». (Manual: E.2, E.5)

### Azul Abril Cadena Campos — Seguridad y resultados de las pruebas

**P:** ¿Qué detiene el carro y en qué orden actúan las capas?  
**R:** Del más rápido al más lento: el láser del carro (decenas de ms, sin el teléfono); PARO o soltar el botón (STOP por WiFi, unos 15 ms); la app, que cancela la ruta sin video, sin `SENS` o con `WD` (0.7 a 1.5 s); el watchdog de comandos (1 s); el de hardware, que reinicia el chip a los 5 s. Último recurso: el interruptor del portapilas. En el carro se observó el watchdog con la app caída (15-oct, sin cronómetro); el paro láser aislado y el de hardware, **NO MEDIDO**. (Manual: G.2)

**P:** ¿Cómo frena el carro ante un obstáculo y hay un interruptor para apagar el modo autónomo?  
**R:** El láser mide cada 33 ms; con 2 lecturas seguidas bajo 150 mm en v3 (`FW:b396390`, `SEG=3`), el ESP32 corta los motores sin esperar al teléfono y bloquea el arranque bajo 180 mm (`TOF_BLOQUEO=180`). Solo actúa en perfil AUTO y avanzando; el modelo da 33–103 ms hasta confirmar más inercia. En M5 (`n=10`) frenó 10/10 sin contacto (margen chasis 8.0–10.0 cm). No hay un interruptor único de autónomo: una ruta se corta con PARO y se pasa a Manual; «Usar sensor de distancia (láser)» solo apaga el freno láser. (Manual: G.2, B.3)

**P:** ¿Qué pasa si se pierde el WiFi con el carro en marcha?  
**R:** Sin órdenes por 1,000 ms, el watchdog del carro apaga los motores y vuelve al perfil AUTO. El modelo da un recorrido máximo de unos 21 cm en AUTO (**SIMULADO**; con 19.3 cm/s, y si va a unos 30 cm/s sería más). Se observó con la app cerrada, pero el corte de WiFi en marcha es **NO MEDIDO** (prueba B9). (Manual: G.2)

**P:** ¿Qué tan confiable es el simulador y qué cubren las pruebas automáticas en PC?  
**R:** Sirve para comparar versiones en las mismas condiciones y hallar errores de lógica, no para prometer cómo se porta el carro. Las semillas del Monte Carlo son `160000 + índice × 10007 + corrida` (precisión) y `170000 + índice × 10007 + corrida` (banco autónomo). En v1.2.5 (27-nov), `make -C nucleo_c test` tiene **1 fallo conocido** en `pruebas_sim_ruta` (`61 casos, 1 fallos`: «las quince cajas obligan a esquivar»), mientras los otros 23 programas —incluidos `pruebas_pisos_dificiles` (42), `pruebas_esquive_confirmado` (277), J15 (21) y J16 (46)—, `make -C pruebas_firmware test` (567 casos), `probar_version.py` (21) y `verificar_enlaces.py` pasan con 0 fallos. (Manual: H.3, H.4)

**P:** ¿Qué sí está validado físicamente en el carro y con qué límites?  
**R:** El 25–26 nov con `FW:b396390` y las correcciones del 26-nov: M8 AUTO (`n=5`, 28.7 cm/s), M5 frenado (`n=10`, 10/10 sin contacto), M6 bloqueo (`n=3`/distancia), M1 deriva (0.0° en 10 min), M3 pared lateral (`n=20`/ángulo), piso limpio (`n=5`, 5/5), mochila negra (`n=3` tras corrección) y bajos de 6 cm (`n=2` tras corrección). El 27-nov con `APP:0.4-1792ab0` añadimos el rechazo `< 600 mm` sin movimiento (`2/2` a 433 mm) y `esquive a 46–50 cm/s: 3 corridas, 1 con toque` (incluyendo 1 arranque con sombras aceptado por láser a 90 cm). Ojo con el límite: las corridas buenas con obstáculo son con `n` pequeño en los mismos escenarios, y el frenado aislado M5 se midió a ~28 cm/s con pilas gastadas (**NO MEDIDO** a 46–50 cm/s). (Manual: H.4, I.4)

**P:** ¿Qué es lo que aún no se probó en el carro?  
**R:** Cinco puntos clave siguen **SIMULADO**, **NO MEDIDO** o **PENDIENTE**: (1) **frenado de emergencia aislado M5 a 46–50 cm/s con pilas nuevas** (**NO MEDIDO**; a esa velocidad puede haber un toque suave); (2) **objetos al costado fuera del haz del láser** (`~25°`), que es el **1 fallo conocido** en `pruebas_sim_ruta` y ahora tampoco bloquea el arranque en `aprenderPiso` (**PENDIENTE**); (3) **patas de mueble** en el carro real (M2, omitido; en banco v3 tuvo 43 contactos en banquito y 21 en silla en 1,000 corridas); (4) **modo manual con corrección de rumbo (`RUMBO:ON`)**, **PENDIENTE** (igual que M8 Manual); y (5) **objetos que no devuelven eco IR** (la mochila dio 100/100 eco sólido; el 0 % de eco solo está **SIMULADO**, con 40/40 contactos en banco). También faltan M4, M7, M9 y bloques C y D. (Manual: H.4, H.5)

**P:** ¿Qué pasa con pilas nuevas?  
**R:** Con pilas AA alcalinas nuevas y la calibración nueva el carro recorre `1.5 m` en unos `3 s` (`46` a `50 cm/s`, medido con cronómetro el 27-nov sin pasarse). El frenado validado (`M5`, `10` de `10` sin contacto, margen de `4.5` a `6.5 cm` desde la punta del láser) se midió a unos `28 cm/s` con pilas ya gastadas, así que **el frenado aislado a `46–50 cm/s` es `NO MEDIDO`**. Como la distancia de frenado crece más que proporcionalmente con la velocidad, ese margen puede no alcanzar y existe la posibilidad de un toque suave; en la prueba corta del 27-nov obtuvimos `esquive a 46–50 cm/s: 3 corridas, 1 con toque` (2 limpias sin contacto y 1 con choques). Por eso en la demostración usamos siempre un obstáculo blando (caja de cartón o cojín), nunca algo frágil, y si va a golpear pasamos a manual desde la app o apagamos el carro (Plan B). (Manual: 1.5, G.5, 12.8)

**P:** ¿Qué pasa si hay sombras en el piso?  
**R:** Antes, `aprenderPiso` rechazaba el inicio si más del `5 %` del piso se veía distinto, así que la sombra del carro o de una persona bloqueaba el arranque aunque no hubiera nada delante. Ahora (`v1.2.5`, umbral único `OBS_APRENDER_TOF_LIBRE_MM = 600` mm en `nucleo_c/obstaculos.h`) consulta al láser `VL53L0X` como segunda opinión: (1) si el láser fresco (`<= 400 ms`) y válido ve libre `>= 60 cm`, permite iniciar avisando `"Piso con sombras; el láser ve libre a X cm; continuando con el láser como autoridad"` y aprende el piso solo con los grupos compatibles e *inliers* sin contaminarlo con sombras (y en marcha la visión sola con ToF fresco no dispara esquives); (2) si el láser mide `< 60 cm`, no arranca y avisa `"Tienes un objeto a X cm, muévelo"`; (3) si no hay lectura fresca o válida (sin eco, vencida o error), mantiene el rechazo avisando que no se pudo confirmar el frente con el láser. Esto está **SIMULADO** en software (42 casos en `pruebas_pisos_dificiles` y 277 en `pruebas_esquive_confirmado`) y **VALIDADO EN EL CARRO** en el rechazo `< 600 mm` sin movimiento (`2/2` a `433 mm`) y en `1` arranque con sombras (`9 %` descartado, láser libre a `90 cm`, terminado sin toques) dentro de las 3 corridas del 27-nov; la contrapartida es que un objeto al costado fuera del haz del láser (~25°) ya no bloquea el arranque y sigue **PENDIENTE** de probar en el carro. (Manual: E.3, F.2, F.4, 12.8)

### Todos — Preguntas generales

**P:** ¿Cómo lo hicieron, qué versiones hubo y por qué v3 y no v1.1 ni v4–v6?  
**R:** Empezó como «WALL-E», un sketch de Arduino de un solo archivo, y pasó a núcleo en C99 probado en PC: v1.0 (27-oct), v1.1 (10-nov), v2 y v3 (simuladas), v4–v6 (descartadas en banco con ruido realista) y v1.2/v1.2.5 (v3 + revisión de interfaz + correcciones del 26-nov + sombras por láser en `aprenderPiso`). Frente a v1.1 (paro a 120 mm, margen 1.5–3.5 cm), v3 con `FW:b396390` (`SEG=3`, 150/180 mm) es la opción más segura. (Manual: I.2, I.3)

**P:** ¿Qué explica cada integrante en la exposición?  
**R:** Portada, cierre y preguntas: todos. Cada quien profundiza en su área para exponer (sección 3) y todos deben entender todo el sistema. (Manual: 1.5)

**P:** ¿Usaron herramientas de inteligencia artificial?  
**R:** «Nos asistimos de herramientas de inteligencia artificial para la programación. Entendemos el funcionamiento del sistema y la guiamos para construir lo que queríamos.» El curso no exige declararlo, pero lo permite, y por eso el equipo lo declara (sección 6). (Manual: I.2)

## 3. Quién explica qué en la exposición

Principio de la guía: **todos deben entender todo el sistema; cada quien profundiza en su área para exponer** (portada, cierre y preguntas: todos). Quien expone un tema no es necesariamente quien lo construyó; todos deben poder explicar todo el sistema.

### Orden de las 10 diapositivas

| # | Diapositiva | Responsable |
|---|---|---|
| 1 | Portada | Todos |
| 2 | El reto | Vitto Eduardo Rangel Fortuna |
| 3 | Lo que lleva el carro | Jaime Fernando Guerrero Segoviano |
| 4 | Cómo se conecta | Jaime Fernando Guerrero Segoviano |
| 5 | Cómo ve | Melanie Contreras Solis |
| 6 | Cómo decide | Melanie Contreras Solis |
| 7 | Seguridad | Azul Abril Cadena Campos |
| 8 | Lo que sí medimos | Azul Abril Cadena Campos |
| 9 | Demostración en vivo | Vitto Eduardo Rangel Fortuna (explica) y Jaime Fernando Guerrero Segoviano (corre el carro) |
| 10 | Cierre y preguntas | Todos |

### Área para exponer por integrante (y subsistemas del manual)

| Integrante | Área para exponer | Subsistemas del manual que cubre |
|---|---|---|
| Jaime Fernando Guerrero Segoviano | Qué lleva el carro (hardware y firmware) y cómo se conecta (red WiFi `CarroFSI`, WebSocket, comunicación carro-teléfono-cámara); corre el carro en la demostración | [A · Hardware](manual/capitulos/01d_subsistema_A_hardware.md), [B · Firmware](manual/capitulos/01e_subsistema_B_firmware.md) |
| Vitto Eduardo Rangel Fortuna | El reto (planteamiento del problema) y la demostración en vivo (app Android y los tres modos: manual, semiautónomo, autónomo) | [D · App Android](manual/capitulos/01g_subsistema_D_app_android.md) |
| Melanie Contreras Solis | Cómo ve (cámara y láser) y cómo decide (el ciclo del navegador: ve, compara, decide, pide ayuda) | [C · Cámara](manual/capitulos/01f_subsistema_C_camara.md), [E · Núcleo en C](manual/capitulos/01h_subsistema_E_nucleo_c.md), [F · Visión](manual/capitulos/01i_subsistema_F_vision.md) |
| Azul Abril Cadena Campos | Seguridad (capas de protección) y lo que sí medimos (resultados de las pruebas físicas, con sus límites) | [G · Seguridad](manual/capitulos/01j_subsistema_G_seguridad.md), [H · Simulador y pruebas](manual/capitulos/01k_subsistema_H_simulador_pruebas.md) |
| Todos | Portada, cierre, preguntas y visión general del proyecto | [I · Proceso](manual/capitulos/01l_subsistema_I_proceso.md) y capítulo 1 |

*Nota sobre autoría y operación:* **Jaime Fernando Guerrero Segoviano** armó e integró el sistema base del carro y su lógica, operó el carro en las pruebas físicas y lo corre en la demostración mientras **Vitto Eduardo Rangel Fortuna** explica.

### Lo que todos deben saber (10 puntos)

1. Tres nodos en red local propia (`CarroFSI`, 2.4 GHz): carro (`ESP32 DevKit`), cámara (`ESP32-WROVER`) y teléfono Android.
2. La cámara comparte la *powerbank* del DevKit; los motores van aparte con 4 pilas AA Energizer vía `TC1508A` (`INT1`–`INT4`).
3. Sin *encoders*: distancia por tiempo (`ms_por_cm`) y giros/rumbo con el giroscopio `MPU6050`.
4. Láser `VL53L0X` (3.5 cm del suelo, sobresale 3.5 cm): frena en el carro a 150 mm (`FW:b396390`, `SEG=3`) y bloquea arranque bajo 180 mm.
5. Núcleo en `C99`: mismo código en PC (pruebas/simulador) y teléfono (JNI), respondiendo cada 50 ms.
6. Visión: color del piso + `EfficientDet-Lite0`; con las correcciones del 26-nov, cuando el ToF está fresco manda el láser real en el centro.
7. Si no puede esquivar con seguridad, pasa a `AYUDA`, detiene el carro y cede la cruceta al operador.
8. Watchdog de comandos apaga motores a los 1,000 ms sin latido (250 ms); watchdog de hardware reinicia a los 5,000 ms.
9. Exposición: **Opción 2** (demostración con pilas AA nuevas y calibración nueva a 46–50 cm/s, incluyendo obstáculos blandos; v3 + `FW:b396390` + correcciones del 26-nov + sombras por láser; Plan B: mismo firmware en manual/semiautónomo; frenado aislado **NO MEDIDO** a esa velocidad); v1.1 no es respaldo seguro.
10. Etiquetas honestas en todo: las corridas buenas del 26-nov son `n=3` en los mismos escenarios donde falló, y `RUMBO:ON` en Manual sigue pendiente.

### Área para exponer (3 preguntas más probables por integrante)

- **Jaime Fernando Guerrero Segoviano (Hardware, firmware y conexión):** (1) ¿Cómo se conectan los tres nodos en la red propia `CarroFSI` (canal 1, `.1` y `.2`, DHCP `.10–.20`, WebSocket `ws://192.168.4.1/ws`)? (2) ¿Por qué v3 con `SEG=3` (150/180 mm) y no v1.1 (120 mm) ni v4–v6? (3) ¿Por qué fuentes separadas (*powerbank* compartida para DevKit y cámara; 4 pilas AA para motores) y giroscopio en lugar de *encoders*?
- **Vitto Eduardo Rangel Fortuna (App Android, el reto y demostración):** (1) ¿Cuál es el reto del proyecto y qué hacen los tres modos (manual, semiautónomo, autónomo)? (2) ¿Qué vigila la app (`SENS`, video, `WD`, `TOF_PARO`) frente al núcleo en C por JNI, y qué hace `ManualFragment` con `RUMBO`? (3) ¿Qué cambió en la revisión de interfaz y qué guarda la caja negra?
- **Melanie Contreras Solis (Cámara, láser y cómo decide el navegador):** (1) ¿Por qué una placa aparte para la cámara (RGB565 HQVGA con `frame2jpg`, ~320 ms de retraso) y cómo se combinan color, red neuronal y láser (`solo_laser` y `PARALELO` del 26-nov)? (2) ¿Cómo estima el navegador su posición sin *encoders* y cuáles son sus ocho estados? (3) ¿Qué hace el ciclo del navegador (ve, compara, decide, pide ayuda) y qué pasa si se pulsa «Continuar» tras `AYUDA` sin mover el carro?
- **Azul Abril Cadena Campos (Seguridad y resultados de las pruebas):** (1) ¿En qué orden actúan las capas de paro desde el láser (`SEG=3`, 150/180 mm) hasta el portapilas, y qué pasa si cae el WiFi? (2) ¿Qué se midió el 25–26 nov (M1, M3, M5 10/10, M6, M8 AUTO, mochila 3/3 y bajos 6 cm 2/2 con `n` pequeño)? (3) ¿Qué aún no se probó en el carro (objetos al costado fuera del haz del láser, patas de mueble, modo manual con corrección de rumbo `RUMBO:ON`, objetos que no devuelven eco)?

## 4. Qué decir si algo falla en la demostración

Durante la demostración en vivo, **Vitto explica** los tres modos y lo que hace la app mientras **Jaime corre el carro**.  
**Nota de la demostración (Opción 2):** pilas AA alcalinas nuevas y un juego de repuesto; piso con al menos **60 cm libres al frente según el láser** (`OBS_APRENDER_TOF_LIBRE_MM = 600` mm); obstáculo blando (caja de cartón o cojín), nunca algo frágil; confirmar la recta de `1.5 m` en unos `3 s` (`46–50 cm/s`) antes de empezar. **Frenado aislado a 46–50 cm/s: NO MEDIDO** (el frenado `10/10` con margen de `4.5–6.5 cm` se midió a `~28 cm/s` con pilas gastadas; a `46–50 cm/s` la distancia de frenado crece más que proporcionalmente y puede haber un toque suave; el 27-nov se midió `esquive a 46–50 cm/s: 3 corridas, 1 con toque`). Si el carro va a golpear, pasar a manual desde la app o apagarlo (Plan B).

Regla general: mano cerca del carro y el pulgar sobre PARO, que actúa al tocarlo. Si algo no se entiende, primero PARO; después se explica.

### El carro se detiene solo

- **Qué se ve:** el carro queda quieto sin que nadie toque PARO. La pantalla dice «Obstáculo: detenido», «La ruta se detuvo» o un aviso como «El láser frenó el carro…» o «El carro se frenó solo: no le llegaron comandos en 1 s…».
- **Manos:** no lo agarren. Si dice «Obstáculo: esquivando», dejen que termine. Si la ruta se detuvo: quiten el obstáculo, reposicionen el carro y toquen «Iniciar ruta» otra vez, como pide la app.
- **Decir:** «Esto es la seguridad funcionando, no una falla. El láser va en el carro y corta los motores sin esperar al teléfono: dos lecturas seguidas bajo 120 mm en el firmware v1.1, 150 mm en el v3. Y si pasa un segundo sin órdenes, el carro se apaga solo. La distancia real de frenado todavía no la medimos.»

### El carro entra en AYUDA

- **Qué se ve:** el carro queda quieto, el teléfono vibra y dice «Necesito ayuda: toma el control», con «Causa N: …», una cruceta y «Continuar la ruta».
- **Manos:** mantengan apretado un botón de la cruceta para rodear el obstáculo (soltar es STOP) y dejen el carro cerca de la línea. **Muévanlo antes de tocar «Continuar la ruta»**: sin moverlo, el navegador puede pedir ayuda otra vez. Tienen 3 minutos; después pasa a ERROR. PARO cancela.
- **Decir:** «Cuando no puede esquivar con seguridad, el carro no insiste: se detiene y nos cede el mando. Está diseñado así. El 10 de noviembre, una de seis corridas entró en AYUDA y no identificamos la causa.»

### Se cae el WiFi

- **Qué se ve:** la cabecera deja de decir «Conectado» y aparece «Se perdió la conexión con el carro» o «El carro no responde». Con una ruta en marcha, la app la cancela.
- **Manos:** PARO no cruza un WiFi caído; quien frena es el watchdog del carro (1 s). Si el carro no frena, apaguen el interruptor del portapilas 4×AA. Después: teléfono en la red «CarroFSI», «Conectar», reposicionar el carro y volver a iniciar la ruta.
- **Decir:** «El carro no depende del teléfono para frenar: si pasa un segundo sin órdenes, apaga los motores. Lo vimos una vez con la app cerrada, el 15 de octubre. El corte de WiFi en marcha todavía no lo probamos; el modelo da unos 21 centímetros de recorrido.»

### No hay video de la cámara

- **Qué se ve:** en Manual, sobre el video: «La cámara no responde. Se reintenta sola cada 3 s. Puedes seguir manejando sin video.» En una ruta, «Cámara sin imágenes» y el carro se detiene; «Iniciar ruta» no sale sin video.
- **Manos:** en Manual no hace falta nada: se maneja igual. Si la cámara sigue sin responder, apáguenla y enciéndanla. Una ruta detenida no se reanuda sola: reposicionen y vuelvan a iniciar.
- **Decir:** «El video lo manda otra placa; manejar no depende de él. Si falta en una ruta, el carro se detiene por seguridad. La cámara ya se colgó antes, el 27 de octubre, y volvió al apagarla y encenderla.»

### La app se cierra o se congela

- **Qué se ve:** la app desaparece o deja de responder, y el carro se detiene.
- **Manos:** mano cerca del carro; si no frena, interruptor del portapilas. Reabran la app, toquen «Conectar» en Inicio y esperen «Conectado». Si el teléfono pasa a segundo plano o se gira, la app detiene el carro a propósito.
- **Decir:** «Si Android cierra la app, el carro frena de inmediato; si solo se congela, deja de llegar el latido y el watchdog apaga los motores en un segundo. Lo observamos con la app cerrada a media maniobra, sin cronómetro.»

### Apagar el autónomo y seguir en manual

No existe un interruptor único de modo. Lo autónomo es una ruta en marcha.

- **Manos:** 1) toquen **PARO** (rojo; está en la cabecera mientras la ruta mueve el carro, y en Manual): manda STOP y cancela la ruta. 2) Toquen la pestaña **Manual**; si la ruta seguía, la app pregunta «Detener y cambiar». 3) En Manual, el carro se mueve solo mientras mantienen un botón apretado. 4) La velocidad sale de Más → Ajustes → «Velocidad en Manual».
- **Ojo:** por defecto Manual va en «Rápido» (perfil MANUAL, 255) y ahí el láser no frena; con «Lento» (perfil AUTO, 210) sí. «Usar sensor de distancia (láser)» (Más → Calibración → «Abrir sensores») apaga el freno láser, no el modo autónomo. Último recurso: interruptor del portapilas.
- **Decir:** «No hay un solo interruptor de modo. El autónomo es una ruta en marcha: se corta con PARO y pasamos a Manual, donde el carro solo se mueve mientras mantenemos el botón.»

### Mientras se reinicia (30 segundos cada dato)

1. **Empezó como «WALL-E»:** un sketch de Arduino de un solo archivo. Separamos la lógica en C para probarla en PC sin el carro.
2. **Giroscopio, no encoders:** los motores TT tienen holgura y las llantas patinan. El 10-nov, 21 de 23 giros quedaron a ±3° según el propio giroscopio.
3. **Un solo código, dos plataformas:** el núcleo en C se compila con GCC en PC y con el NDK en el teléfono; en v1.2.2 pasan `pruebas_esquive_confirmado` (275), J15 (21) y J16 (46), con 1 fallo conocido en `pruebas_sim_ruta` (cajas laterales).
4. **La cámara tira cuadros viejos:** guarda solo el último y descarta el anterior, para que el retraso no se acumule; manda unos 12 cuadros por segundo.
5. **Arranque a plena potencia:** con menos de 255 desde parado los motores solo zumbaban; por eso cada avance arranca 250 ms a tope.

## 5. Qué es medido y qué es simulado

Cada cifra de esta guía lleva una etiqueta. Este es el resumen para no sobrevender.

**VALIDADO EN EL CARRO** (25–26 nov-2026 con `FW:b396390`, `cam-v2` y app `0.4-b396390` + correcciones del 26-nov; más histórico v1.1 del 10-nov)

- **Línea base recta 2 m (`IMU+RUMBO`):** `n=3`, `3/3` OK, 186 cm, 28.5 cm/s, deriva 0/11/0 cm, 0 falsos positivos (2 corridas previas sin IMU por cable SCL roto derivaron +43 cm).
- **M8 AUTO:** `n=5`, `5/5`, 140 cm en 4.82–4.91 s, media 28.7 cm/s, deriva 0 cm en `5/5`.
- **M5 frenado (`TOFP=150`):** `n=10`, `10/10` sin contacto, `TOF_PARO` media 144.7 mm, margen del chasis 8.0–10.0 cm (media 9.1), punta del láser 4.5–6.5 cm (media 5.6).
- **M6 bloqueo (`TOF_BLOQUEO=180`):** `n=3` por distancia; a 20 y 18 cm la primera avanzó 355–415 ms sin tocar y frenó a 9–10 cm, las otras bloqueo instantáneo (6–19 ms); a 16 cm `3/3` bloqueo instantáneo. Sesgo positivo del VL53L0X: +40 a +51 mm (+67 a +102 mm con tela negra mate).
- **M1 deriva del giroscopio:** `n=1` de 10 min quieto, 0.0° en 0, 1, 2, 5 y 10 min; tras 32.3 s en AUTO, 0.0°. Muestreo 10.0 Hz.
- **M3 pared lateral a 30 cm (`n=20`/ángulo):** 0° 100 % libre; ~20° 85 % libre; ~31°, ~43° y ~61° 0 % libre (detectada).
- **Mochila negra mate (42×14 cm):** estático 20–80 cm (`n=100`), `100/100` con eco sólido. Dinámico con `main` original (`n=3`): r1 terminó sin tocar, r2 AYUDA causa 14, r3 fallo grave con contacto. Dinámico **con las correcciones del 26-nov** (mismo escenario, `n` pequeño `n=3`): `3/3` terminado, 0 contactos, 0 AYUDA, reincorporación a 1–2 cm de la línea.
- **Objetos bajos (2 y 6 cm):** láser los vio en `20/20` a 40 cm (`aprenderPiso` rechaza a 40 cm: 18 % > 5 %). Dinámicos a 90 cm: 2 cm `n=3`, `3/3` terminado, 0 contactos; 6 cm `n=3`: r1 AYUDA causa 2 sin contacto (antes de la corrección), r2 y r3 terminados con 0 contactos (con la corrección del 26-nov).
- **Piso limpio 1.5 m:** `n=5`, `5/5` terminado, `0/5` falsos positivos, 0 paradas falsas, 0 esquives; recorrió 133–139 cm de 150 por desgaste de pilas (recalibración `ms_por_cm_x10=355` aplicada después, sin repetirlas).
- **Pruebas cortas del 27-nov-2026 (`APP:0.4-1792ab0`, `FW:b396390`, pilas AA nuevas `46–50 cm/s`, arreglo de sombras en `aprenderPiso`):**
  - C1 sin movimiento (objeto a ~40 cm): `2/2` rechazos exactos (`ToF = 433 mm`, mensaje `"Tienes un objeto a 43 cm, muévelo"`).
  - C2 (`Recta 200 cm`, obstáculo blando a ~90 cm): **`esquive a 46–50 cm/s: 3 corridas, 1 con toque`** (corrida 1 con toques/choques, 3 `GIRO_TIMEOUT`, `TOF_PARO` `125 mm`, min ToF `49 mm`, `AYUDA` causa 6, `38.9 s`; corrida 2 `TERMINADO` sin toque en `17.1 s`, min ToF `328 mm`; corrida 3 `TERMINADO` sin toque en `13.7 s`, min ToF `212 mm`, donde `aprenderPiso` descartó `9 %` por sombras y el láser a `90 cm` aceptó el arranque con `"Piso con sombras; el láser ve libre a 90 cm; continuando con el láser como autoridad"` — `1/1` con sombras). No es validación completa.
- **Histórico v1.1 (10-nov-2026):** B6 (`n=6`, 6/6 sin contacto, 5/6 completas). No es respaldo seguro por paro a 120 mm.

**SIMULADO** (PC, banco, teléfono con carro y cámara falsos)

- Banco de 23 escenarios × 1,000 corridas de v3 (16-nov-2026), con 23 compuertas en rojo (**calculado con v3 antes de las correcciones del 26-nov, sin recalcular**); objeto con 0 % de eco IR sigue solo simulado.
- Pruebas en PC tras v1.2.5 (27-nov): **1 fallo conocido** en `pruebas_sim_ruta` (`61 casos, 1 fallos`: cajas laterales fuera del haz del láser), 0 fallos en los otros 23 ejecutables de `nucleo_c` —incluidos `pruebas_pisos_dificiles` (42), `pruebas_esquive_confirmado` (277), J15 (21) y J16 (46)—, `pruebas_firmware` (567) y `probar_version.py` (21).
- Revisión de interfaz de la app (24-nov-2026): probada con carro y cámara falsos; cuadros lentos de 19.2 % a 3.3 % o menos.
- Monte Carlo de precisión J16 (31-oct-2026), 1,000 corridas por escenario (**calculado antes de las correcciones del 26-nov, sin recalcular**).

**NO MEDIDO / PENDIENTE**

- Frenado aislado M5 a `46–50 cm/s` con pilas nuevas (**NO MEDIDO**), objeto lateral fuera del haz del láser al arrancar (`aprenderPiso`) o en marcha (**PENDIENTE**), C3 (sombras sin obstáculo) y caso sin ToF/sin eco en el carro (**PENDIENTE**), M8 en Manual (límite de seguridad), M2 (patas reales), objeto con 0 % de eco IR, M4, M7, M9, corte de WiFi en marcha (B9), bloques C y D (luz cambiante, ruta en L, cuadrado), prueba física de `RUMBO:ON` en modo Manual (**PENDIENTE**), inclinación/FOV de cámara e inclinación del láser, corrientes y autonomía.

## 6. Uso de herramientas de IA en el desarrollo

Declaración del equipo:

> «Nos asistimos de herramientas de inteligencia artificial para la programación. Entendemos el funcionamiento del sistema y la guiamos para construir lo que queríamos.»

El curso no exige declararlo, pero lo permite, y por eso el equipo lo declara.

Lo que muestra el repositorio:

- En el historial de git, una sola cuenta (`fernandosegrr`) concentra los commits y muchos llevan `Co-Authored-By` de agentes de código usados para escribir y revisar código, documentación y pruebas.
- Los resultados del carro y las pruebas en PC se respaldan con archivos, fechas y salidas reales; explicar cada subsistema en la exposición corresponde a cada integrante (sección 3).
- Operó el carro en las pruebas físicas y lo opera en la demostración: **Jaime Fernando Guerrero Segoviano**.

## 7. Lo que aún no validamos

Para decir en voz alta. El autónomo v3 con las correcciones del 26-nov tiene **validación física PARCIAL** en el carro (25–26 nov: M1, M3, M5, M6, M8 AUTO, piso limpio, mochila negra y bajos de 2 y 6 cm; las corridas buenas con obstáculo son posteriores a las correcciones y con `n` pequeño en los mismos escenarios donde ocurrió la falla). Lo siguiente sigue **SIMULADO**, **NO MEDIDO** o **PENDIENTE**:

- Frenado aislado M5 a `46–50 cm/s` con pilas nuevas (**NO MEDIDO**; en C2 del 27-nov hubo `esquive a 46–50 cm/s: 3 corridas, 1 con toque`).
- Objetos laterales fuera del haz del láser (`pruebas_sim_ruta` tiene 1 fallo conocido; el haz del `VL53L0X` es estrecho, ~25°; desde v1.2.5 tampoco bloquean el arranque en `aprenderPiso`, caso **PENDIENTE** en el carro).
- Muebles con patas en el carro real (M2, omitido; sigue **SIMULADO** en banco).
- Objeto con 0 % de eco IR (la mochila dio `100/100` eco sólido; el caso de 0 % de eco sigue **SIMULADO**).
- Velocidad real en modo Manual (M8 Manual, **NO MEDIDO** por límite de seguridad) y `RUMBO:ON` en modo Manual (**PENDIENTE**).
- Bloques C y D del protocolo: luz cambiante (M4), corte de WiFi en marcha (B9), tapar la cámara 2 s, ruta en L, cuadrado y giros cortos (M7).
- Latencia de extremo a extremo (M9), inclinación del láser e inclinación/FOV de la cámara, escala del giroscopio con vuelta completa (`IMUS=0`), corrientes, tensión de las pilas y autonomía.
- Causa exacta de la corrida 5 de v1.1 (10-nov) y el comportamiento de «Continuar» tras `AYUDA` sin mover el carro.

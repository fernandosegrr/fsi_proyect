# Guía de exposición — Carro FSI v1.2.1 (1 de diciembre de 2026)

- **Versión:** v1.2.1 — autónomo v3 con correcciones del 26-nov (`navegador.c` y `ManualFragment.kt`); firmware en el carro `FW:b396390` (`SEG=3`, `TOFP=150`, `TOF_BLOQUEO=180`); cámara `cam-v2`; app `0.4-b396390` + correcciones del 26-nov; validación física parcial (25–26 nov-2026).
- **Decisión para la exposición (Opción 1 con condiciones):** exponer v3 autónomo con `FW:b396390` y las dos correcciones del 26-nov. **Plan B:** el mismo firmware v3 con el autónomo apagado (modo manual o semiautónomo). **No usar v1.1 como respaldo seguro:** su paro a 120 mm deja solo 1.5 a 3.5 cm de margen y puede golpear el sensor.
- **Condiciones operativas:** pilas AA alcalinas nuevas y un juego de repuesto; arrancar cada ruta con al menos 100 cm de piso libre delante (`aprenderPiso` rechaza a 40–80 cm); encender primero la *powerbank* con el carro quieto 3 s para calibrar el giroscopio y luego conectar las pilas; revisar cables antes de salir.
- **Cómo usarla:** léanla completa una vez. El guion (sección 1) se dice en voz alta, las preguntas (sección 2) van por subsistema y la sección 4 es para cuando algo falle en vivo. Cada respuesta cierra con `(Manual: X.n)`, la sección del manual que la explica.
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

## 2. Preguntas probables

Respuestas cortas y honestas. Cifras de pruebas en PC: las de la corrida del 24-nov-2026 al armar la v1.2 (en el núcleo coinciden con [`estado_20261116.md`](estado_20261116.md)); [`exposicion.md`](exposicion.md) conserva cifras anteriores.

### A · Hardware

**P:** Si las ruedas son mecanum, ¿por qué el carro no se mueve de lado?  
**R:** Según el equipo, se usaron ruedas mecanum por agarre en terrenos con fricción donde otras ruedas costaban. El driver TC1508A (entradas INT1 a INT4) tiene solo dos canales: las ruedas de cada lado reciben la misma señal, así que el carro es una tracción diferencial y no usa movimiento lateral. (Manual: A.2)

**P:** ¿Por qué hay fuentes de energía separadas para los motores y la electrónica?  
**R:** Los motores TT dan picos de corriente y ruido, y la ESP32 se reinicia si su tensión baja demasiado; por eso las 4 pilas AA Energizer alimentan solo el driver y comparten la tierra, mientras que la cámara **comparte la *powerbank* del DevKit**. Corrientes y tensiones bajo carga: **NO MEDIDO**. (Manual: A.3)

**P:** ¿Por qué un giroscopio y no encoders en las ruedas?  
**R:** Los engranes de plástico de los motores TT tienen holgura y las llantas patinan: un encoder mide el eje del motor, no el patinaje; el giroscopio MPU6050 mide cuánto gira el chasis. A cambio, la distancia sigue siendo por tiempo, y los ángulos de las pruebas los midió el propio giroscopio, sin cinta (**NO MEDIDO**). (Manual: A.3)

### B · Firmware

**P:** ¿Cómo se hablan el teléfono y el carro, y qué órdenes entiende?  
**R:** El teléfono abre un WebSocket (`ws://192.168.4.1/ws`) y manda texto plano: `FORWARD`, `REVERSE`, `LEFT`, `RIGHT`, `STOP`, `GIRAR:<grados>`, `RUMBO:ON`. Mientras un botón está apretado repite la orden cada 250 ms; el carro responde con `SENS` (ángulo y distancia) cada 100 ms. Es texto y no JSON para no gastar ciclos ni memoria de la ESP32. (Manual: B.2)

**P:** ¿Cómo va recto el carro y cómo gira un ángulo exacto?  
**R:** En recta usa `RUMBO`, un control proporcional-integral sobre el giroscopio, limitado a ±15 % de diferencia entre lados. Los giros usan `GIRAR`, que frena cuando el ángulo medido más la inercia aprendida llega al objetivo. En v1.1 (10-nov) 21 de 23 giros quedaron a ±3° según el propio giroscopio (**VALIDADO EN EL CARRO**); cuánto mejora `RUMBO`, **NO MEDIDO**. (Manual: B.4)

**P:** ¿`STOP` tiene prioridad sobre las demás órdenes?  
**R:** No se salta la fila: todo entra a una cola de 32 lugares que se vacía en orden. Su prioridad es que, con la cola llena, no se descarta; se tira primero un `PING` viejo. En PC, 100,000 comandos con dos hilos no perdieron ningún `STOP` (**SIMULADO**). (Manual: B.2)

**P:** ¿Qué firmware tiene el carro hoy y qué cambia en el v3?  
**R:** Ya tiene grabado el firmware v3 `FW:b396390` (`SEG=3`, `TOFP=150`, `TOF_BLOQUEO=180`), que sube el paro a 150 mm, bloquea el arranque bajo 180 mm y tolera un «sin eco» suelto. En M5 (`n=10`) frenó 10/10 sin contacto (margen del chasis 8.0–10.0 cm). v1.1 (120 mm) **no** es respaldo seguro porque deja solo 1.5–3.5 cm de margen. (Manual: B.3)

### C · Cámara

**P:** ¿Por qué la cámara es otra placa y no va junto a los motores?  
**R:** El sensor GC0308 no comprime JPEG: entrega imagen cruda y la ESP32 la comprime por software, unos 56 ms por cuadro en 240×176. En la placa de los motores eso podría causar pausas de más de 100 ms en `loop()`; es el razonamiento de `exposicion.md`, no una medición. Una cámara con JPEG por hardware lo evitaría, pero no la probamos. (Manual: C.3)

**P:** ¿Cuánto retraso tiene el video y qué significa para el carro?  
**R:** Medimos 320 ms con cronómetro, una sola vez; la meta era 300 ms y no se cumple. Por eso el freno de emergencia no depende de la cámara. La cuenta con la calibración da unos 8.5 cm hasta quedar detenido en AUTO: es una cuenta, no una medida con cinta, y con el carro en marcha **NO MEDIDO**. (Manual: C.5)

**P:** ¿Qué pasa si la cámara deja de mandar imágenes con el carro avanzando?  
**R:** El navegador manda STOP si pasan más de 700 ms sin cuadro y queda en ERROR sin reanudar solo; la app cancela la ruta a los 1,000 ms. El 15-nov, con v1.1, una recta perdió el video 720 ms y terminó en ERROR (causa 5) con el carro detenido (**VALIDADO EN EL CARRO**; app sin versión anotada). El reinicio de la placa de la cámara solo está probado en PC (**SIMULADO**). (Manual: C.2)

### D · App Android

**P:** ¿Qué decide la app y qué decide el carro?  
**R:** La app no decide cuándo girar ni cuánto avanzar: eso lo hace el navegador en C, al que la app le da cada 50 ms el último cuadro de la cámara y los datos del carro. La app manda el comando que él devuelve y vigila. El carro ejecuta y tiene su propio watchdog y su freno por láser. (Manual: D.2)

**P:** ¿Qué cambió en la interfaz de la v1.2 y cómo saben que funcionó?  
**R:** Solo cuándo y cuántas veces se redibuja la pantalla: las sesiones avisaban hasta 20 veces por segundo y cada aviso la rehacía. En un teléfono (Huawei ELS-NX9, Android 10) los cuadros con una pasada lenta de «medir y acomodar» bajaron de 19.2 % a 3.3 % o menos, con carro y cámara falsos (**SIMULADO**). Con el carro real, **PENDIENTE**. (Manual: D.4)

**P:** ¿Qué guarda la caja negra y se publica?  
**R:** Por cada ruta, un archivo de texto en el teléfono con la configuración, el estado del navegador en cada tick, las decisiones, lo que mandó el carro y números de la visión: nunca imágenes. Se queda en el teléfono; `cajas_negras/` está en `.gitignore`, así que no se publica. (Manual: D.2)

### E · Núcleo en C

**P:** ¿Cómo sabe el carro dónde está si no tiene encoders?  
**R:** Lo estima: multiplica el tiempo de motor por la calibración y usa el giroscopio en los giros. La estimación se desvía y hay que recalibrar si cambian pilas, piso o peso. En B6 (v1.1) el error lateral final fue de unos 0 a 10 cm, medido a ojo; la deriva en general, **NO MEDIDO**. (Manual: E.2)

**P:** ¿Qué estados tiene el navegador y qué diferencia hay entre AYUDA y ERROR?  
**R:** Ocho: ESPERANDO, SIGUIENDO, DETENIDO_OBSTACULO, ESQUIVANDO, REINCORPORANDO, TERMINADO, ERROR y AYUDA. En ERROR el carro se detiene y solo sale con una ruta nueva. En AYUDA se detiene, una persona lo maneja con la cruceta y toca «Continuar la ruta»; a los 3 minutos sin respuesta pasa a ERROR. (Manual: E.2)

**P:** ¿Qué pasa si tocan «Continuar» tras una AYUDA sin mover el carro?  
**R:** El navegador no comprueba que la causa ya no esté: devuelve los presupuestos de esquivadas y calcula la vuelta a la línea. Tras una AYUDA por giro (causas 16 a 19), la v3 ignora los cuadros del centro hasta el siguiente giro y solo queda el freno láser. Regla: mover el carro con la cruceta antes de «Continuar». (Manual: E.5)

### F · Visión

**P:** ¿Cómo sabe el carro qué es piso y qué es obstáculo?  
**R:** No sabe qué es cada cosa. Aprende cómo se ve el piso libre (1 s, con el carro quieto) y marca las celdas de 10×10 píxeles cuyo color ya no se parece. Si una zona (izquierda, centro o derecha) junta 7 celdas hay un posible obstáculo; que lo sea lo decide la fusión con la red y el láser. Un objeto del color del piso no se ve por color. (Manual: F.2)

**P:** ¿Por qué piden color Y red, y quién manda si el láser no está de acuerdo?  
**R:** Cada uno se equivoca distinto: el color con juntas de baldosa y sombras, la red con cajas inventadas; juntos filtran a ambos, aunque con sol o sombra fallan a la vez. Con las correcciones del 26-nov (`solo_laser`), cuando `autoridad_laser` está activo y el ToF está fresco, **el láser manda en el centro**: solo él confirma obstáculo en el centro y las reglas de visión pura no disparan esquive ni bloquean la reincorporación; la cámara sigue pudiendo iniciar un vistazo lateral. (**ADVERTENCIA:** antes de esa corrección se explicaba que el láser libre no borraba la sospecha visual.) (Manual: F.3)

**P:** ¿Cómo calculan la distancia con una sola cámara?  
**R:** No la calculan en centímetros: usan qué tan abajo llega el obstáculo en la imagen y lo reparten en tres bandas de cercanía. El «corredor» sí convierte filas en centímetros, pero está apagado: la altura de la cámara se midió en **9 cm** (simulador 11.5 cm, corredor 10 cm), mientras inclinación y campo de visión siguen **NO MEDIDO**. La distancia medida la da el láser. (Manual: F.2)

### G · Seguridad

**P:** ¿Qué detiene el carro y en qué orden actúan las capas?  
**R:** Del más rápido al más lento: el láser del carro (decenas de ms, sin el teléfono); PARO o soltar el botón (STOP por WiFi, unos 15 ms); la app, que cancela la ruta sin video, sin `SENS` o con `WD` (0.7 a 1.5 s); el watchdog de comandos (1 s); el de hardware, que reinicia el chip a los 5 s. Último recurso: el interruptor del portapilas. En el carro se observó el watchdog con la app caída (15-oct, sin cronómetro); el paro láser aislado y el de hardware, **NO MEDIDO**. (Manual: G.2)

**P:** ¿Qué pasa si se pierde el WiFi con el carro en marcha?  
**R:** Sin órdenes por 1,000 ms, el watchdog del carro apaga los motores y vuelve al perfil AUTO. El modelo da un recorrido máximo de unos 21 cm en AUTO (**SIMULADO**; con 19.3 cm/s, y si va a unos 30 cm/s sería más). Se observó con la app cerrada, pero el corte de WiFi en marcha es **NO MEDIDO** (prueba B9). (Manual: G.2)

**P:** ¿Cómo frena el carro ante un obstáculo y en cuánto tiempo?  
**R:** El láser mide cada 33 ms; con 2 lecturas seguidas bajo el umbral (120 mm en v1.1, 150 mm en v3) el ESP32 corta los motores sin esperar al teléfono. Solo actúa en perfil AUTO y avanzando. El modelo da de 33 a 103 ms hasta confirmar, más la inercia; la distancia de parada medida no existe (**NO MEDIDO**). (Manual: G.2)

**P:** ¿Hay un interruptor para apagar el modo autónomo y pasar a manual?  
**R:** No uno solo. Lo autónomo es una ruta en marcha: se corta con PARO y se pasa a la pestaña Manual, donde el carro solo se mueve mientras se mantiene un botón apretado. «Usar sensor de distancia (láser)» apaga el freno por láser, no el modo autónomo. Paso a paso, en la sección 4. (Manual: G.2)

### H · Simulador y pruebas

**P:** ¿Qué tan confiable es el simulador?  
**R:** Sirve para comparar versiones en las mismas condiciones y encontrar errores de lógica, no para prometer cómo se porta el carro. Usa el navegador, el detector de color y la fusión reales; solo el mundo y el carro son sintéticos, con supuestos marcados, y se ajustó con cuatro rectas y un esquive medidos en octubre. Toda cifra de v3 es **SIMULADO**. (Manual: H.3)

**P:** ¿Qué es el Monte Carlo y para qué sirve la semilla?  
**R:** Repite la misma prueba miles de veces con variaciones al azar y resume media y p95; la semilla hace que el azar se repita igual. Se usan `160000 + índice de escenario × 10007 + corrida` en precisión y `170000 + índice × 10007 + corrida` en el banco autónomo. No citen `0xF51A2026` (solo aparecía en documentos de v1.1) ni `0xC0FFEE01` (no está en ningún archivo ni commit). (Manual: H.3)

**P:** ¿Cuántas pruebas automáticas hay y qué cubren?  
**R:** El 24-nov-2026, al armar la v1.2: 1,901 casos en 24 programas del núcleo en C y 567 en 5 del firmware, con sensores falsos, más 60 del servidor falso y 21 del verificador de versiones, sin fallos (**SIMULADO**). El banco (23 escenarios × 1,000 corridas) corre aparte y deja 23 compuertas en rojo para v3. (Manual: H.4)

### I · Proceso de desarrollo

**P:** ¿Cómo lo hicieron y qué versiones hubo?  
**R:** Empezó como «WALL-E», un sketch de Arduino de un solo archivo. Se separó la lógica en C99 probada en PC y vinieron v1.0 (27-oct, probada en pista), v1.1 (10-nov, validada en el carro), v2 y v3 (simuladas), v4 a v6 (probadas en el banco y descartadas) y v1.2 (24-nov) = v3 + revisión de interfaz. (Manual: I.2)

**P:** ¿Por qué la v1.2.1 usa la v3 y no la v6 ni la v1.1?  
**R:** v4, v5 y v6 se descartaron en simulación con ruido realista (23 compuertas en rojo; ramas archivadas). Frente a v1.1, v1.1 **no** es respaldo seguro porque su paro a 120 mm deja solo 1.5–3.5 cm de margen y puede golpear el sensor. Por eso la decisión es Opción 1 con condiciones: exponer v3 autónomo con `FW:b396390` (`SEG=3`, 150/180 mm) y las dos correcciones del 26-nov (Plan B: mismo firmware en manual). (Manual: I.3)

**P:** ¿Qué salió mal en el camino?  
**R:** El esquive de v1.0 (27-oct) no hizo un rodeo limpio; se corrigió en J15 y J16 y se validó el 10-nov. En v5, un banco sin ruido dio «G1 cumple» y no se sostuvo al calibrarlo con las 7 cajas negras del 15-nov. Y la prueba en el teléfono encontró un cierre de la app (Manual en horizontal) que la lectura del código no vio. (Manual: I.3)

### Preguntas de fondo

**P:** ¿Por qué una red propia y no el WiFi de la escuela?  
**R:** Así el teléfono, la cámara y el carro se hablan sin router externo ni internet (no hubo otra razón registrada además de no depender de un router externo); procesar video en la nube o en una PC se descartó por latencias de más de 300 ms. El canal es fijo para que la cámara no busque canal tras una caída. (Manual: B.3)

**P:** ¿Por qué láser y cámara?  
**R:** Se complementan. La cámara ve ancho y lejos, pero llega unos 320 ms tarde y la luz la engaña; el láser mide milímetros desde el propio carro, pero es un solo haz de unos 25° y puede no dar eco con objetos oscuros, mates, transparentes o patas finas. Por eso el freno de emergencia es del láser y la cámara ayuda a elegir por dónde rodear. (Manual: G.3)

**P:** ¿Por qué C y JNI?  
**R:** Se eligió C porque se vio C en clase (FSI). Además (análisis del repo), el mismo código en C99 se compila con GCC en la PC y con el NDK en el teléfono para probar miles de rutas sin emulador ni carro. JNI es el puente por el que Kotlin llama a esas funciones (`puente.c`, 145 funciones). No medimos si es más rápido que Kotlin (**NO MEDIDO**). (Manual: E.3)

**P:** ¿Qué es una red neuronal aquí y quién la entrenó?  
**R:** Es una función con millones de números ajustados de antemano con muchas fotos etiquetadas: recibe una imagen y devuelve cajas con clase y confianza. Usamos EfficientDet-Lite0 (80 clases de COCO), de los ejemplos oficiales de TensorFlow Lite, tal cual; el repositorio no trae código ni datos de entrenamiento, y ni quién la entrenó exactamente ni la licencia Apache 2.0 en la fuente primaria están verificados (**no verificado**). Solo marca cajas con confianza de 0.6 o más y no decide sola: debe coincidir con el color. (Manual: F.3)

**P:** ¿Cómo saben que funciona?  
**R:** Por niveles, y cada resultado lleva su etiqueta. Pruebas en PC: 1,901 casos del núcleo y 567 del firmware, sin fallos (**SIMULADO**). Banco: miles de corridas con semillas fijas (**SIMULADO**). En el carro, solo la v1.1: el 10-nov, 6 de 6 corridas sin tocar la caja y 5 de 6 completas. De la v3 no hay corridas en el piso. (Manual: I.4)

**P:** ¿Qué no funciona todavía?  
**R:** En el banco, v3 no cumple sus metas con muebles de patas (banquito: 43 contactos en 1,000; silla: 21), con una pared pegada a la ruta (386 AYUDA en 1,000) ni con sol, sombra y reflejo (paradas en 93, 82 y 126 de 1,000); un bloque oscuro sin eco terminó en contacto 40 de 40 veces (**SIMULADO**). En el carro, la corrida 5 del 10-nov entró en AYUDA sin causa identificada. (Manual: H.4)

**P:** ¿Qué haría falta para mejorarlo?  
**R:** Primero medir en el carro con el protocolo ya escrito: M8, M1, M5 y M6 con el firmware actual; luego M2, M3, M4, M7, M9 y M10, y los bloques A y B. Después, decidir si se graba `SEG=3`. De diseño siguen sin hacerse un tope global de continuaciones tras AYUDA, leer el estado de la medición del láser y detectar atascos. (Manual: H.2)

**P:** ¿Qué explica cada integrante en la exposición?  
**R:** Presentación general: todos. Cada quien explica su especialidad (sección 3) y todos deben entender la visión general del sistema. (Manual: 1.5)

**P:** ¿Usaron herramientas de inteligencia artificial?  
**R:** «Nos asistimos de herramientas de inteligencia artificial para la programación. Entendemos el funcionamiento del sistema y la guiamos para construir lo que queríamos.» El curso no exige declararlo, pero lo permite, y por eso el equipo lo declara (sección 6). (Manual: I.2)

## 3. Quién explica qué en la exposición

Principio de la guía: **todos deben entender todo; cada quien se especializa en lo suyo** (presentación general: todos).

| Subsistema | Qué cubre | Quién lo explica en la exposición |
|---|---|---|
| [A · Hardware](manual/capitulos/01d_subsistema_A_hardware.md) | ESP32 DevKit, driver TC1508A (`INT1`–`INT4`), motores TT, ruedas mecanum, giroscopio, láser, energía | Jaime Fernando Guerrero Segoviano |
| [B · Firmware](manual/capitulos/01e_subsistema_B_firmware.md) | Programa del carro: órdenes, `GIRAR` y `RUMBO`, paro por láser (`SEG=3`), watchdogs | Jaime Fernando Guerrero Segoviano |
| [C · Cámara](manual/capitulos/01f_subsistema_C_camara.md) | Freenove ESP32-WROVER y sensor GC0308, tres modos de video, recuperación, **red propia `CarroFSI`** | Melanie Contreras Solis |
| [D · App Android](manual/capitulos/01g_subsistema_D_app_android.md) | Pantallas, sesiones con carro y cámara, rutas, PARO y AYUDA, caja negra, revisión de interfaz | Vitto Eduardo Rangel Fortuna |
| [E · Núcleo en C](manual/capitulos/01h_subsistema_E_nucleo_c.md) | Navegador de ocho estados, pose, calibración, puente JNI, correcciones del 26-nov | Jaime Fernando Guerrero Segoviano |
| [F · Visión](manual/capitulos/01i_subsistema_F_vision.md) | Color del piso, red EfficientDet-Lite0, fusión con el láser (`solo_laser`), corredor | Jaime Fernando Guerrero Segoviano |
| [G · Seguridad](manual/capitulos/01j_subsistema_G_seguridad.md) | Capas de paro, watchdogs, AYUDA, qué pasa si falla el WiFi o la app | Azul Abril Cadena Campos |
| [H · Simulador y pruebas](manual/capitulos/01k_subsistema_H_simulador_pruebas.md) | Carro virtual, banco Monte Carlo, pruebas en PC, protocolo físico | Jaime Fernando Guerrero Segoviano |
| [I · Proceso](manual/capitulos/01l_subsistema_I_proceso.md) | Versiones v1.0 a v1.2.1, lo descartado, evidencia y etiquetas | Jaime Fernando Guerrero Segoviano |

*Nota sobre operación:* **Jaime Fernando Guerrero Segoviano** armó e integró el sistema del carro, operó el carro en las pruebas físicas y lo opera en la demostración.

### Lo que todos deben saber (10 puntos)

1. Tres nodos en red local propia (`CarroFSI`, 2.4 GHz): carro (`ESP32 DevKit`), cámara (`ESP32-WROVER`) y teléfono Android.
2. La cámara comparte la *powerbank* del DevKit; los motores van aparte con 4 pilas AA Energizer vía `TC1508A` (`INT1`–`INT4`).
3. Sin *encoders*: distancia por tiempo (`ms_por_cm`) y giros/rumbo con el giroscopio `MPU6050`.
4. Láser `VL53L0X` (3.5 cm del suelo, sobresale 3.5 cm): frena en el carro a 150 mm (`FW:b396390`, `SEG=3`) y bloquea arranque bajo 180 mm.
5. Núcleo en `C99`: mismo código en PC (pruebas/simulador) y teléfono (JNI), respondiendo cada 50 ms.
6. Visión: color del piso + `EfficientDet-Lite0`; con las correcciones del 26-nov, cuando el ToF está fresco manda el láser real en el centro.
7. Si no puede esquivar con seguridad, pasa a `AYUDA`, detiene el carro y cede la cruceta al operador.
8. Watchdog de comandos apaga motores a los 1,000 ms sin latido (250 ms); watchdog de hardware reinicia a los 5,000 ms.
9. Exposición: Opción 1 con condiciones (v3 + `FW:b396390` + correcciones del 26-nov; Plan B: mismo firmware en manual); v1.1 no es respaldo seguro.
10. Etiquetas honestas en todo: las corridas buenas del 26-nov son `n=3` en los mismos escenarios donde falló, y `RUMBO:ON` en Manual sigue pendiente.

### Su especialidad (3 preguntas más probables por integrante)

- **Jaime Fernando Guerrero Segoviano (A, B, E, F, H, I; operación):** (1) ¿Qué cambió en `navegador.c` el 26-nov (`solo_laser` y `PARALELO`) y por qué? (2) ¿Por qué v3 con `SEG=3` (150/180 mm) y no v1.1 (120 mm) ni v4–v6? (3) ¿Qué se midió el 25–26 nov (M1, M3, M5, M6, M8 AUTO, mochila, bajos) y qué falta (M2, `RUMBO:ON` Manual, C y D)?
- **Vitto Eduardo Rangel Fortuna (D · App Android):** (1) ¿Qué decide Kotlin frente al núcleo en C por JNI? (2) ¿Qué vigila la app (`SENS`, video, `WD`, `TOF_PARO`) y qué hace `ManualFragment` con `RUMBO`? (3) ¿Qué cambió en la revisión de interfaz y qué guarda la caja negra?
- **Melanie Contreras Solis (C · Cámara y red `CarroFSI`):** (1) ¿Por qué una placa aparte para la cámara y por qué RGB565 HQVGA con `frame2jpg`? (2) ¿Cómo funciona `CarroFSI` (canal 1, `.1` y `.2`, DHCP `.10–.20`) y cómo evita acumular retraso (~320 ms)? (3) ¿Qué hace `recuperacion.c` ante caídas de WiFi, servidor o captura?
- **Azul Abril Cadena Campos (G · Seguridad):** (1) ¿En qué orden actúan las capas de paro desde el láser hasta el portapilas? (2) ¿Por qué el freno láser solo actúa en AUTO y cómo funciona `TOF_BLOQUEO=180`? (3) ¿Qué pasa si cae el WiFi, se congela la app o se pulsa «Continuar» tras `AYUDA` sin mover el carro?

## 4. Qué decir si algo falla en la demostración

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
3. **Un solo código, dos plataformas:** el núcleo en C se compila con GCC en PC y con el NDK en el teléfono; 1,901 casos de prueba en 24 programas, sin fallos el 24-nov.
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
- **Histórico v1.1 (10-nov-2026):** B6 (`n=6`, 6/6 sin contacto, 5/6 completas). No es respaldo seguro por paro a 120 mm.

**SIMULADO** (PC, banco, teléfono con carro y cámara falsos)

- Banco de 23 escenarios × 1,000 corridas de v3 (16-nov-2026), con 23 compuertas en rojo; objeto con 0 % de eco IR sigue solo simulado.
- Revisión de interfaz de la app (24-nov-2026): probada con carro y cámara falsos; cuadros lentos de 19.2 % a 3.3 % o menos.
- Monte Carlo de precisión J16 (31-oct-2026), 1,000 corridas por escenario.

**NO MEDIDO / PENDIENTE**

- M8 en Manual (límite de seguridad), M2 (patas reales), objeto con 0 % de eco IR, M4, M7, M9, corte de WiFi en marcha (B9), bloques C y D (luz cambiante, ruta en L, cuadrado), prueba física de `RUMBO:ON` en modo Manual (**PENDIENTE**), inclinación/FOV de cámara e inclinación del láser, corrientes y autonomía.

## 6. Uso de herramientas de IA en el desarrollo

Declaración del equipo:

> «Nos asistimos de herramientas de inteligencia artificial para la programación. Entendemos el funcionamiento del sistema y la guiamos para construir lo que queríamos.»

El curso no exige declararlo, pero lo permite, y por eso el equipo lo declara.

Lo que muestra el repositorio:

- En el historial de git, una sola cuenta (`fernandosegrr`) concentra los commits y muchos llevan `Co-Authored-By` de agentes de código usados para escribir y revisar código, documentación y pruebas.
- Los resultados del carro y las pruebas en PC se respaldan con archivos, fechas y salidas reales; explicar cada subsistema en la exposición corresponde a cada integrante (sección 3).
- Operó el carro en las pruebas físicas y lo opera en la demostración: **Jaime Fernando Guerrero Segoviano**.

## 7. Lo que aún no validamos

Para decir en voz alta. Nada de la v3 se ha movido en el piso; el protocolo físico está escrito y dice que nada se ha ejecutado ([`limites_conocidos.md`](limites_conocidos.md), [`estado_v3_pendiente_pruebas.md`](estado_v3_pendiente_pruebas.md)).

- Todavía no hemos probado en el piso el autónomo v3 completo, ni grabado su firmware (`SEG=3`).
- Todavía no hemos probado el esquive con cada tipo de obstáculo: caja corrida, banquito con patas, objeto largo, caja con pared al lado. Solo hay seis corridas con una caja de 20×20 cm, y con la v1.1.
- Todavía no hemos medido el paro por láser con cinta métrica: ni la distancia real de frenado (M5) ni el arranque junto a una pared (M6).
- Todavía no hemos cortado el WiFi con el carro en marcha, ni tapado la cámara 2 segundos (bloque D).
- Todavía no hemos calibrado la escala del giroscopio con una vuelta completa en el piso; hoy está en cero.
- Todavía no hemos probado la visión con otra luz u otro piso: sombra, sol, linterna, aprender el piso con una luz y correr con otra (bloque C y M4).
- Todavía no sabemos si el láser ve patas reales (M2) ni cómo ve una pared de lado (M3).
- Todavía no hemos medido la velocidad real en AUTO (M8), la latencia de extremo a extremo (M9), el ritmo del láser (M10), la deriva del giroscopio (M1) ni los giros cortos (M7).
- Todavía no hemos probado rutas con giros en el piso con la v3: en L, cuadrado, caja antes de una esquina.
- Todavía no hemos probado la interfaz nueva con el carro real y su cámara; solo con carro y cámara falsos.
- Todavía no sabemos qué pasó en la corrida 5 del 10-nov, ni qué hace «Continuar» tras una AYUDA sin mover el carro.
- Todavía no hemos medido corrientes, tensión de las pilas ni autonomía.

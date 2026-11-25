# Guía de exposición — Carro FSI v1.2 (1 de diciembre de 2026)

- **Versión:** v1.2 — base v3; autónomo v3 **SIMULADO**, pendiente de validar en el carro; lo validado en el carro es v1.1, 10-nov-2026.
- **Cómo usarla:** léanla completa una vez. El guion (sección 1) se dice en voz alta, las preguntas (sección 2) van por subsistema y la sección 4 es para cuando algo falle en vivo. Cada respuesta cierra con `(Manual: X.n)`, la sección del manual que la explica.
- **Manual:** [`manual/Manual_Proyecto_Carro_FSI.pdf`](manual/Manual_Proyecto_Carro_FSI.pdf). Lo que el equipo debe confirmar está en [`pendientes_confirmar.md`](pendientes_confirmar.md).
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

El freno de emergencia es del láser, no de la cámara, porque el video llega unos 320 milisegundos tarde y la luz lo engaña. Si dos lecturas seguidas marcan menos de 120 milímetros, la ESP32 corta los motores sin esperar al teléfono. Eso es lo que tiene grabado hoy el firmware v1.1. El v3 lo sube a 150, pero solo está compilado.

*Si graban el firmware v3 antes de exponer, cambien este bloque: el paro pasa a 150 milímetros.*

**3:00 · Cómo se protege (2)**

Si pasa un segundo sin órdenes, el carro apaga los motores solo; la app repite la orden cada 250 milisegundos. La app cancela la ruta si falta el video o los datos del carro, y hay un botón rojo de PARO. El interruptor del portapilas corta la energía de los motores.

**3:30 · Cómo lo probamos**

Probamos en tres niveles. Pruebas automáticas en PC: 1,901 casos del núcleo y 567 del firmware, sin fallos en la corrida del 24 de noviembre. Un simulador que repite mil corridas por escenario con semillas fijas. Y el carro real, pero solo con la v1.1: el 10 de noviembre esquivó seis veces una caja de 20 por 20 centímetros sin tocarla, y terminó cinco de seis.

**4:00 · Lo que NO hemos probado**

Lo importante: el autónomo v3 es simulado. Nunca ha corrido en el piso. En el simulador todavía no cumple sus metas con muebles de patas, con una pared pegada a la ruta ni con manchas de sol. Tampoco hemos medido la distancia real de frenado del láser, ni cortado el WiFi con el carro en marcha, ni probado la interfaz nueva con el carro real.

**4:30 · Cierre**

Por eso etiquetamos cada resultado: validado en el carro, simulado, no medido o pendiente. Lo validado es poco y lo decimos. Lo demás está escrito, con su protocolo de prueba física listo. Gracias; con gusto respondemos preguntas.

## 2. Preguntas probables

Respuestas cortas y honestas. Cifras de pruebas en PC: las de la corrida del 24-nov-2026 al armar la v1.2 (en el núcleo coinciden con [`estado_20261116.md`](estado_20261116.md)); [`exposicion.md`](exposicion.md) conserva cifras anteriores.

### A · Hardware

**P:** Si las ruedas son mecanum, ¿por qué el carro no se mueve de lado?  
**R:** El driver TC1508A tiene solo dos canales: las ruedas de cada lado reciben la misma señal, así que el carro es una tracción diferencial, como un tanque. Para ir de lado habría que controlar las diagonales por separado. El repositorio no dice por qué se eligieron ruedas mecanum. (Manual: A.2)

> CONFIRMAR CON EL EQUIPO: ¿Por qué ruedas mecanum si con un driver de dos canales no hay movimiento lateral?

**P:** ¿Por qué hay fuentes de energía separadas para los motores y la electrónica?  
**R:** Los motores TT dan picos de corriente y ruido, y la ESP32 se reinicia si su tensión baja demasiado; por eso las 4 pilas AA alimentan solo el driver y comparten la tierra. Es una razón de diseño: corrientes y tensiones bajo carga **NO MEDIDO**. El README cuenta tres fuentes (la cámara con powerbank propia); `exposicion.md`, dos. (Manual: A.3)

> CONFIRMAR CON EL EQUIPO: ¿La cámara lleva su propia powerbank o comparte la del DevKit?

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
**R:** El grabado es el v1.1 (`FW:6d1b213-20261110`, 10-nov): paro láser a 120 mm. El v3 (`SEG=3`) sube el paro a 150 mm, no deja arrancar con el frente a menos de 180 mm y tolera un «sin eco» suelto. Solo está compilado y probado en PC (**SIMULADO**); los 150 mm salen de un modelo, no de una medición. (Manual: B.3)

> CONFIRMAR CON EL EQUIPO: ¿Se grabará el firmware v3 (`SEG=3`) antes del 1 de diciembre o llega el v1.1?

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
**R:** Cada uno se equivoca distinto: el color con juntas de baldosa y sombras, la red con cajas inventadas; juntos filtran a ambos, aunque con sol o sombra fallan a la vez. El láser confirma un obstáculo a 400 mm o menos, pero si está libre o sin eco no borra lo que ven color y red. Si solo una de las dos ve el obstáculo y el láser no, el carro no frena. (Manual: F.3)

**P:** ¿Cómo calculan la distancia con una sola cámara?  
**R:** No la calculan en centímetros: usan qué tan abajo llega el obstáculo en la imagen y lo reparten en tres bandas de cercanía. El «corredor» sí convierte filas en centímetros, pero está apagado: altura, inclinación y campo de visión de la cámara son valores de partida sin medir (**NO MEDIDO**). La distancia medida la da el láser. (Manual: F.2)

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

**P:** ¿Por qué la v1.2 usa la v3 y no la v6 ni la v1.1?  
**R:** Ninguna versión nueva cumplió las compuertas a ciegas, y en piso limpio v3 salió mejor que v5-fix y v6; todo en simulador. Frente a la v1.1, v3 mejora con muebles de patas en el banco (banquito: 849 ok contra 245), pero tiene 23 compuertas en rojo y no está validada en el carro; por qué se prefirió a la v1.1 no está escrito. (Manual: I.3)

> CONFIRMAR CON EL EQUIPO: ¿Por qué v3 y no v1.1 con la revisión de UI como base de la exposición?

**P:** ¿Qué salió mal en el camino?  
**R:** El esquive de v1.0 (27-oct) no hizo un rodeo limpio; se corrigió en J15 y J16 y se validó el 10-nov. En v5, un banco sin ruido dio «G1 cumple» y no se sostuvo al calibrarlo con las 7 cajas negras del 15-nov. Y la prueba en el teléfono encontró un cierre de la app (Manual en horizontal) que la lectura del código no vio. (Manual: I.3)

### Preguntas de fondo

**P:** ¿Por qué una red propia y no el WiFi de la escuela?  
**R:** Así el teléfono, la cámara y el carro se hablan sin router externo ni internet; procesar video en la nube o en una PC se descartó por depender de internet y por latencias de más de 300 ms. El canal es fijo para que la cámara no busque canal tras una caída. Ningún texto compara con el WiFi de la escuela. (Manual: B.3)

> CONFIRMAR CON EL EQUIPO: ¿Hubo otra razón para crear una red propia además de no depender de un router externo?

**P:** ¿Por qué láser y cámara?  
**R:** Se complementan. La cámara ve ancho y lejos, pero llega unos 320 ms tarde y la luz la engaña; el láser mide milímetros desde el propio carro, pero es un solo haz de unos 25° y puede no dar eco con objetos oscuros, mates, transparentes o patas finas. Por eso el freno de emergencia es del láser y la cámara ayuda a elegir por dónde rodear. (Manual: G.3)

**P:** ¿Por qué C y JNI?  
**R:** El mismo código en C99 se compila con GCC en la PC y con el NDK en el teléfono: así lo probamos con simulaciones de miles de rutas, sin emulador ni carro. JNI es el puente por el que Kotlin llama a esas funciones (`puente.c`, 145 funciones). No medimos si es más rápido que Kotlin (**NO MEDIDO**). (Manual: E.3)

> CONFIRMAR CON EL EQUIPO: ¿Por qué C99 y no Kotlin o C++ para el navegador, y se probó otra opción?

**P:** ¿Qué es una red neuronal aquí y quién la entrenó?  
**R:** Es una función con millones de números ajustados de antemano con muchas fotos etiquetadas: recibe una imagen y devuelve cajas con clase y confianza. Usamos EfficientDet-Lite0 (80 clases de COCO), de los ejemplos oficiales de TensorFlow Lite, tal cual; el repositorio no trae código ni datos de entrenamiento. Solo marca cajas con confianza de 0.6 o más y no decide sola: debe coincidir con el color. (Manual: F.3)

> CONFIRMAR CON EL EQUIPO: ¿Quién entrenó ese archivo y alguien verificó su licencia Apache 2.0 en la ficha oficial?

**P:** ¿Cómo saben que funciona?  
**R:** Por niveles, y cada resultado lleva su etiqueta. Pruebas en PC: 1,901 casos del núcleo y 567 del firmware, sin fallos (**SIMULADO**). Banco: miles de corridas con semillas fijas (**SIMULADO**). En el carro, solo la v1.1: el 10-nov, 6 de 6 corridas sin tocar la caja y 5 de 6 completas. De la v3 no hay corridas en el piso. (Manual: I.4)

**P:** ¿Qué no funciona todavía?  
**R:** En el banco, v3 no cumple sus metas con muebles de patas (banquito: 43 contactos en 1,000; silla: 21), con una pared pegada a la ruta (386 AYUDA en 1,000) ni con sol, sombra y reflejo (paradas en 93, 82 y 126 de 1,000); un bloque oscuro sin eco terminó en contacto 40 de 40 veces (**SIMULADO**). En el carro, la corrida 5 del 10-nov entró en AYUDA sin causa identificada. (Manual: H.4)

**P:** ¿Qué haría falta para mejorarlo?  
**R:** Primero medir en el carro con el protocolo ya escrito: M8, M1, M5 y M6 con el firmware actual; luego M2, M3, M4, M7, M9 y M10, y los bloques A y B. Después, decidir si se graba `SEG=3`. De diseño siguen sin hacerse un tope global de continuaciones tras AYUDA, leer el estado de la medición del láser y detectar atascos. (Manual: H.2)

**P:** ¿Qué parte hizo cada quien?  
**R:** El repositorio no lo dice: los 269 commits anteriores a la v1.2 salen con una sola cuenta y la documentación no reparte el trabajo. No contesten con nombres que no puedan respaldar: cada persona dice qué parte hizo y la explica con su capítulo del manual (sección 3). Si algo lo hizo una herramienta de IA, díganlo (sección 6). (Manual: I.2)

> CONFIRMAR CON EL EQUIPO: ¿Quién hizo qué parte?

**P:** ¿Usaron herramientas de inteligencia artificial?  
**R:** El repositorio muestra agentes de código: 168 de los 269 commits anteriores a la v1.2 traen la línea `Co-Authored-By` de Claude. Hay que decirlo tal cual y poder explicar qué decidió y revisó el equipo. Los hechos y lo que solo el equipo puede contestar están en la sección 6. (Manual: I.2, y sección 6 de esta guía)

## 3. Quién explica qué

Pueden repartirse más de un subsistema por persona. Escriban el nombre en la última columna.

| Subsistema | Qué cubre | Responsable |
|---|---|---|
| [A · Hardware](manual/capitulos/01d_subsistema_A_hardware.md) | ESP32 DevKit, driver TC1508A, motores TT, ruedas mecanum, giroscopio, láser, energía |   |
| [B · Firmware](manual/capitulos/01e_subsistema_B_firmware.md) | Programa del carro: red `CarroFSI`, órdenes, `GIRAR` y `RUMBO`, paro por láser, watchdogs |   |
| [C · Cámara](manual/capitulos/01f_subsistema_C_camara.md) | Freenove ESP32-WROVER y sensor GC0308, tres modos de video, recuperación |   |
| [D · App Android](manual/capitulos/01g_subsistema_D_app_android.md) | Pantallas, sesiones con carro y cámara, rutas, PARO y AYUDA, caja negra, revisión de interfaz |   |
| [E · Núcleo en C](manual/capitulos/01h_subsistema_E_nucleo_c.md) | Navegador de ocho estados, pose, calibración, puente JNI |   |
| [F · Visión](manual/capitulos/01i_subsistema_F_vision.md) | Color del piso, red EfficientDet-Lite0, fusión con el láser, corredor |   |
| [G · Seguridad](manual/capitulos/01j_subsistema_G_seguridad.md) | Capas de paro, watchdogs, AYUDA, qué pasa si falla el WiFi o la app |   |
| [H · Simulador y pruebas](manual/capitulos/01k_subsistema_H_simulador_pruebas.md) | Carro virtual, banco Monte Carlo, pruebas en PC, protocolo físico |   |
| [I · Proceso](manual/capitulos/01l_subsistema_I_proceso.md) | Versiones v1.0 a v1.2, lo descartado, evidencia y etiquetas |   |

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

**VALIDADO EN EL CARRO** (solo v1.1; [`estado_20261110.md`](estado_20261110.md) y [`pruebas_fisicas_20261110.csv`](pruebas_fisicas_20261110.csv))

- 10-nov-2026, B0: firmware `FW:6d1b213-20261110` y app `APP:0.4-6d1b213` conectados por WiFi; giroscopio y láser listos, `RUMBO=1`, escala del giroscopio en 0.
- 10-nov-2026, B6: recta de 200 cm con una caja de 20×20 cm a unos 100 cm, 6 corridas. 6 de 6 sin tocar la caja; 5 de 6 completas (punto 10 de 10), con error lateral aproximado de ~10, ~10, ~5, ~0 y ~0 cm, medido a ojo.
- La corrida 5 entró en AYUDA en el punto 4 de 10, sin causa identificada, y no terminó sola.
- 23 giros de esas corridas: error de −3° a +5°; 21 de 23 dentro de ±3°, según el propio giroscopio (cuenta hecha con los datos del CSV).
- Hay otras observaciones en el carro con firmware anterior y siete rectas del 15-nov (v1.1, app sin versión anotada). No cuentan aquí como validación de la v1.2.

**SIMULADO** (PC, banco, solo compilado, teléfono con carro y cámara falsos)

- Todo el autónomo v3 y sus cifras: banco de 23 escenarios × 1,000 corridas (16-nov-2026), con 23 compuertas en rojo.
- Pruebas en PC del 24-nov-2026 (v1.2): 1,901 casos del núcleo y 567 del firmware, 60 del servidor falso y 21 del verificador de versiones, 0 fallos.
- Firmware `SEG=3` (paro a 150 mm): solo compilado, nunca grabado. La v1.2 (app y firmware) solo se compiló.
- Revisión de interfaz de la app (24-nov-2026, [`revision_ui_android.md`](revision_ui_android.md)): probada en un teléfono con carro y cámara falsos, no con el carro real; cuadros lentos de 19.2 % a 3.3 % o menos.
- Monte Carlo de precisión J16 (31-oct-2026), 1,000 corridas por escenario.

**NO MEDIDO** (no hay medición)

- Distancia de frenado del paro láser y distancia del sensor al frente (M5, M6).
- Corte de WiFi en marcha (B9).
- Si el láser ve patas reales, objetos oscuros o sol (M2).
- Velocidad real en AUTO (¿19.3 o unos 30 cm/s?, M8) y latencia de extremo a extremo (M9).
- Ángulo físico y escala del giroscopio con cinta, y cuánto mejora `RUMBO`.
- Corrientes, tensión de las pilas y autonomía.

**PENDIENTE** (el repositorio lo documenta como falta)

- Protocolo físico de la v3 ([`protocolo_prueba_fisica_autonomo_v2.md`](protocolo_prueba_fisica_autonomo_v2.md)): M1 a M10 y bloques A, B1, B2, C y D. Nada se ha ejecutado.
- Grabar `SEG=3` en el carro.
- La interfaz nueva con el carro y la cámara reales (lista de 10 minutos en `revision_ui_android.md`).
- Los modos de video raw y celdas con la cámara real.

## 6. Uso de herramientas de IA en el desarrollo

**REVISAR Y AJUSTAR POR EL EQUIPO ANTES DE EXPONER**

Lo que muestra el repositorio (revisado con `git log` el 24-nov-2026):

- Antes de la v1.2 el historial tenía 269 commits en todas las ramas y etiquetas; los de la v1.2 se suman a esos. Los 269 tienen el mismo nombre de autor, `fernandosegrr`: una sola cuenta de GitHub.
- De esos 269, 168 llevan un trailer `Co-Authored-By`: 149 de «Claude Sonnet 5.5», 10 de «Claude Opus 5.5» y 9 de «Claude Fable 5.2». Los commits de la v1.2 también lo llevan.
- Existe la rama remota `copilot/fsi-proyect`. Sus commits también salen con ese mismo autor y todos están ya en `main`.
- `docs/bitacora.md` (línea 359) y `docs/validacion_sensores.md` (líneas 41 y 62) mencionan a Claude: la regla «Claude no mueve el carro» y pruebas del 20-oct «hechas por Claude con la app por adb». Los dos textos no coinciden del todo.
- El repositorio muestra agentes de código usados para escribir y revisar código, documentación y pruebas.

Qué significa para la exposición:

- Se puede decir con verdad que se usaron herramientas de IA en el desarrollo y que el historial lo registra.
- No se puede decir que todo lo escribió el equipo a mano, ni que lo hizo todo la IA: el historial no separa lo que decidió una persona de lo que propuso la herramienta.
- Una sola cuenta de autor no prueba quién hizo qué. No la usen para repartir el trabajo.
- Los resultados del carro (10-nov) y las pruebas en PC se respaldan con archivos y fechas, sin importar quién escribió el código. Explicar el código sí depende de cada persona: cada quien debe poder explicar su parte sin ayuda (sección 3).
- Si el profesor pregunta cuánto hizo la IA, contesten con lo que saben y con lo que muestra el repositorio. No lo minimicen ni lo adornen.

Pendiente del equipo:

- CONFIRMAR CON EL EQUIPO: ¿Qué decidió y revisó el equipo, y qué propuso o escribió una herramienta de IA?
- CONFIRMAR CON EL EQUIPO: ¿El curso o el profesor piden declarar el uso de IA, y de qué forma?
- CONFIRMAR CON EL EQUIPO: ¿Quién entiende y puede explicar cada parte del sistema sin ayuda?
- CONFIRMAR CON EL EQUIPO: ¿Quién operó el carro y anotó los resultados en las pruebas físicas?

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

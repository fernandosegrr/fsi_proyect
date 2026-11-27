# Subsistema F · Visión

## F.1 Qué es y para qué sirve

La visión es la parte de la app que mira el video de la cámara del carro y dice si hay algo adelante y qué tan cerca. Usa dos detectores: uno de color, que marca lo que no se parece al piso, y una red neuronal que dibuja cajas sobre objetos. Una zona cuenta como obstáculo solo si los dos coinciden; el láser VL53L0X puede confirmar por su cuenta. La visión no mueve el carro: entrega zonas y bandas de cercanía al navegador (subsistema E). Más detalle: capítulo 8 del manual (`08_vision.md`).

## F.2 Cómo funciona, paso a paso

El cuadro de video sigue este camino. Los pasos 4 y 5 son opcionales y no están en el diagrama.

```
┌────────┐   JPEG, WiFi    ┌─────────────────────────────────────────┐
│ cámara │ ──────────────▶ │ 1  lee ▶ decodifica ▶ analiza (240x176) │
└────────┘                 └───────────┬───────────────────┬─────────┘
                  ┌────────────────────┘                   └───────────┐
                  ▼                                                    ▼
       ┌────────────────────┐                            ┌─────────────────────┐
       │ 2  color del piso  │                            │ 3  red EfficientDet │
       └─────────┬──────────┘                            └──────────┬──────────┘
                 │ zonas IZQ/CEN/DER + banda 1-3                    │ cajas ≥ 0.6
                 └──────────────────────┬───────────────────────────┘
                                        ▼
                            ┌────────────────────────┐
 láser VL53L0X ──────────▶  │ 6  fusión: color Y red │
  (≤ 400 mm confirma)       └───────────┬────────────┘
                                        ▼
                            ┌────────────────────────┐
                            │ 7  navegador (E)       │
                            └────────────────────────┘
```

1. **De la cámara al teléfono.** La cámara (subsistema C) manda cuadros de 240x176 píxeles, unos 12 por segundo, como JPEG por WiFi. `CamaraObstaculos.kt` los pasa por tres hilos (leer, decodificar y analizar) con una cola de uno entre ellos: si llega un cuadro y el anterior sigue esperando, se tira el viejo, para que el retraso no se acumule.
2. **Color del piso** (`obstaculos.c`, C99).
   - Divide el cuadro en celdas de 10x10 píxeles (24 columnas por 17 filas) y promedia el color de cada una.
   - «Aprender piso» (carro quieto, 1 s, al menos 3 cuadros) agrupa los colores de la zona de avance con un k-means corto: hasta 4 grupos, 3 en la app. Un segundo color solo cuenta como piso si forma una banda de lado a lado sin tocar las filas cercanas (regla J10c: así una caja ancha no se aprende como piso). Si se descarta más del 5 % de las muestras, la ruta no sale.
   - Una celda es piso si, con algún grupo, cumple una regla: la suma de diferencias en R, G y B es ≤ 40 (`UMBRAL_COLOR`); tiene el mismo tono (color normalizado a ≤ 30, `UMBRAL_CROMA`) y su luz cambió ≤ 150; o ambas son oscuras (R+G+B menor que 150) y su luz difiere ≤ 90. Las celdas «no piso» sin vecinas «no piso» se borran.
   - Por zona (tercios izquierdo, central y derecho), de la fila mínima hacia abajo (44 % del alto: fila 7 de 17), hay obstáculo con al menos 7 celdas (`MIN_CELDAS`). La cercanía sale de la fila más baja del obstáculo: desde 83.4 % del alto (fila 14) vale 3, desde 66.7 % (fila 11) vale 2, y antes vale 1.
   - Avanzando sin obstáculo, el piso se corre 2 % por cuadro hacia lo que ve; en una ruta queda fijo (`piso_fijo`) y solo se reajusta el brillo.
3. **Red neuronal (EfficientDet-Lite0).** Una red neuronal es una función con millones de números ajustados de antemano con muchas fotos etiquetadas: recibe una imagen y devuelve cajas con clase y confianza. La app solo la ejecuta. El archivo `efficientdet_lite0.tflite` (4,563,519 bytes) conoce 80 clases de COCO. `DetectorTflite.kt` estira el cuadro a 320x320 y `ModeloEnHilo.kt` corre la red en su propio hilo, máximo 10 veces por segundo. Se conservan las cajas con confianza ≥ 0.6, marcadas con la hora del cuadro; a los 300 ms (`vigencia_ms`) caducan. La clase no decide: cualquiera confirma al color.
4. **Movimiento.** Dos módulos, ninguno frena por sí solo. El giro visual (`movimiento_visual.c`) compara la huella de brillo de la mitad de arriba de la imagen entre dos cuadros y convierte el corrimiento en grados. El flujo del piso (`movimiento_piso.c`) compara, por bloques de 16x16, cuánto se movió cada bloque con lo que se movería un piso plano; está apagado y solo agrega avisos. En v1.1 (10-nov-2026, prueba B6) los giros se hicieron con el giroscopio del firmware, no con la cámara: con la cámara del carro ambos están **PENDIENTES** de medir; en v3 son **SIMULADOS**.
5. **Corredor de perspectiva** (`corredor.c`, apagado por defecto). Sin él, un objeto que pasa por un lado también frena. El corredor dibuja la huella del carro con perspectiva (ancha cerca, angosta lejos) y solo deja frenar lo que cae dentro. Usa el borde de abajo porque, con piso plano y nada colgando, un objeto toca el piso por ahí: entre más abajo está en la imagen, más cerca está. Con una cámara de agujero y solo enteros:

```
f    = (ancho / 2) / tan(FOV / 2)                f = 208 px con 240 px y 60°
A(v) = f·sen(incl) + (v − cy)·cos(incl)          v = fila de píxel, cy = alto / 2
d(v) = altura · (f·cos(incl) − (v − cy)·sen(incl)) / A(v)      cm hacia adelante
ancho en píxeles de x cm a esa distancia = x · A(v) / altura
A(v) ≤ 0  →  horizonte: de ahí para arriba no hay piso
```

   Valores de partida (`corredor.h`): carro de 18 cm, cámara a 10 cm del piso, inclinada 20°, campo de 60°, margen de 3 cm por lado (el simulador usaba 11.5 cm, 12° y 56°). En el carro físico la altura real de la cámara es **9 cm del suelo**; su inclinación y campo de visión siguen **NO MEDIDOS**. Con esos valores de partida, el cálculo con las fórmulas de `corredor.c` (hecho para este capítulo) da 11 cm para un borde de abajo en la fila 176, 15 cm en la 141, 20 cm en la 111 y 37 cm en la 71 (la fila mínima), y `docs/obstaculos.md` anota que una mochila se vio desde ~80 cm: no coinciden.
6. **Fusión con el láser** (`fusion.c`, `navegador.c`). Por zona, `hay_obstaculo = color && red`: el color marca la zona y una caja de la red (≥ 0.6, de menos de 300 ms) la cruza. La cercanía es la mayor de las dos y una fuente sola es solo un aviso. El láser manda milímetros cada 100 ms y confirma si es fresco (≤ 400 ms), válido y ≤ 400 mm (`OBSTACULO_LASER_MM`).
   > **ADVERTENCIA (actualizado con las correcciones del 26-nov-2026):** La versión anterior indicaba que un láser libre o sin eco nunca borraba una sospecha visual. Con `autoridad_laser = 1` y ToF fresco (`solo_laser` en `nucleo_c/navegador.c`), **con ToF fresco la visión sola no dispara esquive**: solo `laser_cerca` confirma obstáculo en `ZONA_CENTRO` (manda el láser; la visión sola solo avisa), y las reglas de visión pura (coincidencia visual, precaución, cercanía 3 inmediata, bordes laterales `borde_izq`/`borde_der`, `costado_a_la_vista` y chequeos por color en `tick_reincorporando`) quedan inhibidas por `!solo_laser`. La cámara conserva la capacidad de disparar el vistazo lateral. **Límite:** el láser `VL53L0X` tiene un haz estrecho (~25°), por lo que objetos laterales fuera de ese haz no disparan esquive con ToF fresco (**PENDIENTE** de validar en el carro; `pruebas_sim_ruta` presenta 1 fallo conocido en este caso).

| Láser (`solo_laser` activo) | Cámara | Qué pasa |
|---|---|---|
| ≤ 400 mm (`laser_cerca`) | cualquiera | Frena y confirma centro: manda el láser |
| libre o sin eco (ToF fresco) | color Y red | No frena en el centro ni bloquea reincorporación; puede disparar vistazo lateral |
| libre o sin eco (ToF fresco) | una sola fuente | No frena; a un lado, vistazo del láser |
| caído más de 600 ms en ruta | cualquiera | Causa 19: STOP y ayuda |

   Si discrepan, solo se anota en el registro (`CONTRADICCION_LASER_CAMARA`).
7. **Decisiones** (detalle en el subsistema E). Banda 2 o 3 en cualquier zona frena; la banda 1, solo en el centro con «frenar desde lejos». Parado, el carro elige lado, mide el ancho del obstáculo con el láser, pasa en paralelo y vuelve a la línea. Presupuestos: 6 esquivadas por obstáculo (`NAV_MAX_ESQUIVADAS`), 18 por ruta, 2 reintentos y 4,000 ms con ambos lados ocupados. Si no puede, pasa a `NAV_AYUDA`: queda quieto, el usuario maneja con la cruceta y a los 3 minutos termina en ERROR. Causas de la percepción: 5 (video perdido más de 700 ms), 16 (frente más ancho que el rodeo), 17 (red sin resultados 2.5 s), 18 (láser vio algo en el arco del giro) y 19 (láser caído). Seguridad entre capas: subsistema G.

## F.3 Cómo lo hicimos

- **Color Y red, no uno solo.** El color marca juntas de baldosas, sombras y reflejos; la red inventa cajas (un «refrigerator 43 %» sobre una pared lisa, `fusion.c`) y casi nunca acierta la clase a ras de piso (`docs/modelo_objetos.md`, sin tabla de datos). Pedir los dos, por zona y en 2 cuadros, filtra a ambos. Costo: comparten la imagen y pueden fallar juntos.
- **El láser confirma y no veta.** «Sin eco» también significa «más lejos de ~1.2 m», el haz es de 25° y el alcance depende de la reflectancia (`docs/investigacion_autonomo/08_fusion_percepcion.md`).
- **K = 3 en la app.** En J14, con piso real, la franja de referencia de 80 % tomaba el borde del propio carro y se descartaba 8 %; con 60 % y K = 3 quedó en 0 % (`ConfigVision.kt`).
- **Señal de bordes, descartada.** Ve una libreta del color del piso, pero en 5 escenas sin objetos da 115 celdas falsas contra 0 (`docs/obstaculos.md`).
- **TensorFlow Lite directo.** MediaPipe y Task Vision no dejan medir cada etapa, y el `.tflite` de MediaPipe (13.8 MB) no trae postproceso (`docs/bitacora.md`, Trabajo 8). Lite1 y Lite2 tardan 1.8 y 2.7 veces lo de Lite0; GPU y NNAPI fueron más lentas que la CPU (`docs/modelo_objetos.md`).
- **Origen y licencia del modelo.** Es el EfficientDet-Lite0 de los ejemplos oficiales de TensorFlow Lite, usado tal cual: el repositorio no tiene script ni datos de entrenamiento. Según `docs/modelo_objetos.md` se entrenó con COCO 2017 y su ficha pública lo da con Apache 2.0; `docs/bitacora.md` aclara que no se comprobó el archivo de licencia dentro del `.tflite`, y `docs/licencias/` no lo incluye (quién lo entrenó y la licencia Apache 2.0 en la fuente primaria quedan como **no verificado**).
- **Valores por defecto** (`UMBRAL_COLOR` 40, `MIN_CELDAS` 7, fila mínima 44 %): salen de un ajuste en vivo con el carro real (`docs/obstaculos.md`); fecha y versión de ese ajuste: no registrado por el equipo.
- **Geometría real de la cámara:** altura medida **9 cm del suelo** (el simulador usaba 11.5 cm y `corredor.h` 10 cm); inclinación y campo de visión: **NO MEDIDO**.

## F.4 Cómo lo probamos y con qué resultado

| Qué se probó | Cómo | Resultado | Etiqueta |
|---|---|---|---|
| Esquivar una caja de 20x20 cm | 6 corridas, 10-nov-2026 (`docs/estado_20261110.md`) | 6 de 6 sin tocarla; 5 terminadas; la 5 pidió AYUDA sin causa clara | **VALIDADO EN EL CARRO** (v1.1) |
| Rectas de 1.50 m sin obstáculo | 7 corridas, 15-nov-2026 (`docs/pruebas_fisicas_20261114.csv`, archivo local sin versionar) | 6 terminadas; 1 ERROR por video perdido; red con cajas en 14 a 54 % de los ticks, obstáculo decidido en 12 de 518 | **VALIDADO EN EL CARRO** (app v2) |
| Valores 40, 7 celdas, fila 44 % | Ajuste en vivo (`docs/obstaculos.md`) | 0 falsas alarmas en 270 muestras; mochila vista desde ~80 cm | **VALIDADO EN EL CARRO** (sin fecha) |
| Tiempos en el teléfono | Huawei ELS-NX9 (`docs/vision_huawei.md`, `docs/modelo_objetos.md`) | Color: 6.85 ms por cuadro. Red: mediana 44.7 ms | **SIMULADO** (sin carro) |
| Lógica en C (v3 + correcciones 26-nov) | `make -C nucleo_c test` | `pruebas_esquive_confirmado` (275, 0 fallos), `pruebas_reincorporacion_j15` (21, 0 fallos), `pruebas_precision_j16` (46, 0 fallos); **1 fallo conocido** en `pruebas_sim_ruta` (`61 casos, 1 fallos`: «las quince cajas obligan a esquivar»). Histórico 16-nov: 24 ejecutables, 1,901 casos, 0 fallos (**calculado con v3 antes de las correcciones del 26-nov, sin recalcular**) | **SIMULADO** |
| Autónomo v3 completo (histórico 16-nov) | Banco, 1,000 corridas por escenario (**calculado con v3 antes de las correcciones del 26-nov, sin recalcular**) | 8 escenarios en 1,000 de 1,000 ok; banquito 849 ok y 43 contactos; silla 435 y 21 | **SIMULADO** |
| Giro visual y flujo, con la cámara del carro | Solo pruebas en PC | Sin medición | **PENDIENTE** |

Lo que NO se ha medido:

- La inclinación y el campo de visión de la cámara (la altura real ya se midió en 9 cm), y con ellos el corredor.
- Si la red reconoce objetos en el piso del carro (la sección 8 de `docs/modelo_objetos.md` está vacía) y si el láser ve patas reales (M2, **NO MEDIDO**).
- Sol, sombras, reflejos y cambios de luz con el carro en marcha (bloques C y D, **PENDIENTE**), objeto con 0 % de eco IR (**SIMULADO**) y latencia de extremo a extremo (M9, **NO MEDIDO**).
- Sobre si en la prueba B6 (10-nov) estaba encendido el modelo de objetos: no registrado por el equipo.
- **Límite conocido de `aprenderPiso`:** rechaza el arranque con obstáculos a 40–80 cm (`descartadoPct > 5`, medido en 18 % > 5 % a 40 cm); el ajuste está pendiente y no se aplicó, por lo que las rutas deben arrancar con al menos 100 cm libres delante.

## F.5 Límites conocidos

- **Patas finas y objetos angostos.** Una celda mide 10x10 píxeles y hacen falta 7 por zona: con supuestos de 11.5 cm de altura, 12° y 56°, una pata de 30 mm ocupa 2 a 4 celdas a 50 cm y ninguna a 80 cm (`docs/manual/capitulos/08_vision.md`). El láser pasa entre patas (cono de ±89 mm a 400 mm, chasis de ±100 mm). Banco: banquito 849 ok y 43 contactos, silla 435 y 21 (**SIMULADO**); en el carro, **NO MEDIDO**.
- **Objetos oscuros, mates o transparentes.** Sin eco, el láser no frena ni da libre, y si solo una fuente de visión lo ve, tampoco frena. En el banco, un bloque sin eco terminó en contacto 40 de 40 veces (**SIMULADO**). La hoja del VL53L0X da 80 cm típicos para gris 17 %, menos con luz fuerte. En el carro: **NO MEDIDO**; primero hay que comprobar con el carro quieto si el láser da eco (`docs/estado_v3_pendiente_pruebas.md`).
- **Luz y piso que cambia.** El piso se aprende con la luz de ese momento y en la ruta queda fijo; la app no detecta sola un cambio de iluminación. Sol, sombra y reflejo hacen coincidir a color y red: paradas breves en 8 a 13 % de las corridas del banco (**SIMULADO**). El primer «Aprender piso» falló en 3 de 7 corridas del 15-nov (el límite es 5 % de descarte). Un objeto del tono del piso, o una pared clara contra piso claro, no se ve por color.
- **Retraso y causa común.** El video llega ~320 ms tarde (una sola medida, subsistema C) y la red tarda ~45 ms más. Color y red procesan la misma imagen: si fallan por la misma causa, pedir los dos no ayuda.
- **«Continuar» tras AYUDA sin mover el carro.** La app pide rodear con la cruceta y luego tocar «Continuar la ruta». Si se toca sin haber movido el carro ni quitado la causa, `navegador_ayuda_continuar` no comprueba que el obstáculo ya no esté: borra lo que el láser había confirmado, devuelve los presupuestos (6 esquivadas por obstáculo, 18 por ruta) y vuelve a la línea. Por lectura del código, lo que el láser o color y red sigan viendo lo detendrá otra vez; lo que solo veía una fuente, no. No hay tope global de continuaciones (hallazgo H-12, abierto en v3) ni prueba de ese ciclo (**NO MEDIDO**). En v1.1, la corrida 5 del 10-nov pidió AYUDA sin causa identificada y, tras la cruceta y «Continuar», venció el tope de 60 s.
- **Documentos viejos.** `docs/fusion.md` y `docs/flujo_piso.md` describen reglas anteriores; manda el código.

## F.6 Términos clave

- **celda**: cuadrito de 10x10 píxeles con el color promedio de sus píxeles; el cuadro tiene 24x17.
- **grupos de piso (K)**: colores del piso aprendidos con un k-means corto; hasta 4, 3 en la app.
- **`UMBRAL_COLOR` y croma**: distancia de color que separa piso de no piso (40) y regla de tono que tolera cambios de luz.
- **zona y cercanía**: tercio izquierdo, central o derecho; banda 1, 2 o 3 según qué tan abajo llega el obstáculo (no son centímetros).
- **`fila_minima` y `MIN_CELDAS`**: desde qué fila se mira (44 % del alto) y cuántas celdas «no piso» hacen falta (7).
- **EfficientDet-Lite0**: red neuronal de detección de objetos, entrenada con COCO; su resultado vale 300 ms (`vigencia_ms`).
- **fusión**: regla por zona `hay_obstaculo = color && red`; una fuente sola es un aviso.
- **autoridad del láser**: con `autoridad_laser = 1` y ToF fresco (`solo_laser`), solo `laser_cerca` confirma obstáculo en el centro; las reglas de visión pura en el centro y en reincorporación quedan inhibidas, aunque la cámara sigue pudiendo iniciar un vistazo lateral.
- **«sin eco»**: lectura inválida o fuera de alcance del láser; no cuenta como libre.
- **vistazo**: el carro tuerce 10 a 16° para apuntar el láser a lo que la cámara vio de lado; dos ecos a ≤ 650 mm confirman.
- **corredor**: huella del carro proyectada con perspectiva en la imagen; apagado por defecto.
- **AYUDA (`NAV_AYUDA`)**: el carro queda quieto y deja manejar al usuario; muestra una causa de 1 a 19.

## F.7 Preguntas probables del profesor

1. **¿Cómo sabe el carro qué es piso y qué es obstáculo?** No sabe qué es cada cosa. Aprende cómo se ve el piso libre (1 s con el carro quieto) y marca las celdas de 10x10 píxeles cuyo color ya no se parece. Si una zona junta 7 celdas hay un posible obstáculo; que lo sea lo decide la fusión.
2. **¿Qué es la red neuronal, quién la entrenó y con qué licencia?** Es EfficientDet-Lite0: recibe una imagen y devuelve cajas con clase y confianza. Nosotros no la entrenamos: es el archivo de los ejemplos oficiales de TensorFlow Lite, entrenado con COCO 2017. Su ficha pública menciona Apache 2.0, pero ni quien la entrenó exactamente ni la licencia en la fuente primaria están verificados en el repo (**no verificado**).
3. **¿Por qué piden color Y red, y qué pasa si el láser no está de acuerdo?** Cada uno se equivoca distinto: el color con juntas de baldosa y sombras, la red con cajas inventadas. Pedir los dos en la misma zona filtra a ambos, aunque con sol o sombra fallan juntos. Con las correcciones del 26-nov (`solo_laser`), cuando el láser tiene datos frescos, solo él confirma en el centro y las sospechas solo visuales no disparan esquive ni bloquean la reincorporación; la cámara sigue pudiendo pedir vistazo lateral.
4. **¿Cómo calculan la distancia con una sola cámara?** No la calculan en centímetros: usan qué tan abajo llega el obstáculo y lo reparten en tres bandas. El corredor sí convierte filas en centímetros, pero está apagado porque la altura, la inclinación y el campo de visión son valores de partida sin medir. La distancia medida la da el láser.
5. **¿Qué probaron en el carro?** En v1.1, el 10-nov-2026, esquivar una caja de 20x20 cm: 6 de 6 sin tocarla y 5 de 6 terminadas. El 15-nov, 7 rectas con la app v2: 6 terminadas y 1 con el video perdido. El autónomo v3 solo está simulado y falta validarlo; tampoco medimos patas reales, objetos oscuros ni cambios de luz.

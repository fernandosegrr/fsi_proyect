# Límites conocidos de la navegación

## Rama `feat/autonomo-v5-carbon` (23-nov-2026): lo que cambia y lo que sigue abierto

**SIMULADO / NO MEDIDO en el carro.** El carro real no se movió, no se instaló la APK ni se grabó firmware (solo se compilaron). `main`, el tag `v1.1` (`bd1727b`) y `feat/autonomo-v3` (`e37f77b`) no se tocaron. Detalle en [estado_20261123.md](estado_20261123.md), diseño en [autonomo_v5_diseno.md](autonomo_v5_diseno.md) y protocolo A/B en [protocolo_prueba_fisica_v5.md](protocolo_prueba_fisica_v5.md).

Resultados calibrados (`DESARROLLO` 24 escenarios y `RESERVADO` 27 escenarios × 40 corridas × laser normal/pesimista, velocidad real `25.7–29.4 cm/s`, **SIMULADO**, ver [etapa5.txt](precision/v5/etapa5.txt)):

- **Piso limpio (`G1: CUMPLE`):** `40/40 ok`, `0` contactos, `0` AYUDA y `0` esquives nuevos en las 4 matrices; paradas idénticas a `v3` en sol/sombra/reflejo. Con el interruptor **`Autónomo v5-carbon`** apagado (`v5=0` / `AV5_APAGADO=1`), la simulación reproduce **exactamente** a `v3` (`SI`).
- **Hueco sin eco (`G3: CUMPLE`) y objetos bajos:** `bloque_sin_eco` baja de **40/40** contactos en `v3` a **7/40** (`DESARROLLO`) y **9/40** (`RESERVADO`) en ambos láseres mediante clasificación de masa sólida no-piso de alto contraste/croma (`v5_solido`) y retención ante lavado AEC; `objeto_bajo` pesimista baja de `19/14` a `1/1` contactos y `bajo_solo_color` pesimista de `23/17` a `3/0`.
- **Muebles de patas (`G4: NO CUMPLE` en criterio estricto de AYUDA):** contactos bajan fuerte (`banquito35x45`: 15→4 / 25→6 `DESARROLLO` y 7→2 / 11→1 `RESERVADO`; `silla_patas`: 4→2 / 4→3 y 9→4 / 12→4; `mueble_con_pared`: 11→1 / 21→1), a cambio de más `AYUDA` segura al no girar sin espacio. Queda `1/40` `terminado_falso` en `silla_patas` de `RESERVADO` (`G6: NO CUMPLE`).
- **Pasillos y pared lateral (`G5: NO CUMPLE`):** `pasillo` mejora a `29/40` (normal) y `32/40` (pesimista) en `DESARROLLO` y `40/40` / `39/40` en `RESERVADO` con `0` contactos; `caja_y_pared_al_lado` mantiene `0` contactos en las 4 matrices pero iguala o excede por 1 la `AYUDA` en `DESARROLLO` (`16 vs 15` normal, `18 vs 18` pesimista).
- **Límites que siguen `NO MEDIDO`:** respuesta real del VL53L0X a patas y superficies oscuras/mate, exposición automática real del `GC0308`, distancia sensor–frente `e`, detección física de atasco (`Causa 20`) y recuperación I²C de `SEG=4`.

---

## Rama `feat/autonomo-v3` (16-nov-2026): lo que cambia y lo que sigue igual

**SIMULADO / NO MEDIDO en el carro.** El carro no se movió, no se instaló la APK y no se grabó firmware (solo se compilaron). `main` y el tag `v1.1` (`bd1727b`) no se tocaron. No es v1.2. Detalle en [estado_20261116.md](estado_20261116.md); hallazgos y su gravedad en [investigacion_autonomo/90_hallazgos.md](investigacion_autonomo/90_hallazgos.md).

Las columnas del banco ahora son **excluyentes y suman las corridas**: ok, contacto, bajo_mueble (pasó entre las patas sin tocar), ayuda, error, sin_terminar, falso_fin, esquive_falso, otro (en ese orden de prioridad). La tabla de abajo (v2) usa las columnas viejas, que se traslapaban.

| Caso (1000 corridas, SIMULADO) | v2 | v3 | Qué cambió |
|---|---|---|---|
| Banquito 35 × 45 con patas | 765 ok, 79 contactos, 112 por debajo, 44 ayuda | **849 ok, 43 contactos**, 75 por debajo, 33 ayuda | Memoria del eco y retroceso antes de girar |
| Silla con patas | 316 ok, 34 contactos, 618 por debajo, 32 ayuda | **435 ok, 21 contactos**, 529 por debajo, 15 ayuda | Igual |
| Caja corrida de lado | 998 ok, 2 contactos | 999 ok, 0 contactos, 1 ayuda | Igual |
| Objeto bajo (color y red) / solo color | 997 ok, 1 contacto / 986 ok, 14 contactos | 1000 ok / 1000 ok | Igual |
| Caja con pared a un lado | 615 ok, 2 contactos, 383 ayuda | 613 ok, 1 contacto, 386 ayuda | Sin cambio real (una ayuda por contacto) |
| Manchas de sol, sombras, reflejos | 8–13 % de corridas con parada momentánea | igual | La regla aprobada (color y red coinciden) |
| Error lateral final tras esquivar (Monte Carlo heredado) | 18/44 mm | 18/44 mm | v1.1 da 17/40. Un margen de paso de 180 mm iguala pero mete 2 contactos con el objeto largo: no se aplica |

**Compuertas del encargo: siguen en rojo.** Muebles con patas (contactos y ayudas), la caja con pared a un lado y las paradas con sol, sombra y reflejo. Respecto a v2, dos cosas no mejoran: las ayudas totales de banquito (59 contra 58) y silla (73 contra 64), y una ayuda de más en la caja corrida de lado. Las ayudas «finales» (el carro termina pidiendo ayuda) sí bajan, y los contactos se reducen a la mitad. Lo que quedó: la ayuda con causas 16 y 18 es la respuesta cuando el giro no cabe entre las patas (no hay sensor trasero y el retroceso se limita a 25 cm).

**Modelo del láser.** El banco supone que el láser lee «el grupo más cercano dentro del cono»; la documentación del sensor dice que promedia lo que cae en su cono (media confianza). Con el modelo alterno (`AV3_LASER_MEZCLA=1`, SUPUESTO) los contactos son iguales y las ayudas suben (banquito 23 y silla 14 contra 7 y 4 en 300 corridas). Ninguna cifra de muebles se debe leer como del carro: **NO MEDIDO** si el láser ve patas reales.

**Lo nuevo en v3:** memoria del eco y retroceso antes de girar; causa 19 (láser caído a mitad de ruta); giros de menos de 10° por tiempo no se hacen sin giroscopio sano (umbral SUPUESTO); firmware `SEG=3` (freno 150 mm, bloqueo 180 mm, un «sin eco» aislado no rompe la racha de «cerca»; **solo compilado**); `verificar_version.py --solo-fw`.

**Sigue sin hacerse (diseño):** leer el estado de la medición del VL53L0X (el firmware solo ve milímetros; 8190 y 65535 son «libre»), recuperación del bus I²C, tope global de continuaciones tras ayuda (cada «continuar» devuelve los presupuestos), detección de atasco, bloqueo de exposición (el controlador y la hoja de datos del GC0308 discrepan en los bits de AEC y AGC), frescura de SENS medida desde la medición y no desde su llegada, pines del TC1508A al reiniciar (`seguridad.md` §3.2 dice que el reinicio los lleva «a nivel bajo por hardware»; la hoja del ESP32 indica que los GPIO 25, 26, 27 y 33 arrancan sin salida ni entrada habilitada, es decir flotantes, así que depende del driver o de un pull-down externo: NO MEDIDO).

**Mediciones baratas que cierran lo anterior:** bloque M (M1 a M10) del [protocolo](protocolo_prueba_fisica_autonomo_v2.md#10-bloque-m--mediciones-baratas-v3) y su [plantilla CSV](pruebas_fisicas_mediciones_v3_plantilla.csv).

---

# (Base) Límites conocidos de v2, todavía vigentes salvo lo indicado arriba

Rama `feat/autonomo-v2`, 14-nov-2026 (con las correcciones de la revisión independiente), sobre v1.1 (`bd1727b`) y `6bea71e`. **SIMULADO / NO MEDIDO en el carro.** No es v1.2. El carro no se movió, no se instaló la APK y no se grabó firmware. [Diseño y resultados](autonomo_v2_diseno.md) · [auditoría](auditoria_autonomo_v2.md) · [protocolo de prueba física](protocolo_prueba_fisica_autonomo_v2.md) · [diagnóstico anterior](diagnostico_esquive_20261111.md).

## Lo que el simulador ya dice que no se cumple

| Caso | Resultado SIMULADO (1000 corridas) | Por qué |
|---|---|---|
| Banquito 35 × 45 con patas | 765 ok, 79 contactos con una pata, 160 pasa por debajo, 58 ayuda | El láser pasa entre las patas, la red las da a ratos, el color las ve tarde y delgadas |
| Silla con patas | 316 ok, 34 contactos, 644 pasa por debajo, 64 ayuda | Igual. Tiene más contactos que la rama de partida (12), que casi siempre pasaba por debajo sin enterarse |
| Caja con una pared pegada a un lado (25–44 cm de la línea) | 615 ok, 2 contactos, 385 ayuda | La cámara no distingue una pared clara de un piso claro: el carro elige ese lado ~38 % de las veces, el láser la descubre al girar y pide ayuda (causas 18 y 16). La rama de partida tocaba la pared 216 veces |
| Caja corrida de lado | 998 ok, 2 contactos | El vistazo no confirma a tiempo |
| Objeto bajo (lo ven color y red) | 997 ok, 1 contacto, 2 ayuda | El láser no lo ve; depende de la coincidencia |
| Objeto bajo que solo ve el color | 986 ok, 14 contactos | Una sola fuente sin eco no se distingue de una mancha |
| Manchas de sol, sombras, reflejos | 0 esquives, 0 ayudas; el carro se detiene un momento en 8–13 % de las corridas | Color y red coinciden: la regla aprobada lo trata como obstáculo |
| Error lateral final tras un esquive (Monte Carlo heredado) | 17/40 → 18/44 mm (media/p95) | El paso junto al objeto es ~30 cm más largo |

Dos obstáculos seguidos y una caja 40–60 cm antes de una esquina quedan en 1000 de 1000 sin contactos.

El modelo de mueble del banco (patas de 25–35 mm, asiento por encima del haz del láser) es una suposición: **NO MEDIDO**.

## Fusión y cobertura

El láser fresco y cercano (≤ 400 mm) confirma. Una sola fuente de visión, con el láser libre **o sin eco**, no detiene ni hace avanzar a ratos (bandera `navegador_poner_precaucion` para volver a lo anterior). Color y red en la misma zona confirman desde el segundo cuadro seguido (en el centro y en la banda más cercana, desde el primero); para esquivar sin láser hacen falta tres cuadros con el carro detenido (cuatro y 0.8 s si lo detuvo un solo cuadro).

Lo que solo ve el color a un lado se mira con el láser torciendo el rumbo 10–16° en marcha (vistazo); un eco válido a ≤ 650 mm confirma. Hace falta `RUMBO=1`. El vistazo no alcanza una pata delgada a más de ~50 cm ni algo más bajo que el haz (55 mm de altura, SUPUESTO).

El haz es estrecho y las dos fuentes de cámara comparten la misma vista: pueden fallar juntas. Las bandas 1/2/3 son proximidad de imagen, no centímetros. Si la ruta cuenta con el detector de objetos y este deja de dar resultados 2.5 s, el carro se detiene (causa 17); si el detector está apagado desde el principio, la ruta sale con un aviso y solo el láser confirma.

## Rodeo

El desvío lateral sale del ancho que mide el láser en el primer giro (ancho + 240 mm, entre 400 y 800). Si no se puede comprobar dónde termina el obstáculo (una pared deja de dar eco viéndose), el lado se toma por cerrado, se mide el otro y, si también lo está, se pide ayuda (causa 16). Un objeto de más de ~55 cm de frente no se rodea.

Medir el otro lado es dar la vuelta completa sobre la misma rueda, y eso barre hasta ~44 cm de la línea (derecha) o ~32 (izquierda) y otro tanto hacia atrás. Solo se da si lo que midió el láser queda fuera de ese disco, si la cámara veía libre el otro lado y si el carro venía de avanzar en recta; si no, pide ayuda (causa 16) sin girar más. Durante el primer giro y durante esa vuelta, si el láser ve algo dentro del arco (a menos de ~15 cm girando a la derecha, ~12 a la izquierda, o una pared paralela a menos de ~31 / ~24 cm), el carro deja de girar y pide ayuda (**causa 18**). Con una pared a menos de ~27 cm del lado hacia el que gira puede alcanzar a tocarla antes de detenerse.

El paso en paralelo avanza una pista calculada con el campo de la cámara (56°, SUPUESTO) después del último punto en que el costado pudo verse: una caja de 20 cm se deja unos 60 cm atrás antes de volver. Si durante el paso asoma otro objeto, la pista se alarga suponiéndole 20 cm de fondo. El paso no sigue más allá de una esquina: llega hasta el vértice y, si no queda recta, se pide ayuda (causa 14). Con el destino a menos de ~1.3 m del obstáculo el carro puede volver a la línea justo al final, o terminar más allá del destino dibujado (la última recta se prolonga hasta 2 m).

Antes de entrar a la línea el carro mira; no ve lo que queda pegado a su costado (a menos de ~19 cm de la lente y a más de 28° del eje). Lo cubre una segunda mirada 20 cm más adelante cuando la anterior vio algo.

**No hay sensor trasero.** Las reversas (150–250 mm tras un paro o un vistazo; hasta 450 mm para deshacer una diagonal) recorren solo camino que el carro acaba de pisar y suponen que nadie entró detrás.

## Cierre y sensores

«Terminado» sin rodeo abierto se acepta al llegar; lo que el láser vea más allá del destino no lo impide. Con un rodeo abierto hacen falta el avance cumplido y, parado, tres lecturas nuevas a más de 400 mm o, sin eco, cinco cuadros sin coincidencia (con la red vigente). **Sin eco no cuenta como libre.**

Siguen: pérdida de video a 700 ms, silencio de SENS a 1.5 s, watchdog del firmware a 1000 ms y `TOF_PARO` a 120 mm (v1.1 y `SEG=2`; 150 mm con `SEG=3`, solo compilado). STOP no elimina la latencia ni la inercia.

Firmware: hay cambios **solo compilados** (carro `SEG=2`, cámara `cam-v2`). La app nueva funciona con los firmwares actuales.

## Códigos de detención y AYUDA

La pantalla muestra «Causa N: texto». Con AYUDA desactivada se comunica ERROR; la pérdida de video siempre es ERROR.

| Código | Significado y condición |
|---:|---|
| 0 | Sin causa de error. |
| 1 | Ambos lados ocupados al menos 4000 ms y sin retroceso conocido disponible. |
| 2 | Agotadas las seis maniobras por obstáculo (o 18 en la ruta), o la extensión de la ruta. |
| 3 | Agotados los dos reintentos locales de salida, con el frente aún bloqueado. |
| 4 | Planificador sin ruta válida. |
| 5 | Pérdida de video o sin cuadro para elegir lado tras un paro. ERROR y STOP. |
| 6 | No cabe el arco de salida hacia ningún lado libre y no queda retroceso conocido. |
| 7 | Agotadas cuatro correcciones de reincorporación por error de pose. |
| 8 | Sin retorno geométrico viable con los giros de pivote. |
| 9 | Giro planeado de más de 150° que no está en la ruta: freno de cordura. |
| 10 | El índice de ruta retrocede: freno de cordura. |
| 11 | Recorrido mayor que 2.5 veces la ruta. |
| 12 | Pose a más de 1 m de la ruta. |
| 13 | Histórico, reservado. |
| 14 | El rebase no cabe en la recta ni en su extensión permitida. |
| 15 | Histórico de cierre no libre. |
| 16 | **V2.** Pared u objeto muy ancho: el láser midió los lados al girar y no hay por dónde (o medir el otro lado no era seguro). El carro no se acercó. |
| 17 | **V2.** El detector de objetos dejó de dar resultados vigentes durante 2.5 s. |
| 18 | **V2.** El láser vio algo dentro del arco que barre el chasis al girar para rodear (una pared pegada, un objeto junto al carril): el carro dejó de girar. |
| 19 | **V3.** El láser dejó de responder a mitad de la ruta (vivía al empezar y lleva 600 ms sin vivir): STOP y ayuda. Con el láser aún muerto, continuar vuelve a pedir ayuda. |
| 20 | **V5.** Atasco o bucle: orden `FORWARD` sostenida >= 1.8 s con al menos 5 lecturas láser frescas y válidas (< 850 mm) estancadas en ±20 mm, o 3 intentos de «Continuar» tras `AYUDA` sin avanzar 20 cm (`NAV_ERROR` al 3.º). **SIMULADO / NO MEDIDO**. |

## Piso y precisión

La app exige «Aprender piso» en cada ruta. No detecta sola un cambio de iluminación; la caja negra guarda los números para verlo después (líneas `VTEL`, y `foto` de la cámara con `cam-v2`). Valores de color: 40, 7 celdas, fila mínima 44 %. La pantalla de Visión deja poner el umbral hasta 300 y las celdas hasta 40 sin avisar: con 240/23 el detector de color queda prácticamente ciego.

No hay encoders: la distancia se estima por tiempo. El pivote es **asimétrico** (derecho: radio ~15.8 cm; izquierdo: ~8.7–9 cm, según la calibración y la investigación de cinemática): el «espejo» del giro es incorrecto; el izquierdo sigue siendo poco medido. La velocidad de 19.3 cm/s en AUTO puede estar obsoleta (las cajas negras del 15-nov sugieren ~30 cm/s). Los 120 mm sensor–centro, el campo y la altura de la cámara son supuestos.

## Pendiente de medición física

Todo. Orden propuesto en el [protocolo](protocolo_prueba_fisica_autonomo_v2.md): rectas sin obstáculos, caja de 20 × 20, caja corrida, banquito, objeto largo, caja con pared a un lado, falsos positivos provocados, cortes y rutas con giros (con una caja antes de la esquina). Los datos del [10-nov](estado_20261110.md) y el [CSV físico](pruebas_fisicas_20261110.csv) son históricos y no validan esta rama.

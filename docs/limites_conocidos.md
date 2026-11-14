# Límites conocidos de la navegación

Rama `feat/autonomo-v2`, 13-nov-2026, sobre v1.1 (`bd1727b`) y `6bea71e`. **SIMULADO / NO MEDIDO en el carro.** No es v1.2. El carro no se movió, no se instaló la APK y no se grabó firmware. [Diseño y resultados](autonomo_v2_diseno.md) · [auditoría](auditoria_autonomo_v2.md) · [protocolo de prueba física](protocolo_prueba_fisica_autonomo_v2.md) · [diagnóstico anterior](diagnostico_esquive_20261111.md).

## Lo que el simulador ya dice que no se cumple

| Caso | Resultado SIMULADO (1000 corridas) | Por qué |
|---|---|---|
| Banquito 35 × 45 con patas | 774 ok, 81 contactos con una pata, 162 pasa por debajo, 36 ayuda | El láser pasa entre las patas, la red las da a ratos, el color las ve tarde y delgadas |
| Silla con patas | 318 ok, 40 contactos, 644 pasa por debajo, 39 ayuda | Igual. Tiene más contactos que la rama de partida (12), que casi siempre pasaba por debajo sin enterarse |
| Dos obstáculos seguidos | 994 ok, 6 contactos | Entrar a la línea junto al segundo objeto |
| Objeto bajo (lo ven color y red) | 989 ok, 5 contactos, 6 ayuda | El láser no lo ve; depende de la coincidencia |
| Objeto bajo que solo ve el color | 914 ok, 56 contactos | Una sola fuente sin eco no se distingue de una mancha. La rama de partida tenía 18: avanzaba a ratos, con 4–5 paradas por corrida ante cualquier mancha |
| Manchas de sol, sombras, reflejos | 0 esquives, 0 ayudas; el carro se detiene un momento en 7–12 % de las corridas | Color y red coinciden dos cuadros seguidos: la regla aprobada lo trata como obstáculo |
| Error lateral final tras un esquive (Monte Carlo heredado) | 17/40 → 18/44 mm (media/p95) | El paso junto al objeto es ~30 cm más largo |

El modelo de mueble del banco (patas de 25–35 mm, asiento por encima del haz del láser) es una suposición: **NO MEDIDO**.

## Fusión y cobertura

El láser fresco y cercano (≤ 400 mm) confirma. Una sola fuente de visión, con el láser libre **o sin eco**, no detiene ni hace avanzar a ratos (bandera `navegador_poner_precaucion` para volver a lo anterior). Color y red en la misma zona confirman desde el segundo cuadro seguido; para esquivar sin láser hacen falta tres cuadros con el carro detenido.

Lo que solo ve el color a un lado se mira con el láser torciendo el rumbo 10–16° en marcha (vistazo); un eco válido a ≤ 650 mm confirma. Hace falta `RUMBO=1`. El vistazo no alcanza una pata delgada a más de ~50 cm ni algo más bajo que el haz (55 mm de altura, SUPUESTO).

El haz es estrecho y las dos fuentes de cámara comparten la misma vista: pueden fallar juntas. Las bandas 1/2/3 son proximidad de imagen, no centímetros. Si la ruta cuenta con el detector de objetos y este deja de dar resultados 2.5 s, el carro se detiene (causa 17); si el detector está apagado desde el principio, la ruta sale con un aviso y solo el láser confirma.

## Rodeo

El desvío lateral sale del ancho que mide el láser en el primer giro (ancho + 240 mm, entre 400 y 800). Si no se puede comprobar dónde termina el obstáculo (una pared deja de dar eco viéndose), el lado se toma por cerrado, se mide el otro y, si también lo está, se pide ayuda (causa 16). Un objeto de más de ~55 cm de frente no se rodea.

El paso en paralelo avanza una pista calculada con el campo de la cámara (56°, SUPUESTO) después del último punto en que el costado pudo verse: una caja de 20 cm se deja unos 60 cm atrás antes de volver. Con el destino a menos de ~1.3 m del obstáculo el carro puede volver a la línea justo al final, o terminar más allá del destino dibujado (la última recta se prolonga hasta 2 m).

Antes de entrar a la línea el carro mira; no ve lo que queda pegado a su costado (a menos de ~19 cm de la lente y a más de 28° del eje). Lo cubre una segunda mirada 20 cm más adelante cuando la anterior vio algo.

**No hay sensor trasero.** Las reversas (150–250 mm tras un paro o un vistazo; hasta 450 mm para deshacer una diagonal) recorren solo camino que el carro acaba de pisar y suponen que nadie entró detrás.

## Cierre y sensores

«Terminado» sin rodeo abierto se acepta al llegar; lo que el láser vea más allá del destino no lo impide. Con un rodeo abierto hacen falta el avance cumplido y, parado, tres lecturas nuevas a más de 400 mm o, sin eco, cinco cuadros sin coincidencia (con la red vigente). **Sin eco no cuenta como libre.**

Siguen: pérdida de video a 700 ms, silencio de SENS a 1.5 s, watchdog del firmware a 1000 ms y `TOF_PARO` a 120 mm. STOP no elimina la latencia ni la inercia.

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
| 16 | **V2.** Pared u objeto muy ancho: el láser midió los dos lados al girar y no hay por dónde. El carro no se acercó. |
| 17 | **V2.** El detector de objetos dejó de dar resultados vigentes durante 2.5 s. |

## Piso y precisión

La app exige «Aprender piso» en cada ruta. No detecta sola un cambio de iluminación; la caja negra guarda los números para verlo después (líneas `VTEL`, y `foto` de la cámara con `cam-v2`). Valores de color: 40, 7 celdas, fila mínima 44 %. La pantalla de Visión deja poner el umbral hasta 300 y las celdas hasta 40 sin avisar: con 240/23 el detector de color queda prácticamente ciego.

No hay encoders: la distancia se estima por tiempo. El pivote izquierdo es el espejo del derecho, no una medida propia. Los 120 mm sensor–centro, el campo y la altura de la cámara son supuestos.

## Pendiente de medición física

Todo. Orden propuesto en el [protocolo](protocolo_prueba_fisica_autonomo_v2.md): rectas sin obstáculos, caja de 20 × 20, caja corrida, banquito, objeto largo, falsos positivos provocados, cortes y rutas con giros. Los datos del [10-nov](estado_20261110.md) y el [CSV físico](pruebas_fisicas_20261110.csv) son históricos y no validan esta rama.

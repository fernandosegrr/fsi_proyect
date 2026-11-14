# Autónomo v2 — diseño, decisiones y límites

Rama `feat/autonomo-v2`, 13-nov-2026 (corregida el 14-nov tras una revisión independiente, sección 2.9), sobre v1.1 (`bd1727b`) y la rama de partida (`6bea71e`). **No es v1.2.**

**Todo lo que aquí se dice que «mejora» está SIMULADO: salió del banco `nucleo_c/pruebas_autonomo_v2.c`, no del carro. Nada de este documento está VALIDADO EN EL CARRO.** El carro no se movió, no se instaló la APK y no se grabó ningún firmware. La auditoría que motivó cada decisión está en [auditoria_autonomo_v2.md](auditoria_autonomo_v2.md); los números completos, en [precision/autonomo_v2_comparacion.txt](precision/autonomo_v2_comparacion.txt).

## 1. Qué quedó hecho, qué quedó a medias y qué quedó en papel

| Idea del encargo | Estado | Dónde |
|---|---|---|
| I. Caja negra por corrida | **Hecha** (compilada, sin probar en el teléfono) | `CajaNegra.kt`, `navegador_caja_negra`, `vision_tele.c` |
| G. Control de movimiento | **Parcial**: giros chicos sin tiempo, rumbo parado, carril del rodeo, salto de objetivo acotado. Frenado suave y velocidad adaptativa: diseño | `navegador.c` |
| C. Barrido con giroscopio y láser | **Hecha** en el primer giro del rodeo y en el regreso; el barrido de «pocos grados» sin rodear: diseño | `navegador.c` (rodeo medido, mirada) |
| F. Recuperación escalonada antes de AYUDA | **Hecha**: cambio de lado, paso extra, deshacer en reversa, retroceso con evidencia | `navegador.c` |
| D. Fusión por zona | **Parcial**: consenso temporal de la coincidencia y vistazo del láser. Fusión probabilística completa: diseño | `navegador.c` |
| V1. Estabilidad fotométrica | **Parcial**: telemetría para medirla (cámara y teléfono). Bloqueo de exposición: diseño (sin hoja de datos del GC0308) | `camara_prueba.ino`, `vision_tele.c` |
| V6. Sincronía y latencia | **Parcial**: rumbo y láser del mismo SENS en el barrido; sello por cuadro en la cámara: diseño | `navegador.c` |
| H. Seguridad y degradación | **Parcial**: detector caído, percepción incompleta al iniciar, firmware SEG=2. Modo prudente sin video: diseño | `navegador.c`, `carro.c`, app |
| V4. Detector neuronal | **Parcial**: confirmación temporal. Filtro de clases y geometría: diseño | `navegador.c` |
| V2. Aprendizaje continuo del piso | Diseño | — |
| V3. Robustez del clasificador | Diseño (el banco no mostró a las juntas como causa) | — |
| V5. Calibración fila–distancia con el láser | Diseño | — |
| A. Odometría fusionada | Diseño | — |
| B. Acelerómetro | Diseño (extensión de protocolo definida) | — |
| E. Mapa local y planificador | Diseño (trabajo futuro) | — |

## 2. Lo que se implementó

### 2.1 Rodeo medido (ideas C, F, G)

El carro gira sobre un costado, no sobre su centro: con las medidas de J14 (90° a la derecha = 150 mm adelante y 165 mm a la derecha) el pivote derecho queda a 157 mm del centro y el izquierdo a 90 mm. La esquina más lejana del chasis (200 × 240 mm) barre hasta 285 mm (derecha) o 225 mm (izquierda). Casi todos los contactos del banco con la rama de partida salían de ahí: un giro hecho junto a algo que no se había visto.

**Barrido del primer giro.** Mientras el firmware hace el primer giro del rodeo, cada `SENS` trae el ángulo del giroscopio y la distancia del láser. Con eso se sabe dónde pega el haz respecto de la línea de la ruta (se usa el ángulo de medio periodo antes, porque la distancia es la mediana de las últimas tres lecturas). De las lecturas sale:

- el **ancho** del obstáculo hacia ese lado: lo que se ve seguido desde el frente, sin saltos de más de 250 mm (`NAV_ESCANEO_SALTO_MM`). El desvío lateral es ancho + 40 + 200 mm (`NAV_ESCANEO_RELLENO_MM`, `NAV_ESQ_HOLGURA_VOLVER_MM`), nunca menos de 400 ni más de 800;
- si hay una **pared de ese lado** (ecos a más de 250 mm de lado, o a más de 100 mm si caen antes de la cara y junto al carro: una pared pegada al carril): el desvío no pasa de pared − 260 mm;
- si el final de la cara **no se pudo comprobar**: cuando deja de haber eco se calcula a qué distancia estaría la cara con ese ángulo más 8°; si pasa de 700 mm (`NAV_ESCANEO_ALCANCE_UTIL_MM`, `NAV_ESCANEO_HOLGURA_GRADOS`) el láser no la habría visto de todos modos, así que el lado se toma por **cerrado**.

**Cambio de lado** (propuesto por el dueño del proyecto durante el trabajo). Con un lado cerrado, el carro sigue girando en el mismo sentido —media vuelta y un cuarto más, sobre la misma rueda, sin acercarse— y el último cuarto barre el otro lado. Si está libre, rodea por ahí. Si los dos están cerrados pide ayuda con la **causa 16** (frente ancho), de frente y a la distancia a la que frenó. Esa vuelta barre un disco alrededor de la rueda (hasta ~44 cm de la línea a la derecha, ~32 a la izquierda, y otro tanto hacia atrás), así que **solo se da** si ningún eco del barrido cae dentro del disco ni hay nada junto al carro más cerca que su alcance, si la cámara veía libre el otro lado cuando el carro se detuvo y si el carro venía de avanzar en recta (lo de atrás, sobre la línea, lo acaba de pisar). Si no, pide ayuda sin girar más.

**Guardia del giro.** Durante el primer giro del rodeo y durante esa vuelta, el haz del láser va unos 30° por delante de la esquina que barre. Una lectura válida más corta que ~15 cm (girando a la derecha) o ~12 cm (a la izquierda) es algo dentro del arco; dos lecturas seguidas a la misma distancia lateral y cada vez más cerca son una pared paralela a la ruta, y si queda a menos de ~31 cm (derecha) o ~24 cm (izquierda) el frente la alcanzaría antes de terminar el giro. En los dos casos el carro deja de girar y pide ayuda con la **causa 18**. No lo evita todo: con ~0.3 s de ventaja el chasis puede alcanzar a tocar, pero deja de empujar.

**Paso en paralelo.** A `c` mm de lado, la cámara (56° de campo, SUPUESTO) solo ve el piso desde `c / tan(28°)` = 1.88 `c` por delante de la lente. El costado del objeto queda fuera del láser y casi fuera de la cámara. Por eso se avanza una **pista** = 1.88 `c` + 100 + 230 mm (`NAV_PASO_VISTA_MARGEN_MM`) desde el último punto en que el costado pudo estar a la vista; si la cámara lo sigue viendo en la orilla lejana de ese lado, la cuenta vuelve a empezar. Si el paso llevaba 250 mm sin ver nada de ese lado y algo asoma, es **otro objeto** más adelante: de ese no se midió el ancho ni se sabe el fondo, y la pista se alarga (como si empezara en la misma línea, más 200 mm de fondo supuesto). Tope: 2500 mm. El paso y sus alargamientos **no pasan del final de la recta**: junto a una esquina llegan hasta el vértice y, si ya no queda recta, se pide ayuda (causa 14). Durante el paso el firmware sostiene el rumbo **de la línea** (no el rumbo con que el carro salió, que trae 2 o 3 grados de error) y apunta a un punto del carril 400 mm adelante: sin eso, en metro y medio el carro se cerraba 8 cm contra lo que rodeaba.

**Regreso con mirada.** El regreso es en diagonal. Después del primer giro el carro queda mirando hacia donde va a entrar a la línea y espera dos cuadros ya estables (tope 600 ms después de la latencia del video):

- si el color ve algo cerca (cercanía 2 o 3, 6 celdas o más) en cualquier zona, o algo más lejos en la zona hacia donde sigue la ruta, o el láser tiene eco a 400 mm o menos, **no entra**: vuelve al carril, avanza 400 mm (750 si lo visto está más adelante) y mira otra vez;
- **punto ciego**: girado hacia la línea, la cámara no ve lo que queda pegado al costado del carro (a menos de ~19 cm de la lente y a más de 28° del eje). Si la mirada anterior vio algo delante y esta ya no, el final de ese algo puede estar justo ahí: se avanza 200 mm y se mira de nuevo; solo la segunda mirada despejada seguida deja entrar;
- si la entrada se ocupa **a medio camino**, el carro deshace la diagonal en reversa (por donde acaba de pasar) en vez de girar junto a lo que apareció.

Junto al final de la ruta (menos de 400 mm de recta después de la diagonal) se usa un cruce casi transversal, que entra antes a la línea y deja recta para afinar.

**Cierre.** «Terminado» sin rodeo abierto se acepta al llegar. Con un rodeo abierto hacen falta el avance cumplido y, con el carro parado, tres lecturas nuevas del láser a más de 400 mm (`NAV_FIN_HOLGURA_MM`; antes 800, que hacía «esquivar» una pared a 60 cm que no estorbaba) o, sin eco, cinco cuadros sin coincidencia. **El láser sin eco sigue sin contar como libre.** Lo que el láser ve más allá del destino con 400 mm de holgura no detiene. El presupuesto de esquivadas es por obstáculo (6) y por ruta (18). Si la última recta se prolongó y el carro volvió antes del destino dibujado, la prolongación se quita.

### 2.2 Vistazo con el láser (ideas D, C)

El cono del láser (~25°) no alcanza una caja corrida de lado que todavía estorba al chasis, ni las patas de un mueble. El color sí las ve, pero una sola fuente de visión no detiene al carro. Cuando algo sin confirmar lleva 3 cuadros en un lado (6 celdas, o una raya vertical de 3: una pata) y su orilla de adentro queda a 190 mm o menos del eje, el carro **tuerce el rumbo** hacia ahí —entre 10° y 16°, con el control de rumbo del firmware, sin frenar—, deja mirar al láser y vuelve. Dos ecos válidos a 650 mm o menos **confirman** (es la regla aprobada: láser cercano y válido confirma). Entonces se detiene, retrocede hasta 250 mm por donde vino y rodea por el lado contrario. Sin eco sigue su ruta.

Costo medido en el banco: unos 3 cm de desvío lateral que se recupera solo, sin paradas. Un vistazo por cosa vista (`NAV_VISTAZO_LEJOS`): los de cerca no confirmaron más y sí acercaron el carro a las patas de una silla. Hace falta `RUMBO=1`; sin él no hay vistazo.

### 2.3 Coincidencia de color y red con consenso temporal (ideas D, V4)

La regla aprobada no cambia: color **y** detector en la misma zona confirman aun con el láser libre o sin eco. Lo que cambia es cuándo:

- cuenta desde el **segundo cuadro seguido** (`NAV_COINCIDENCIA_CUADROS_FRENAR`), salvo en la zona **central** con cercanía 3, donde cuenta desde el primero: ahí cada cuadro de espera son hasta 6 cm hacia algo que el láser, que mira justo ahí, no ve. Mientras el carro rodea o vuelve, siempre desde el primero;
- para **esquivar** sin láser que lo confirme hacen falta 3 cuadros con coincidencia desde que el carro se detuvo; si lo detuvo un solo cuadro, 4 y además 0.8 s detenido;
- si el rodeo ya estaba decidido y el carro solo retrocedió para que cupiera el giro, **rodea** al terminar de retroceder mientras alguna fuente de visión siga viendo algo. Antes volvía a pedir la confirmación: con un objeto bajo, a 30 cm el láser ya no lo ve y la red lo da a ratos, así que el carro avanzaba otra vez hacia él.

Una caja inventada por la red sobre una mancha de color suele durar un cuadro. Con esto los falsos positivos del banco pasaron de 4–14 esquives por cada 1000 corridas a 0.

**Avanzar a ratos** (700 ms de avance, 300 de pausa por una sola fuente sin eco) queda **apagado por defecto**: `navegador_poner_precaucion(NAV_PRECAUCION_NUNCA)`. En el banco producía de 4 a 5 paradas por corrida con manchas de sol y no cambiaba el resultado del objeto bajo que ven las dos fuentes. Ver el riesgo en la sección 5.

### 2.4 Pose y giros (idea G)

- Con el giroscopio sano, un giro menor que el mínimo del firmware (5°) **no se hace por tiempo**: en el banco con giros por tiempo realistas, pedir 4° daba ~18°. Lo absorbe el control de rumbo al avanzar.
- **Parado después de un avance, el rumbo sigue al giroscopio.** Antes se quedaba con la última lectura recibida avanzando (hasta 150 ms vieja) y lo que el carro giraba al frenar se perdía.
- El objetivo de rumbo que se manda nunca se aleja más de 20° del rumbo actual: con más de 30° de error el firmware deja de corregir (lo toma por un choque) y el carro seguía torcido hasta el final del avance.
- Si un giro por ángulo falla, se reintenta hasta 3 veces en la ruta antes de dejar de pedirlos (antes: uno).

### 2.5 Salud de la percepción (idea H)

- `navegador_aporte_modelo`: si la ruta cuenta con el detector de objetos y pasan 2.5 s sin un resultado vigente, el carro se detiene y pide ayuda con la **causa 17**. Mientras la red no está vigente, los cuadros no cuentan como «sin coincidencia» para cerrar un rodeo.
- Al iniciar, la app dice si la ruta sale sin láser, sin detector o sin cámara.
- La ayuda y los errores muestran «Causa N: …».

### 2.6 Firmware del carro: `SEG=2`

Compatible con la app v1.1. `SENS` no cambia (la app exige exactamente 3 campos).

- Un texto desconocido o vacío **no renueva el watchdog ni queda como comando actual**. Antes sí: un cliente que probara un comando nuevo dejaba el carro avanzando sin latido, sin freno láser y sin control de rumbo (hallazgo crítico F-C1).
- `FORWARD` en perfil AUTO con el frente confirmado a menos de 150 mm (`TOF_BLOQUEO_MM`) no arranca: contesta `TOF_PARO:<mm>`. Retroceder, girar y el perfil MANUAL no se bloquean. `TOF_PARO` sigue en 120 mm.
- El freno láser se rearma en cada avance nuevo.
- `CAPS` agrega `SEG=2,TUSO=<0|1>` antes de `FW:`.

### 2.7 Firmware de la cámara: `cam-v2`

`/stats` agrega al final `"fw"`, `"caps"` y `"foto"` (medias de rojo, verde y azul de todo el cuadro y de su mitad de abajo; milésimas de muestras quemadas y oscuras; cada 500 ms). `AUTOPRUEBA` queda en 0.

### 2.8 Caja negra (idea I)

Un archivo de texto por corrida en `Android/data/com.fsi.carro/files/cajas_negras/cn_<fecha>_<hora>.txt`. Se conservan las últimas 40. Cada línea: `<ms desde el inicio> <texto>`.

| Línea | Qué trae |
|---|---|
| `CAB` | ruta, firmware, giroscopio, láser, cámara, detector, configuración de visión, `/stats` de la cámara, teléfono |
| `CN` | una por tick. `t` hora; `e` estado; `f` fase del rodeo y del regreso; `c` comando; `p` x, y, rumbo×10; `ip` paso; `tof` mm, válida, edad; `cu` edad del último cuadro; `fv` fuentes por zona (1 color, 2 red, 3 las dos); `hay`; `cz` cercanía; `nz` celdas; `pr` precaución; `vz` vistazo (lado, fase, ecos, total); `esq` esquivadas (ruta, obstáculo); `ay` ayudas; `ca` causa; `ob` rodeo abierto; `lat` desvío y fin del obstáculo; `fin`; `mod` detector (requerido, vigente); `rf` objetivo de rumbo. La app agrega `ang`, `mm`, `bs` del último SENS |
| `VTEL` | una por cuadro. `c` cuadrícula; `fm` primera fila; `n` celdas; `um`, `mc` umbral y mínimo; `gan` ganancia; `mot` motivos; `b` brillo media, p10, p90; `cr` croma; `sat` quemadas, negras; `ref` color de la franja; `d` distancia al piso media, p50, p90, máx.; `h` histograma (0-9, 10-19, 20-29, 30-39, 40-59, 60-89, 90-149, 150+); `np` celdas no piso; `z` por zona y banda; `f` por fila; `g` grupos del piso con su peso |
| `*_DIAG` | las decisiones del navegador (`ESQUIVE_DIAG`, `BARRIDO_DIAG`, `BORDE_DIAG`, `VISTAZO_DIAG`, `REINC_DIAG`, `AYUDA_DIAG`…) |
| `EV` | `TOF_PARO`, `WD`, percepción incompleta |
| `FIN` | estado, causa y mensaje |

Ninguna línea lleva una imagen.

### 2.9 Revisión independiente del diff de seguridad (14-nov)

Un revisor independiente (otro agente, solo lectura, con su propio banco fuera del repositorio) leyó completos los cambios del navegador y del firmware del carro. **No encontró fallos críticos** (ningún avance ni reversa sin límite; el STOP forzado sigue intacto; el protocolo conserva la compatibilidad) y sí estos, todos corregidos:

| Sev. | Hallazgo | Corrección |
|---|---|---|
| ALTO | La vuelta completa del cambio de lado no comprobaba que cupiera: con una pared a 30–44 cm de la línea el chasis la tocaba y seguía empujando | Compuerta de la vuelta, guardia del giro y causa 18 (2.1) |
| ALTO | Junto a una esquina, el paso en paralelo seguía de frente ~1 m más allá, repitiendo el mismo plan hasta el freno de «lejos de la ruta» | El paso no pasa del final de la recta; a la tercera vez que el plan de «pasar primero» no avanza se pide ayuda; por dentro de una esquina no se alarga con la cuenta de otro tramo; el tope del primer giro del regreso descuenta lo que dobla la ruta |
| MEDIO | La coincidencia con cercanía 3 frenaba al segundo cuadro; el comentario decía que al primero | En el centro frena al primero (2.3) |
| MEDIO | Reversas mayores que la regla escrita (15 cm) y 30 cm «deducidos» detrás al entrar a la línea, sin haberlos pisado | Se quitó el espacio deducido (no cambió ningún resultado del banco). Quedan tres reversas, documentadas con su tope: 15 cm en el reintento de siempre; de 15 a 25 cm y nunca más de lo avanzado en recta; y deshacer una diagonal, hasta 45 cm y nunca más de lo avanzado en ella |
| MEDIO | Una pared a 25 cm o menos contaba como lado abierto y el carro avanzaba hacia ella | Cuenta como pared (2.1) |
| BAJO | «Continuar» tras la ayuda arrastraba un `TOF_PARO` o un vistazo confirmados antes y rodeaba un obstáculo que ya no estaba | Se olvidan al continuar |
| BAJO | Dos banderas dependían de que la memoria llegara en ceros | Se fijan en `navegador_iniciar` |
| BAJO | Firmware: tras ~33 s frente a algo, `TOF_PARO:<mm>` podía llevar una distancia vieja | Índice propio para las últimas lecturas (`tof.c`) |

Dudas del revisor que quedan para la pista: con `NAV_VISTAZO_LEJOS` el primer vistazo puede darse con lo visto ya a cercanía 2 (~37 cm), torciendo hacia ello; en la diagonal de regreso la reacción al láser depende de los cuadros; junto a paredes el rodeo puede terminar 0.8–1.4 m más allá del destino dibujado (también en la rama de partida).

De la revisión salieron dos escenarios nuevos del banco (`caja_y_pared_al_lado` y `caja_antes_de_esquina`) y pruebas unitarias: 17 en `pruebas_esquive_confirmado`, 2 en `pruebas_navegador` y 5 en el firmware.

## 3. Banderas

| Bandera | Por defecto | Otros valores |
|---|---|---|
| `navegador_poner_precaucion` | `NAV_PRECAUCION_NUNCA` | `COLOR_CERCA` (solo color, centro, cerca), `SIEMPRE` (como la rama de partida) |
| `navegador_poner_vistazo` | `NAV_VISTAZO_LEJOS` | `NO`, `LEJOS_Y_MEDIO`, `TODOS` |
| `rumbo_en_vuelta` (campo) | 0 | 1: apuntar al punto planeado durante el avance del regreso. Mejora 1–2 mm el error lateral y empeora el rumbo final |
| `AUTOPRUEBA` (cámara) | 0 | 1 para el banco |
| `NAV_DESHACER_GIRO_MAX` (constante) | 0 (apagado) | 50: si la guardia detiene el primer giro antes de esos grados, deshacerlo y rodear por el otro lado |

La app todavía no expone las dos primeras: valen sus valores por defecto. La autoridad del láser sigue encendida por defecto.

## 4. Lo que no se implementó y por qué

- **V1, bloqueo de exposición y balance de blancos.** El GC0308 no tiene hoja de datos en el proyecto y el firmware no escribe ningún registro del sensor. Escribirlos a ciegas, sin poder grabar ni ver la cámara, puede dejarla sin imagen. Diseño: `/control?expo=fija|auto&awb=fijo|auto`, atendido por la tarea de captura, que se libera sola si no hay PING en N segundos; un firmware anterior contesta 400 y la app sigue como hoy. Antes hay que **medir** con la telemetría nueva si la cámara de verdad cambia entre «Aprender piso» y la ruta.
- **V2, aprendizaje continuo.** Aprender el piso en marcha puede aprender un obstáculo que el láser no ve (algo bajo). Con `piso_fijo` eso no pasa. Sin datos del carro que digan cuánto deriva el piso en una ruta, el riesgo no se justifica.
- **V3, clasificador.** En el banco las juntas no produjeron falsos positivos; las manchas de sol y los reflejos sí, y esos se resolvieron en la decisión (2.3), no en el clasificador. Tocar el clasificador sin imágenes reales es ajustar contra mi propio modelo sintético.
- **V5, fila–distancia.** Las bandas 1/2/3 siguen siendo proximidad de imagen. La autocalibración con el láser necesita lecturas reales emparejadas; la caja negra ya las guarda (`CN tof` y `VTEL f`).
- **V6, sello por cuadro.** El MJPEG no lleva número ni hora. Requiere cambiar el formato del video.
- **A, odometría fusionada.** La distancia sigue por tiempo calibrado. La tasa de cambio del láser contra un obstáculo fijo es utilizable solo en los ~50 cm antes de frenar.
- **B, acelerómetro.** Diseño: `CAPS ACC=<0|1>` antes de `FW:`; bits nuevos en las banderas de `SENS` (32 golpe, 64 inclinado); `ACC?` → `ACC:<ax>,<ay>,<az>,<pico>,<banderas>`. Sin poder grabar el firmware no se puede calibrar ningún umbral de golpe.
- **E, mapa local.** El rodeo medido con mirada cubre los casos del banco; un planificador sobre rejilla es más código que probar del que cabe sin el carro.
- **G, giro fino de lazo cerrado por pulsos** (`GMIN=1`). Diseño: pulso–medir–pulso dentro de `GIRAR`. Mientras tanto, los ajustes de menos de 5° los absorbe el rumbo.
- **Coincidencia con memoria** (sostener la confirmación cuando la red parpadea). Se probó: subía banquito y silla, pero daba 23 esquives por cada 200 corridas con manchas de sol. Retirada.
- **Afinar de lado hasta el final del tramo tras un rodeo.** Se probó: 1–3 mm menos de error lateral y 2° más de error de rumbo final. Retirada.
- **Deshacer el giro y rodear por el otro lado** cuando la guardia detiene el primer giro. Se probó: en el escenario de la caja con una pared pegada bajó de 615 a 534 de 1000 y subió de 2 a 7 contactos (el carro queda unos 15 cm más cerca del obstáculo y el giro de salida deja de caber). Queda apagado (`NAV_DESHACER_GIRO_MAX` en 0).

## 5. Resultados, riesgos y límites

1000 corridas por escenario, semillas `170000 + índice × 10007 + corrida`. «ok» = llegó, sin contacto físico y dejando atrás el objeto. «bajo mueble» = pasó entre las patas, por debajo, sin tocar: no cuenta como contacto ni como éxito.

| Escenario | v1.1 (`bd1727b`) | Partida (`6bea71e`) | Nueva |
|---|---|---|---|
| libre_limpio | 948 ok, 4 ayuda, 52 esq., para en 138 | 1000 ok, para en 192 | 1000 ok, para en 0 |
| falso_juntas | 967 ok, 1 ayuda, 33 esq., para en 123 | 1000 ok, para en 196 | 1000 ok, para en 0 |
| falso_sol | 179 ok, 792 ayuda, 255 esq., para en 658 | 996 ok, 4 esq., para en 852 | 1000 ok, 0 esq., para en 93 |
| falso_sombra | 298 ok, 552 ayuda, 418 esq., para en 583 | 989 ok, 12 esq., para en 746 | 1000 ok, 0 esq., para en 82 |
| falso_reflejo | 520 ok, 466 ayuda, 1409 esq., para en 511 | 987 ok, 14 esq., para en 569 | 1000 ok, 0 esq., para en 126 |
| falso_exposicion | 956 ok, 5 ayuda, 41 esq., para en 144 | 1000 ok, para en 204 | 1000 ok, para en 0 |
| falso_detector | 142 ok, 289 ayuda, 1516 esq., para en 984 | 1000 ok, para en 1000 | 1000 ok, para en 0 |
| caja20 | 629 ok, 2 cont., 370 ayuda | 1000 ok | 1000 ok |
| caja20_lateral | 991 ok, 3 cont., 7 ayuda | 663 ok, 337 cont. | 998 ok, 2 cont. |
| banquito35x45 | 245 ok, 596 cont., 470 bajo mueble, 267 ayuda | 17 ok, 133 cont., 939 bajo mueble | 765 ok, 79 cont., 160 bajo mueble, 58 ayuda |
| objeto_largo90 | 4 ok, 535 cont., 724 ayuda | 85 ok, 898 cont., 71 ayuda | 1000 ok |
| silla_patas | 161 ok, 524 cont., 708 bajo mueble, 139 ayuda | 39 ok, 12 cont., 950 bajo mueble | 316 ok, 34 cont., 644 bajo mueble, 64 ayuda |
| pared_tras_el_final | 1000 ok | 68 ok, 449 cont., 923 ayuda | 1000 ok |
| dos_obstaculos | 359 ok, 67 cont., 579 ayuda | 19 ok, 110 cont., 905 ayuda | 1000 ok |
| pasillo | 559 ok, 5 cont., 438 ayuda | 1000 ok | 1000 ok |
| aparece_de_repente | 521 ok, 7 cont., 473 ayuda | 1000 ok | 1000 ok |
| objeto_bajo | 960 ok, 25 cont., 28 ayuda | 990 ok, 4 cont., 6 ayuda | 997 ok, 1 cont., 2 ayuda |
| caja20_con_sol | 447 ok, 3 cont., 550 ayuda | 997 ok, 3 cont. | 1000 ok |
| bateria_baja | 569 ok, 4 cont., 427 ayuda | 1000 ok | 1000 ok |
| pared_al_frente (imposible) | 0 paran bien, 1000 cont. | 520 paran bien, 480 cont. | 1000 paran y piden ayuda, 0 cont. |
| bajo_solo_color (límite) | 967 ok, 22 cont., 16 ayuda | 975 ok, 18 cont., 7 ayuda | 986 ok, 14 cont. |
| caja_y_pared_al_lado (nuevo) | 470 ok, 79 cont., 478 ayuda | 632 ok, 216 cont., 210 ayuda | 615 ok, 2 cont., 385 ayuda |
| caja_antes_de_esquina (nuevo) | 586 ok, 2 cont., 412 ayuda | 999 ok, 1 ayuda | 1000 ok |

Dieciséis de los 23 escenarios quedan con 1000 de 1000 y cero contactos (los siete de falsos positivos sin un solo esquive ni ayuda); en la pared al frente, donde lo correcto es detenerse y pedir ayuda, también las 1000.

**Compuertas que no se cumplen** (el script `scripts/compuertas_autonomo.py` deja 19 en rojo; no se relajó ninguna):

1. **Banquito y silla con patas**: 765 y 316 de 1000, con 79 y 34 contactos con una pata y 58 y 64 ayudas. El láser pasa entre las patas, la red las da a ratos y el color las ve tarde y delgadas. La silla tiene **más contactos que la rama de partida** (34 contra 12): la de partida casi siempre pasaba por debajo sin enterarse; la nueva a veces confirma una pata con el vistazo y maniobra cerca. La guardia del giro cambia algunos de esos casos por ayuda (causa 18). El modelo de mueble del banco es una suposición (patas de 25–35 mm, asiento por encima del haz): **NO MEDIDO**.
2. **Caja corrida de lado**: 2 contactos en 1000 (la de partida, 337). **Objeto bajo**: 1 contacto y 2 ayudas (la de partida, 4 y 6).
3. **Falsos positivos con manchas de color**: 0 esquives y 0 ayudas, pero el carro se detiene un momento en 8–13 % de las corridas, cuando color y red coinciden. Eso lo manda la regla aprobada. (Antes de frenar al primer cuadro en el centro eran 7–12 %.)
4. **Caja con una pared pegada a un lado** (25–44 cm de la línea; escenario nuevo): 615 de 1000, 2 contactos y 385 ayudas. La cámara del banco no distingue esa pared clara del piso, así que el carro elige ese lado en ~38 % de las corridas; el láser la descubre al girar y el carro se detiene y pide ayuda (causas 18 y 16). La rama de partida llegaba 632 veces pero **tocaba la pared 216**. Es el único escenario donde el éxito queda por debajo de la partida.
5. **Monte Carlo heredado** (`scripts/comparar_esquives.py`): el error lateral final tras un esquive pasa de 17/40 a 18/44 mm (media/p95) con RUMBO. El paso junto al objeto es unos 30 cm más largo y queda menos recta para afinar. Sin firmware nuevo mejora (33/78 → 25/65).

El objeto bajo que solo ve el color (límite conocido de la percepción) quedó en 986 de 1000 con 14 contactos, mejor que la rama de partida (975 y 18) aunque el carro ya no avanza a ratos: lo que lo arregló fue mantener el rodeo decidido después de retroceder (2.3).

**Riesgos que el simulador no cubre:**

- El pivote del lado izquierdo es el espejo del derecho, no una medida propia.
- El campo de la cámara (56°), su altura (115 mm) y su inclinación (12°) son los del manual, sin medir. La pista del paso en paralelo y el vistazo dependen del campo.
- El alcance del láser contra una pared vista de lado (el corte de 700 mm) sale del modelo del banco.
- El vistazo depende de cómo responde el control de rumbo real a un cambio de 10–16°; en el banco se pasa unos 3°.
- No hay sensor trasero: las reversas (hasta 250 mm tras un paro o un vistazo, hasta 450 mm para deshacer una diagonal) recorren solo camino que el carro acaba de pisar y suponen que nadie entró detrás.
- La guardia del giro depende de la latencia real de `SENS` y de cuánto tarda en detenerse un giro: en el banco no evitó el toque con una pared a menos de ~27 cm del lado hacia el que gira (2 de 1000).
- Una pared del mismo tono que el piso no la ve la cámara: el carro solo la descubre con el láser, al girar hacia ella.
- El rodeo ahora es más largo: una caja de 20 cm se deja atrás unos 60 cm antes de volver a la línea. Con el destino a menos de ~1.3 m del obstáculo, el carro puede volver a la línea justo al final o terminar más allá del destino dibujado.

## 6. Qué necesita firmware nuevo

| Función | Carro actual (v1.1) | Carro `SEG=2` | Cámara actual | Cámara `cam-v2` |
|---|---|---|---|---|
| Rodeo medido, mirada, vistazo, caja negra | Sí (usa `GIRAR`, `SENS` y `RUMBO` que ya existen) | Sí | Sí | Sí |
| Desconocidos sin efecto, `FORWARD` bloqueado, freno rearmado | No | Sí | — | — |
| Telemetría fotométrica en la caja negra | — | — | No (`foto=sin_datos`) | Sí |

La app nueva funciona con los dos firmwares actuales. Los firmwares nuevos funcionan con la app v1.1.

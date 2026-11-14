# Auditoría del modo autónomo, de la cámara a los motores (autónomo v2)

Rama `feat/autonomo-v2`, creada desde `fix/navegacion-esquive-confirmado` (`6bea71e`). Fecha: 13-nov-2026.
**Todo lo que aquí se dice del comportamiento es lectura de código o SIMULADO; nada de esta auditoría se midió en el carro.** No se movió el vehículo, no se instaló APK ni se grabó firmware, no se leyó `secretos.h` ni se tocó `original_backup/`, y no se vieron imágenes reales de la cámara.

Los números de línea se refieren al commit `6bea71e` (los archivos cambian después en esta misma rama). Método: lectura completa del núcleo (`nucleo_c`), tres auditorías independientes de solo lectura (cámara, firmware del carro, app) y un banco nuevo que corre el navegador, la fusión y el clasificador de color reales contra un mundo sintético ([resultados de partida](#3-evidencia-del-banco-realista-simulado)).

Severidad: **CRÍTICO** = puede producir contacto o un Terminado falso; **ALTO** = rompe la autonomía (AYUDA o ruta cancelada sin necesidad) o esconde la causa; **MEDIO** = degrada precisión o robustez; **BAJO** = deuda o documentación.

## 1. Resumen ejecutivo

1. La rama de partida arregló la decisión para la caja centrada, pero en un mundo más realista **toca objetos que v1.1 esquivaba**: una caja corrida de lado (el cono del láser a 400 mm cubre ±89 mm y medio chasis mide 100 mm), muebles de patas (el láser pasa entre ellas y el color no junta las 7 celdas), un objeto de 90 cm de fondo y una pared 45 a 65 cm más allá del final de la ruta.
2. La causa de fondo de los contactos al rodear no es la percepción sino la **geometría del pivote**: el carro gira sobre un costado (el pivote derecho de 90° medido, 150 mm adelante y 165 mm a la derecha, equivale a girar sobre un punto 157 mm a la derecha del centro). El giro de regreso barre el chasis 21 a 29 cm **hacia** el objeto. Los bancos anteriores del esquive corrían sin pivote.
3. La causa más probable de los **falsos positivos de visión** del 11-nov es fotométrica: manchas de sol, reflejos y sombras cambian el brillo del piso más de lo que tolera la regla de tono (150 de 765), el reajuste de brillo solo actúa si TODO el piso cambia, el piso queda congelado desde «Aprender piso» y la cámara corre con exposición, ganancia y balance de blancos automáticos. Los valores 240 y 23 son legales en las barras de la app: son un síntoma (alguien desensibilizó), no la causa.
4. **«Terminado» solo sale con `NAV_TERMINADO`**, pero la app deja correr y cerrar una ruta con la percepción inoperante (sin modelo neuronal el color solo nunca frena, y cada cuadro con el modelo caducado cuenta como «sin coincidencia» para el cierre).
5. Las causas se pierden: la AYUDA no deja registro de su código, las cancelaciones a mitad de ruta no se muestran y el logcat del teléfono descarta líneas. Por eso no se pudo diagnosticar la AYUDA de B6-r5. El banco da una hipótesis: en v1.1 la causa dominante es la 1 (los dos lados ocupados 4 s por color) en el 16 % de las corridas con caja, parecido al 1 de 6 medido.

## 2. Hallazgos por capa

### 1A. Cámara y transmisión

| Sev. | Hallazgo | Dónde |
|---|---|---|
| CRÍTICO | El sensor GC0308 corre 100 % automático (exposición, ganancia, balance de blancos): el firmware no llama ningún `set_*` ni `set_reg`. El teléfono congela el piso (`piso_fijo`) desde que termina «Aprender piso» y pueden pasar hasta 63 s antes de moverse. El único amortiguador es un reajuste escalar de brillo (≥18 %, mismo tono, las tres zonas). Un cambio de balance de blancos no tiene compensación. | `camara_prueba/camara_prueba.ino:492-497`; `EjecutarRutaFragment.kt:98,260,390`; `obstaculos.c:1008-1100,1137,1182` |
| ALTO | `AUTOPRUEBA 1` en el firmware de producción: durante ~1 min tras cada arranque pide `size=qvga` (reinicia el sensor, no lo restaura) y alterna compresores con un teléfono conectado. | `camara_prueba.ino:85,1410,1422-1446` |
| ALTO | No hay edad por cuadro: se sella al llegar al teléfono, el navegador lo sella con la hora del tick y usa una latencia constante (320 ms en JPEG, de una sola medida con cronómetro). No hay edad máxima. El MJPEG no lleva número ni tiempo. | `CamaraObstaculos.kt:523,534,1277-1297`; `navegador.c:4300-4302`; `ModoSemiautonomo.kt:198` |
| ALTO | «Aprender piso» no funciona en modo CELDAS (UDP): las rutas corren sobre JPEG o RAW. La telemetría que solo viaje en la cabecera `CEL1` no llega al modo autónomo. | `puente.c:232-252,459-460` |
| MEDIO | Cualquier reinicio del sensor (cambio de tamaño, doble init del arranque, recuperación) borra el estado fotométrico. | `camara_prueba.ino:507-521,549-571` |
| MEDIO | `enviar_todo` no corta al cliente lento a 1.5 s (reintenta hasta 99 veces); tras un atasco entrega un cuadro viejo. | `camara_prueba.ino:1014,1028-1030` |
| MEDIO | El `numero` de `CEL1` no descarta atrasados ni duplicados y no hay identificador de arranque. | `CamaraObstaculos.kt:574-585` |
| MEDIO | La app solo lee `/stats` antes de abrir el video: sin telemetría de la cámara durante la ruta. | `VideoCarro.kt:18-19,317-340` |
| MEDIO | El capítulo 5 del manual contradice el código: `/stream` está en el puerto 80 (no 81); la cabecera `CEL1` real es marca, `numero`, `ms_captura`, ancho, alto, columnas, filas y 2 bytes reservados (no hay `seq`, `ts_ms`, `cap_us`); la compresión tarda 54 a 56 ms (no 9.2); captura en el núcleo 0 con 3 búferes. | `paquete_celdas.h:8-16`; `camara_prueba.ino:484,1347,1627`; `docs/latencia.md:101` |
| BAJO | Sin telemetría térmica ni de alimentación (solo `brownout` como causa de reinicio). Los simuladores de cámara no modelan deriva, pérdida ni reinicio. | `camara_prueba.ino:353`; `scripts/camara_falsa.py:39-41` |

### 1B. Visión

| Sev. | Hallazgo | Dónde |
|---|---|---|
| CRÍTICO | El clasificador de piso compara distancia RGB; la regla de «mismo tono, otra luz» solo tolera 150 de 765 de diferencia de brillo. Una mancha de sol, un reflejo o una sombra fuerte quedan «no piso» aunque conserven el tono. El reajuste de brillo es global (70 % de las celdas, las tres zonas y la franja): no sirve para luz local. | `obstaculos.c:552-570,1008-1100`; `obstaculos.h:89-92` |
| ALTO | La cercanía sale solo de la fila más baja que alcanza la mancha: cualquier falso positivo que toque las filas de abajo es «OBSTACULO 3». | `obstaculos.c:762-797` |
| ALTO | Una pata delgada (25 a 35 mm) ocupa 1 o 2 celdas de ancho: no junta `MIN_CELDAS` (7) en su zona, así que el color no la marca y no puede coincidir con la red. | `obstaculos.c:776-797`; `obstaculos.h:101` |
| ALTO | El detector neuronal no filtra por clase, no revisa la geometría de la caja ni pide confirmación temporal (eso solo existe en el modo «cualquier objeto», apagado). Una caja inventada de una sola inferencia cuenta como fuente. La vigencia de 300 ms se mide desde que se OFRECIÓ el cuadro. | `fusion.c:228-249`; `DetectorTflite.kt:59,195`; `CamaraObstaculos.kt:963-968` |
| MEDIO | Aprender piso: el modelo se reemplaza aunque el informe salga «muy variado» o con 0 grupos; un obstáculo que toque el 20 % inferior se aprende como piso; la máscara de «Calibrar visión» no se aplica al aprender. | `obstaculos.c:1463-1538`; `puente.c:540-553` |
| MEDIO | Corredor de perspectiva: apagado por defecto y con constantes sin medir (10 cm, 20°, 60°) que no coinciden con las del manual (115 mm, 12°, 56°). Con la fusión por coincidencia queda inoperante. | `corredor.h:37-42`; `fusion.c:205-207`; `ModoSemiautonomo.kt:360` |
| MEDIO | Juntas de loseta: un grupo con menos del 15 % de apoyo no se aprende y el filtro de ruido solo quita celdas aisladas. En el banco no llegan a marcar con juntas de hasta 8 mm (pasan por la regla de tono): hipótesis de menor rango. | `obstaculos.c:343-360,746-757` |
| BAJO | El flujo del piso está apagado y solo trabaja sobre imagen completa. El manual dice «YCbCr»; el clasificador trabaja en RGB con color normalizado. | `movimiento_piso.h:26`; `docs/manual/capitulos/08_vision.md` |

### 1C. Fusión y decisión

| Sev. | Hallazgo | Dónde |
|---|---|---|
| CRÍTICO | Solo confirman el láser a ≤400 mm o la coincidencia color+red en el mismo cuadro. El cono (±12.5°) a 400 mm cubre ±89 mm: una caja cuyo borde quede entre 89 y ~110 mm del eje nunca se confirma y el carro la roza. Banco: caja corrida de lado, 100 contactos de 300 (v1.1: 2). | `navegador.c:2071-2072,2104-2146`; `navegador.h:203` |
| CRÍTICO | Muebles de patas: el láser pasa entre las patas, el color no junta celdas y la red sola no frena. Banco: banquito 295 y silla 289 contactos de 300 con la planta del mueble (v1.1: 206 y 261). | `navegador.c:2108`; `fusion.c:194-216` |
| CRÍTICO | La vigencia del modelo neuronal no llega al navegador: sin red (apagada, caída o caducada) el color nunca frena y cada cuadro suma a `vision_sin_coincidencia`, que basta para cerrar la ruta. | `puente.c:2451-2462`; `navegador.c:1149-1152,2302` |
| ALTO | «Precaución»: con una sola fuente y el láser sin eco (lo normal en un cuarto abierto: más de 1.2 m libres) el carro avanza por tandas de 700 ms y 300 ms de pausa. Banco: 4.8 paradas por corrida con sol, sombra o reflejo, y 0.2 por corrida en piso limpio por cajas inventadas de la red. Cada pausa suma error de distancia por tiempo. | `navegador.c:2110,4349-4359` |
| ALTO | `zona_hay` de los lados solo vale con coincidencia: `elegir_lado` no ve un obstáculo lateral real que solo marque el color (usa el conteo de celdas como desempate). | `navegador.c:2132,3418-3430` |
| MEDIO | El interruptor «Autoridad del láser» no hace nada: `autoridad_laser` se guarda y nunca se lee. | `navegador.c:1691,3964-3970` |
| MEDIO | Estados muertos: el sondeo del arranque ahora exige a la vez láser cerca y centro libre; la racha «solo cercanía 2 al centro» y el veto del láser conservan lógica de v1.1 que ya no decide. | `navegador.c:2188-2199,2321-2328,2544-2563` |

### 1D. Navegación

| Sev. | Hallazgo | Dónde |
|---|---|---|
| CRÍTICO | Cierre de ruta: exige 3 lecturas válidas >800 mm o «sin eco». Si hay una pared o un mueble entre 40 y 80 cm más allá del destino la lectura es válida y nunca libre: el cierre «reintenta» **esquivando la pared**. Banco: 139 contactos y 269 AYUDA de 300 (v1.1 terminaba 300 de 300). | `navegador.c:1136-1168,3459-3495,4368-4381` |
| CRÍTICO | El regreso «casi transversal» gira ~85° hacia la línea con el objeto todavía al costado. El pivote derecho mueve el frente 285 mm hacia el objeto (el izquierdo 210 mm) y el desvío lateral es de 400 mm. Mirar y volver atrás acerca además el carro ~255 mm al objeto. Banco: objeto de 90 cm, 183 contactos de 300. | `navegador.c:3052-3209,3728-3790` |
| CRÍTICO | Obstáculo ancho o pared: tras el tramo de lado, el giro de enderezarse desplaza el centro 165 mm (pivote derecho) hacia el plano del obstáculo. Banco: pared que cierra el paso, 36 contactos de 300 (v1.1: 300 de 300). No se mide el ancho antes de comprometer la maniobra. | `navegador.c:3001-3047,3582-3608` |
| ALTO | Presupuesto de 6 «esquivadas» por ruta que también cuenta las extensiones y nunca se repone al pasar un obstáculo. Banco: dos cajas seguidas, 245 AYUDA de 300 por la causa 2. | `navegador.c:3518,3747-3755,4264` |
| ALTO | Micro-giros: un giro de exactamente 4° se hace (el mínimo útil es 4) pero no se pide al firmware (su mínimo es 5): va por tiempo. El pivote va a PWM 255 sin rampa y STOP es rueda libre: medido 4°→18° y 4°→5°. | `navegador.h:102,191`; `navegador.c:984-994,2814-2817`; `firmware_carro/carro.c:164-200` |
| ALTO | Tras un `GIRO_TIMEOUT` o un giro abortado, `giro_fw_caido` manda TODOS los giros restantes de la ruta por tiempo, aunque el giroscopio esté sano. | `navegador.c:1016,1062,4138-4142` |
| MEDIO | La AYUDA escribe `motivo` (1 a 3) pero no la causa (0 a 15); `ayuda_continuar` la borra. | `navegador.c:723,4266` |
| MEDIO | Sin láser, la distancia a la cara se toma de la banda visual (300 o 500 mm): son estimaciones sin calibrar. `NAV_SENSOR_FRENTE_MM` (120) es un supuesto. | `navegador.c:3375-3400`; `navegador.h:205` |
| MEDIO | El retroceso de escape (150 mm) es ciego: no hay sensor trasero y `TOF_PARO` no actúa en reversa ni en pivotes. | `navegador.c:3434-3440`; `firmware_carro/sensores.c:185-200` |

### 1E. Firmware del carro

| Sev. | Hallazgo | Dónde |
|---|---|---|
| CRÍTICO | Un texto desconocido renueva el watchdog y queda como `comando_actual`: `carro_avanzando()` pasa a falso con los motores adelante, así que el freno láser no actúa y RUMBO deja de corregir. Un cliente que sondee un comando nuevo contra un firmware viejo deja el carro avanzando sin watchdog y sin láser. | `carro.c:203-206,425-430,622-625`; `sensores.c:190-193` |
| ALTO | Carrera que deja el freno láser enclavado en apagado si `__STOP_AUTO` y un `FORWARD` recién llegado se ejecutan en la misma pasada de la cola. | `sensores.c:190-193`; `carro.c:411-412,562-572` |
| ALTO | `FORWARD` no se bloquea con el obstáculo confirmado: cada reintento da un pulso a 255 antes de volver a frenar. | `pruebas_sensores.c:1326-1328` |
| ALTO | Avance con el láser ciego sin aviso: 500 ms de datos viejos antes de declararlo caído; lecturas <8 mm refrescan el «vivo»; `TOF_USO:0` persiste entre clientes y no se anuncia. Con el láser caído el carro sigue avanzando. | `tof.c:126-133,210-213`; `sensores.c:101`; `pruebas_sensores.c:1350-1356` |
| ALTO | Giro a ciegas si el MPU6050 se duerme o congela: `GIRAR:90` seguiría a 255 hasta su tope (1850 ms, ~240°). | `giro.c:180,334-338`; `imu.c:350-358` |
| ALTO | `GIRAR` rechaza menos de 5°; sin tolerancia ni corrección posterior (`GIRO_OK` significa «se cumplió la regla de corte»). | `giro.c:159-162,300-349`; `config.h:208` |
| ALTO | Los capítulos 4 y 6 del manual describen un firmware que no existe (`GIRO:<grados>` con frenado proporcional y tolerancia ±2°, bloqueo frontal con histéresis a 150 mm, `EBRAKE` activo, RUMBO a 50 Hz, IMU a 200 Hz, comandos `BACKWARD`, `ESTADO`, `VERSION?`, `SENS?`, `CAL_IMU`). Varios de esos «comandos» son textos desconocidos (ver el primer hallazgo). | `docs/manual/capitulos/04_firmware.md`; `06_protocolo.md` |
| MEDIO | La cola es FIFO: STOP no se adelanta, solo no se desaloja. El watchdog lo renuevan comandos que no mueven (`MODO_*`, `BAL_*`, `GIRAR` rechazado). `IMU_CERO` se acepta en marcha. Una lectura «libre» intercalada reinicia la racha del freno. Sin causa de reinicio ni contador de arranques. | `carro.c:469-472,498-528`; `sensores.c:353-355`; `tof.c:136,147` |
| MEDIO | El servidor falso diverge del firmware (CAPS sin `RUMBO=1`, no entiende `RUMBO:*`, ignora desconocidos, freno láser a 10 Hz, `GIRAR` sin asentado) y su prueba fija esas diferencias. `verificar_version.py --puerto` exige siempre el teléfono. | `servidor_falso.py:132,186,251-255`; `probar_servidor_falso.py:120`; `verificar_version.py:136-149` |
| BAJO | Solo se lee `GYRO_ZOUT`; el acelerómetro está despierto y sin usar. Si `loop()` se cuelga el PWM sigue hasta 5 s. STOP y EBRAKE son rueda libre. | `imu.c:24,296-305`; `config.h:143`; `carro.c:685-699` |

### 1F. App y comunicación

| Sev. | Hallazgo | Dónde |
|---|---|---|
| CRÍTICO | La ruta arranca, corre y cierra con la percepción inoperante sin aviso: sin modelo neuronal no hay coincidencia posible; el error del modelo va a un campo que Ejecutar ruta no muestra; tampoco avisa si el láser está apagado. | `CamaraObstaculos.kt:962,987-989`; `ModoSemiautonomo.kt:192-205` |
| CRÍTICO | Los ajustes experimentales de «Visión» (umbral 0 a 300, celdas 1 a 40, fila, K, modelo, corredor, flujo) gobiernan la ruta sin validarse ni mostrarse; se guardan en cada movimiento de la barra y sobreviven a `adb install -r`. Ya pasó tres veces (72, 119 y 240). | `ConfigVision.kt:28-29,63,67`; `VisionActivity.kt:608-614`; `EjecutarRutaFragment.kt:259-262` |
| ALTO | Las causas se pierden: el motivo de una cancelación a mitad de ruta nunca se muestra; la causa de AYUDA solo vive en pantalla; el historial no la guarda; no hay archivo de registro (solo logcat, que el Huawei recorta). | `EjecutarRutaFragment.kt:473-475,541-550`; `AlmacenRutas.kt:118-127` |
| ALTO | El parser de `SENS` exige exactamente 3 campos: cualquier ampliación del firmware mataría todas las rutas a 1.5 s. | `ControlLogica.kt:572-573` |
| ALTO | No se vigila la salud del detector de color (el motivo «todo no piso» solo va a logcat) ni se distingue un cierre confirmado por láser de uno visual. | `obstaculos.c:1219-1221`; `EjecutarRutaFragment.kt:583-589` |
| MEDIO | `SENS` se sella al procesarlo: una ráfaga TCP de lecturas viejas entra como 3 lecturas frescas. El control comparte hilo con la interfaz (cada tick y cada SENS redibujan). El resumen y la rejilla que llegan al navegador pueden ser de cuadros distintos. | `ControlLogica.kt:577-579`; `navegador.c:4162-4164`; `puente.c:431-439,2463-2473` |
| MEDIO | En AYUDA, «adelante» cerca del obstáculo cancela la ruta (el `TOF_PARO` se toma como no atendido). Un solo cuadro con error del detector cancela la ruta. El mensaje dice 500 ms y el corte de video es a 700. | `ModoSemiautonomo.kt:209-213,331,382-387,417` |
| BAJO | «Restablecer» deja K = 4 (la app usa 3). La pantalla no fija orientación: girar el teléfono cancela la ruta. El WebSocket no se ata a la WiFi del carro. | `ConfigVision.kt:196`; `MainActivity.kt:184-188` |

### 1G. Transversal

- **Unidades:** coherentes (mm, grados, décimas de grado con signo documentado en cada frontera, un solo reloj `elapsedRealtime` truncado a 32 bits con restas sin signo). No se encontraron desbordes.
- **Constantes fijas donde debería haber medidas:** centro-sensor 120 mm, chasis 200×240 mm, altura e inclinación de la cámara, altura y cono del láser, latencia visual de 320 ms, bandas 300/500 mm. Todas quedan como SUPUESTO en el diseño y como dato a medir en el protocolo físico.
- **Batería:** el protocolo no lleva voltaje; no se vigila ni la del carro ni la caída de velocidad.
- **Caminos a Terminado:** `tick_siguiendo` (`navegador.c:2686`), `reinc_terminar` (`:3316`) y la vuelta tras «toma el control» (`:3810`). Los tres pasan por la verificación de `navegador_tick` (`:4368`) si la ruta usa cámara o láser.

## 3. Evidencia del banco realista (SIMULADO)

`nucleo_c/pruebas_autonomo_v2.c` con `scripts/compuertas_autonomo.py`, 300 corridas por escenario, mismas semillas y mismo mundo para las tres versiones; el informe de 1000 corridas va en `docs/precision/autonomo_v2_comparacion.txt`. Supuestos del mundo en [el diseño](autonomo_v2_diseno.md).

| Escenario | v1.1 `bd1727b` (éxito / contactos / AYUDA) | Partida `6bea71e` |
|---|---|---|
| Piso libre con sol | 54 / 0 / 235 | 299 / 0 / 0 (1439 paradas) |
| Piso libre con sombra | 91 / 0 / 167 | 295 / 0 / 0 (1334 paradas) |
| Piso libre con reflejo | 156 / 0 / 140 | 297 / 0 / 0 (1444 paradas) |
| Caja 20×20 centrada | 242 / 4 / 55 | 300 / 0 / 0 |
| Caja 20×20 corrida de lado | 296 / 2 / 2 | 200 / **100** / 0 |
| Banquito 35×45 con patas | 67 / 206 / 76 | 5 / **295** / 0 |
| Objeto de 90 cm de fondo | 1 / 80 / 249 | 65 / **183** / 93 |
| Silla con patas | 36 / 261 / 50 | 11 / **289** / 0 |
| Pared 45-65 cm tras el final | 300 / 0 / 0 | 19 / **139** / 269 |
| Dos cajas seguidas | 178 / 13 / 111 | 9 / 42 / 266 |
| Pared que cierra el paso (debe pedir ayuda sin tocar) | 0 / 300 / 157 | 255 / 36 / 300 |

Lectura: la política de la rama de partida quita los esquives y las AYUDA falsas de v1.1 pero no llega a las metas: hay contactos en siete escenarios. Ninguna de las dos versiones pasa las compuertas.

## 4. Pendientes conocidos revisados

- **Micro-giro de 4° que giró 18°.** Confirmado en código: 4° se ejecuta por tiempo (hueco entre los mínimos 4 y 5). El pivote va a 255 sin rampa (~130°/s), la duración es el intervalo entre `RIGHT` y `STOP` por WebSocket (el jitter entra directo al ángulo) y la cola de inercia medida (ANT de 144 a 191 ms a la derecha) son 14 a 25° desde plena velocidad. Se arregla en dos capas: el navegador no hace giros por tiempo con el giroscopio sano, y el firmware gana un modo fino pulso-medir-pulso.
- **`verificar_version.py` necesita teléfono con `--puerto`.** Confirmado (`:136-149`): no hay modo «solo firmware».
- **`servidor_falso` sin `RUMBO=1`.** Confirmado, y la prueba lo fija; hay 12 divergencias más.
- **AYUDA de B6-r5 sin causa.** No determinable (no hubo registro). Hipótesis del banco: causa 1 (los dos lados ocupados 4 s). La caja negra de esta rama la deja diagnosticable.
- **Falsos positivos del banquito.** Hipótesis ordenadas, con la telemetría numérica que las separa:

| # | Hipótesis | Qué se vería en la telemetría de visión |
|---|---|---|
| 1 | Mancha de sol o reflejo de ventana en piso brillante | Celdas marcadas agrupadas, con brillo muy por encima del piso y tono parecido; celdas saturadas (algún canal ≥250); el reflejo se queda en las mismas filas mientras el carro avanza, la mancha de sol baja de fila |
| 2 | Exposición, ganancia o balance de blancos automáticos | El brillo medio (o el cociente R/G, B/G) de TODA la zona cambia entre «Aprender piso» y el falso positivo; los registros del sensor cambian; el histograma de distancia al piso se corre entero |
| 3 | Piso aprendido en otra luz o en otro sitio | La distancia al piso ya es alta en el primer cuadro de la ruta, sin cambio posterior |
| 4 | Sombra (del carro, del operador, de muebles) | Celdas marcadas más oscuras que el piso con el mismo tono; la sombra propia se queda en las filas de abajo |
| 5 | Juntas de loseta | Celdas marcadas en filas o columnas de 1 celda de grueso (caja envolvente ancha y baja) |
| 6 | Umbral mal puesto | Descartada como causa: 240 y 23 desensibilizan |
| 7 | Saturación del modelo de color | Modelo con 0 grupos o motivo «todo no piso» más de 1 s |

## 5. Qué se arregla en esta tanda

| Tema | Decisión |
|---|---|
| Cierre de ruta, obstáculo más allá del destino | Se arregla (núcleo) |
| Geometría segura del rodeo con el pivote real, medida del ancho y del fondo con el láser durante los giros | Se arregla (núcleo) |
| Cono del láser frente al ancho del chasis; patas; vigencia del modelo | Se arregla (núcleo y fusión) |
| Presupuesto por obstáculo; giros por tiempo; `giro_fw_caido` | Se arregla (núcleo) |
| Robustez fotométrica del clasificador y telemetría numérica | Se arregla (núcleo), sin imágenes reales |
| Caja negra por corrida, causa de AYUDA, compuertas de salida | Se arregla (app y puente) |
| Firmware del carro: texto desconocido, bloqueo de `FORWARD`, giro fino, acelerómetro, CAPS | Se arregla y se compila; **no se graba** |
| Firmware de la cámara: `AUTOPRUEBA`, telemetría fotométrica, fijar exposición | Se implementa detrás de bandera apagada y se compila; **no se graba** |
| Servidor falso y `verificar_version.py` | Se arregla |
| Manual (capítulos 4, 5, 6, 8, 9) contra el código | Se corrige lo que toca al modo autónomo |
| Mapa local y planificador (E), hilo de control separado de la interfaz, corte de PWM por temporizador | Diseño y trabajo futuro |

## 6. Cierre: qué pasó con cada decisión (13-nov-2026)

La tabla de arriba es el plan con el que se cerró la auditoría. Esto es lo que quedó de verdad en la rama; el detalle está en [el diseño](autonomo_v2_diseno.md).

| Tema | Resultado |
|---|---|
| Cierre de ruta, obstáculo más allá del destino | Hecho. SIMULADO: 1000 de 1000 |
| Geometría del rodeo, ancho con el láser | Hecho. El fondo no se mide: se cubre con la pista del paso en paralelo y la mirada antes de entrar |
| Cono del láser frente al chasis | Hecho con el vistazo: 998 de 1000 (la partida, 663) |
| Patas de muebles | **Parcial**: 774 y 318 de 1000. Sigue siendo un límite |
| Vigencia del modelo | Hecho (causa 17 y aviso al iniciar) |
| Presupuesto por obstáculo, giros por tiempo, `giro_fw_caido` | Hecho |
| Robustez fotométrica del clasificador | **No se tocó el clasificador.** Los falsos positivos se resolvieron en la decisión (consenso temporal). Queda la telemetría para medir la causa real |
| Caja negra, causa de AYUDA, aviso al iniciar | Hecho; compilado, sin probar en el teléfono |
| Firmware del carro | Hecho y compilado: texto desconocido, bloqueo de `FORWARD`, rearme del freno, `CAPS`. **Giro fino y acelerómetro: solo diseño** |
| Firmware de la cámara | Hecho y compilado: `AUTOPRUEBA` en 0 y telemetría fotométrica. **Fijar exposición: solo diseño** |
| Servidor falso | Hecho. `verificar_version.py` sin modo solo-firmware: pendiente |
| Manual contra el código | Se agregaron secciones de la rama a los capítulos 5, 6, 7, 8, 12, 13 y 15. **Las discrepancias viejas de los capítulos 4, 6 y 9 que encontró la auditoría (F-A6, A-M9) siguen sin corregir** |
| Mapa local, hilo de control aparte, corte de PWM por temporizador | Diseño y trabajo futuro |

Hipótesis más probable de los falsos positivos de visión, sin cambio: mancha de sol o reflejo sobre piso brillante, seguida de un cambio de exposición o de balance de blancos de la cámara con el piso fijo. **NO MEDIDO**: la caja negra y `/stats` de la cámara ya guardan los números para confirmarlo o descartarlo en la primera sesión física.

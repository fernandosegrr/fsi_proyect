# Límites conocidos del modo automático

Este documento resume las limitaciones físicas, de sensores y de diseño del carro en el modo semiautónomo (`nucleo_c/`, `firmware_carro/` y `app_android/`). El sistema opera sin encoders, IMU, lidar ni ultrasonido: toda la navegación depende de estimación por tiempo (*dead reckoning*) y de una cámara VGA reducida a 240×176 (~12 fps).

## 1. Pose solo por tiempo con error acumulado

- **Causa:** Los 4 motores TT con ruedas mecanum no tienen encoders de rueda ni giróscopo (IMU). El navegador (`nucleo_c/navegador.c`, `avanzar_posicion` y `girar_rumbo`) estima `(x_mm, y_mm, rumbo_grados)` integrando únicamente los milisegundos de comando enviados y los parámetros de `nucleo_c/calibracion.c`.
- **Efecto:** Cualquier variación de voltaje de batería, fricción desigual, patinaje de los rodillos mecanum o junta de loseta acumula error de rumbo y posición. Con un error típico de ±5° por giro y ±3 % en velocidad lineal, el error acumulado crece de ~5–10 cm tras 2–3 tramos a decenas de centímetros en rutas largas (> 4–6 m) o con muchos giros.
- **Mitigación actual:** Calibración separada por perfil (`AUTO` y `MANUAL`), balance por lado (`BAL_AUTO`), compensación del desplazamiento del pivote en toda la ruta y frenos de cordura (`NAV_MOTIVO_LOCURA_*`) que detienen el carro si la pose se aparta más de 1 m de la ruta o supera 2.5 veces la distancia planeada.

## 2. Profundidad de 250 mm supuesta para los obstáculos

- **Causa:** Con una sola cámara monocular sin mapa de profundidad denso, el navegador solo ve la cara frontal del obstáculo antes de girar para esquivarlo. Al avanzar en paralelo (`NAV_FASE_PARALELO` en `nucleo_c/navegador.c`), supone un fondo fijo de `NAV_OBSTACULO_PROFUNDO_MM = 250` mm (25 cm) más `NAV_PASO_MARGEN_MM = 100` mm.
- **Efecto:** Si el obstáculo mide bastante más de 25 cm de fondo (por ejemplo, una caja larga puesta a lo largo o un mueble continuo), al intentar reincorporarse a la línea el carro puede encontrarse todavía con el costado del obstáculo.
- **Mitigación actual:** Durante la vuelta a la línea (`NAV_REINCORPORANDO`) el detector sigue activo; si el obstáculo sigue ocupando el camino, el navegador frena y ejecuta otro esquive (hasta `NAV_MAX_ESQUIVADAS = 3`) o pide `AYUDA`.

## 3. Modelo de pivote constante

- **Causa:** El giro se ejecuta moviendo solo un lado (`VELOCIDAD_GIRO = 255`) mientras el otro queda en PWM 0. `cal_desplazamiento_giro` (`nucleo_c/calibracion.c`) modela el desplazamiento del centro del carro con un punto de pivote fijo por sentido de giro y la cuerda del arco $2R\sin(\theta/2)$ a partir de una medición de referencia (por ejemplo, 15 cm adelante y 16.5 cm a la derecha a 90° en `AUTO`, radio ~15.8 cm a la derecha y ~9 cm a la izquierda).
- **Efecto:** En el piso real el centro instantáneo de rotación no es perfectamente constante: depende del ángulo total del giro (la inercia inicial pesa más en giros cortos de 20°–30° que en 90°), del estado de carga de la batería y del agarre de cada rueda mecanum.
- **Mitigación actual:** Medición independiente para izquierda y derecha en la app, compensación del pivote al planificar tramos de ruta y aviso previo si algún sentido de giro sigue con valores estimados sin medir.

## 4. Esquive desde la diagonal

- **Causa:** Cuando el carro sigue un tramo en diagonal (45°) o detecta un segundo obstáculo mientras vuelve en diagonal hacia la línea (`NAV_REINCORPORANDO`), la maniobra de esquive (`NAV_ANGULO_ESQUIVAR = 45°`, avance lateral de `NAV_TRAMO_ESQUIVAR_CM = 40` cm y contra-giro) parte de un rumbo oblicuo respecto a la cuadrícula.
- **Efecto:** El desplazamiento asimétrico de los dos pivotes (apartarse a un lado y enderezarse hacia el otro) combinado con la proyección diagonal acumula más error geométrico que en un tramo ortogonal, y el campo visual de la cámara barre oblicuamente el obstáculo.
- **Mitigación actual:** Reincorporación calculada sobre la polilínea exacta de la ruta (`ubicar_en_linea`, `punto_en_linea`) con seno y coseno del rumbo real en lugar de múltiplos fijos de 45°, y evaluación de las columnas laterales (0 y 9 en filas 7 a 9) durante el esquive y la reincorporación.

## 5. Caja del color del piso (el color manda)

- **Causa:** En `nucleo_c/obstaculos.c` y `nucleo_c/fusion.c`, el modelo de color del piso aprendido al arrancar tiene autoridad sobre el detector general de objetos (`CFG_PESO_PISO`, `fusion_combinar`). Esto evita que los reflejos de la loseta y las juntas disparen falsos positivos constantes.
- **Efecto:** Un obstáculo mate del mismo color, brillo y cromaticidad (`Y, Cb, Cr`) que la loseta (por ejemplo, una caja o zócalo del mismo tono crema/gris claro del piso y sin bordes oscuros marcados) puede clasificarse como piso y producir un falso negativo.
- **Mitigación actual:** El aprendizaje inicial exige que los colores del piso aparezcan en la franja inmediata (filas 8 y 9) y rechaza arrancar si más del 5 % de la zona de referencia se descarta (`"Hay algo frente al carro, despéjalo"`), además de fijar `CFG_PISO_FIJO = 1` durante toda la corrida para no aprender una pared o caja uniforme a mitad de ruta.

## 6. Objetos de menos de 3 celdas

- **Causa:** La imagen de 240×176 se resume en una rejilla de 10×10 celdas (`OBS_FILAS = 10`, `OBS_COLUMNAS = 10`), y el filtro espacial exige al menos `OBS_AREA_MINIMA = 3` celdas conectadas o activas para declarar un obstáculo real (y 6 celdas para frenado inmediato en un solo cuadro de cercanía 3).
- **Efecto:** Objetos muy delgados o pequeños (patas finas de silla o mesa, cables, varillas o topes bajos que cubren solo 1 o 2 celdas de la rejilla) se descartan como ruido puntual o juntas de loseta.
- **Mitigación actual:** Confirmación temporal en 2 cuadros para objetos medianos (3 a 5 celdas) y uso del modo semiautónomo en espacios donde los obstáculos tengan volumen suficiente para cubrir al menos 3 celdas (~6–8 cm de ancho a corta distancia).

## 7. Columnas laterales (0 y 9)

- **Causa:** Las columnas extremas de la rejilla (`c = 0` y `c = 9`, el 10 % izquierdo y derecho de la imagen) sufren mayor viñeteo óptico de la lente GC0308 y ven objetos fuera del ancho físico del carro cuando éste avanza recto. Por ello `obstaculos.c` (`OBS_COL_MIN = 1`, `OBS_COL_MAX = 8`) las excluye del resumen por zonas ordinario.
- **Efecto:** En avance recto, un obstáculo que asome únicamente por el borde extremo de la imagen no frena al carro hasta que entra en las columnas 1 a 8.
- **Mitigación actual:** En `nucleo_c/navegador.c` (`zona_borde_cerca`), las columnas 0 y 9 sí se evalúan en las filas cercanas (7 a 9) únicamente durante `NAV_ESQUIVANDO` y `NAV_REINCORPORANDO` (con el mismo filtro de máscara y fantasma fijo), para cubrir las esquinas del carro al girar y reincorporarse sin elevar los falsos positivos en recta.

## 8. Frenado medido (9 a 14 cm en perfil AUTO)

- **Causa:** Aunque el deslizamiento mecánico puro al cortar PWM en perfil `AUTO` (210, ~19.3 cm/s) es de ~3 cm (`CAL_AUTO_FRENADO_MM = 30`), la distancia total desde que el obstáculo entra en una fila de la cámara hasta que el carro queda quieto incluye la exposición y envío JPEG (~80 ms a 12 fps), WiFi + decodificación + JNI + tick de 50 ms + WebSocket (`latencia_visual_ms` ~200–300 ms, más un segundo cuadro si requiere confirmación).
- **Efecto:** En pruebas reales en perfil `AUTO`, el carro recorre entre **9 y 14 cm** desde que el obstáculo cruza el umbral visual hasta detenerse por completo (y superaría los 20 cm en perfil `MANUAL` a 38 cm/s, razón por la cual las rutas usan `PERFIL_AUTO`). Si un obstáculo aparece de golpe en la fila 9 (< 20 cm), el margen físico es mínimo.
- **Mitigación actual:** Ejecución obligatoria en `PERFIL_AUTO`, frenado en el primer cuadro cuando el obstáculo cercano (cercanía 3) cubre 6 o más celdas, y descuento de `frenado - arranque` una sola vez en la pose al detenerse.

## 9. Watchdog de 1 s (unos 20 cm en perfil AUTO)

- **Causa:** El firmware del ESP32 (`firmware_carro/config.h`, `TIEMPO_WATCHDOG_MS = 1000`) apaga los motores si pasan 1000 ms sin recibir un comando real por WebSocket (`PING` no renueva este reloj). Por su parte, el navegador en la app corta por falta de video tras `NAV_MAX_HUECO_VIDEO_MS = 700` ms.
- **Efecto:** Si el video se congela pero el WiFi del carro sigue conectado, el navegador frena a los 700 ms (~13.5 cm recorridos a ciegas a 19.3 cm/s). Si el teléfono pierde por completo el enlace WiFi con el ESP32 en pleno avance, el carro sigue avanzando durante 1 s completo antes de que actúe el watchdog del firmware, recorriendo **unos 20 cm** en perfil `AUTO` (y hasta ~38 cm si estuviera en `MANUAL`).
- **Mitigación actual:** `PING` excluido de la renovación del watchdog en `carro.c`, parada inmediata al desconectarse el cliente WebSocket activo (`WS_EVT_DISCONNECT`) y corte a `NAV_ERROR` con `STOP` al superar 700 ms sin cuadro nuevo.

## 10. Cámara con tono rosado y viñeteo

- **Causa:** El módulo ESP32-WROVER con sensor GC0308 entrega imágenes JPEG 240×176 con una dominante cálida/rosada en interiores, oscurecimiento pronunciado en las esquinas (viñeteo, especialmente en la zona izquierda) y saturación por reflejos especulares en pisos de loseta clara.
- **Efecto:** El viñeteo y los artefactos fijos de la lente pueden hacer que ciertas celdas de borde o esquina difieran siempre del color central del piso y aparezcan como un «obstáculo fantasma» fijo desde el primer cuadro; a la inversa, un reflejo intenso puede romper el color de una baldosa.
- **Mitigación actual:** Calibración de máscara de visión en 3 lugares distintos (`nucleo_c/mascara_vision.c`, que rechaza activarse si sale sospechosa), detección de fantasma fijo al arrancar más sondeo de 4 cm con firma hexadecimal persistente durante toda la corrida (tope de 12 celdas por zona y cancelación si crece más de 30 %) y tolerancia ampliada al brillo en `obstaculos.c`.

## 11. Giroscopio y láser: qué corrigen y qué no (J11)

- **Error esperado con y sin giroscopio** (simulación, 400 rutas en L de Monte Carlo, radios de pivote de 6 a 20 cm, el carro gira ±10 % de lo que cree el navegador; mismas semillas en las dos vueltas):

  | | error final medio | percentil 95 |
  |---|---|---|
  | **Sin giroscopio** (giros por tiempo) | **153 mm** | **378 mm** |
  | **Con giroscopio** (giros por ángulo medido) | **45 mm** | **116 mm** |

  Es un simulador con un modelo mío del firmware: el giroscopio simulado tiene error de escala de ±2 %, sesgo de ±0.3 °/s sin corregir y ruido de ±1° por giro, y el carro simulado frena sin inercia desconocida. **No son medidas del carro real**: la prueba 6 de `docs/validacion_sensores.md` dará los números reales. La mejora viene de que el rumbo ya no depende del tiempo de cada giro; la distancia sigue siendo por tiempo (y eso no cambia).
- **El giroscopio no corrige la distancia** ni lo que el carro se corre de lado por el pivote: se compensa con el modelo de pivote, igual que antes (ahora calculado con el ángulo REAL). Un patinazo (rueda que resbala) hace que el giroscopio mida un giro que no movió el carro.
- **Giros que no van por ángulo**: menos de 5° (no se hacen con giroscopio; los de 4° del plan se hacen por tiempo), más de 200° (el firmware no los acepta; el de deshacer una esquivada de 225° va por tiempo) y los manuales («toma el control»).
- **Si el firmware no contesta a un giro** (J12): el navegador pregunta `IMU?` y compara el ángulo contra el del inicio (80 % o más de lo pedido: no repite; menos: repite solo lo que falta). Solo se duplicaría un giro si no hay ningún dato del giroscopio (ni `SENS` ni `IMU?`), y entonces cae al método por tiempo.
- **El láser ve un punto, no un plano**: solo mira recto al frente, a unos 6-8 cm del piso. Un obstáculo bajo (menos de ~6 cm), alto desplazado a un lado o un objeto muy delgado (pata de silla) puede pasar sin que lo vea; la cámara sigue siendo la que decide. Un piso brillante o un objeto negro/transparente puede dar lecturas raras (de ahí el filtro de 2 lecturas y el valor «libre»).
- **Freno de emergencia a 12 cm**: con el frenado medido del carro (9 a 14 cm en AUTO) un obstáculo que aparece de golpe a 12 cm igual se toca; el freno reduce el golpe, no lo elimina.
- **Con el interruptor del láser apagado el firmware también deja de frenar** (`TOF_USO:0`): es el comportamiento de antes.

## 12. Ajustes de J12 (anticipación, láser, calibración con giroscopio)

- **Anticipación del giro:** aprende por sentido y por perfil con la mediana de los últimos 5 giros de 30° o más. Con motores muy asimétricos la simulación llega a ±3° en 9 de 10 (izquierda 10/10, derecha 9/10). **En el carro real no está verificado**; hay que dejarla aprender con ~4 giros por lado antes de juzgarla (`ANT?`). Con mucha inercia el tramo final después de que el giroscopio informa no se ve (≈1.5° en la simulación).
- **Láser:** reintenta cada 5 s, pero la inicialización de la librería se hace solo con el carro quieto y espera al sensor unas decenas de ms dentro de `loop()`; el tiempo real no se ha medido en el carro. Si más de 1 de cada 5 arranques necesita reintentos la app avisa: casi siempre es cableado o soldadura, XSHUT flotando o pull-ups débiles.
- **Calibración del giro con el giroscopio:** solo ms por grado y frenado; el desplazamiento del pivote sigue manual. Con 3 giros por lado el frenado sale con menos precisión que los ms por grado. Si el piso o las pilas cambian entre giros, los puntos se alejan de la recta y la prueba se rechaza como inconsistente. Probada solo con el carro simulado.
# J13 (20 oct): límites que siguen vigentes

El giroscopio ahora reintenta el sesgo tras motivo 4 con motores detenidos; no elimina la variación de pilas, agarre ni desplazamiento del pivote. La calibración usa cuatro giros por lado, excluye calentamiento y permite descartar un válido con dos puntos físicamente admisibles. Dos puntos no demuestran repetibilidad; hay que comprobarla con el carro.

La autoridad opcional del láser decide a 400 mm solo en columnas 3 a 6. En J14 se probó con el carro real en B4 (recta de 150 cm), B5 (ruta en L) y B6 (caja a ~100 cm): con ella encendida no hubo falsos paros en piso vacío ni toques a la caja en las corridas 3 y 4, pero no se completó la comparación pareada B7/B8 ni el corte de WiFi B9. El dueño decidió dejarla siempre encendida; el valor por defecto de la app ahora es true (el núcleo en C conserva 0 por defecto para sus pruebas). Requiere lectura válida y fresca; sin ella vuelve a la cámara. El láser sigue viendo un punto: habilitarlo puede ignorar un objeto central que la cámara ve pero queda fuera del haz. La cámara conserva la elección del lado y los obstáculos fuera del carril. TOF_PARO del firmware se conserva a 120 mm, independiente del nuevo umbral de navegación.

J10c resuelto en simulación en J14 (no se probó con una caja ancha real): se refinó la regla en obstaculos_aprender_terminar. Un segundo color de piso solo se acepta como franja continua si ninguna de sus muestras toca las filas cercanas al carro (filas >= 7 de 10). Compromisos y riesgos conocidos:
- Un obstáculo muy ancho (como una pared o caja transversal) situado a más de 50 cm (filas 4 a 6) que abarque todo el ancho podría seguir confundiéndose con un tapete lejano durante el aprendizaje. Acercarse no garantiza que el detector de color lo recupere: su color podría haber quedado aprendido como piso. El láser podría detectarlo si está habilitado y su haz alcanza la caja; falta comprobarlo con el carro.
- Un tapete legítimo que empiece exactamente en la fila 7 (a unos 35-45 cm del carro) sin tocar las filas 8 y 9 no se aceptará como segundo piso (dará descartado > 5 % y muy_variado = 1), exigiendo colocar el carro con las filas cercanas despejadas o completamente sobre el tapete. El modelo de objetos queda encendido por defecto (ConfigVision.usarModelo = true, con fallback automático al detector de color si falla al abrir).

## 13. Tiempos del loop del firmware (mediciones J13)

- Mediciones reales observadas con LOOP? en el ESP32:
  * Reposo sin cliente WiFi conectado: 1.7 ms (1684 a 1717 us a 60 s, 0 excesos de 5 ms).
  * En el aire con motores encendidos: 3.25 ms (3251 us, 0 excesos de 5 ms en 36 s con cinco giros y 9 s de motores).
  * App conectada (reposo o marcha con giros): 6.8 a 8.4 ms (8431 us en reposo con 2 excesos por minuto; 6775 a 6783 us en piso con 2 a 3 excesos).
  * Arranque inicial del sistema: 12.8 a 12.9 ms (pico transitorio único de inicialización con 1 exceso, observado en 12856, 12858 y 12935 us).
- Hipótesis de tráfico de la app: los mensajes periódicos y la serialización WebSocket podrían explicar el incremento con la app conectada. En el aire con motores y sensores se observaron 3.25 ms sin excesos, pero esas mediciones no aíslan todas las causas; la hipótesis no está demostrada.
- Impacto observado en giros: los giros por ángulo validados en J13 no mostraron una afectación atribuible a esos picos. Esto no demuestra que ninguna condición de tráfico afecte la precisión; sigue pendiente medirla en las rutas reales.

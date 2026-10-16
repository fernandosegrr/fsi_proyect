# Navegador del modo semiautónomo

Código: `nucleo_c/navegador.h` y `navegador.c` (C estándar, solo enteros, sin `malloc`, sin
recursión). Pruebas: `nucleo_c/pruebas_navegador.c` con un carro simulado
(`simulador_navegador.c/.h`). Se usa desde la app por JNI (`puente.c`, `NavegadorNativa.kt`) y el
ciclo de 50 ms está en `ModoSemiautonomo.kt`.

## Qué hace

Une dos módulos: `trayectoria.c` (de una ruta de celdas saca pasos AVANZAR/GIRAR con su tiempo) y
`obstaculos.c` (dice si hay algo adelante). Cada ~50 ms se llama a
`navegador_tick(estado, ahora_ms, resultado_obstaculos)` y devuelve **un comando para mantener**:
`FORWARD`, `REVERSE`, `LEFT`, `RIGHT` o `STOP` (los mismos del protocolo real, sin parámetros).
El navegador no tiene sensores de posición: lleva una **posición estimada por tiempo**
(x, y en milímetros y rumbo en grados) contando cuánto tiempo estuvo avanzando o girando con la
calibración de `nucleo_c/calibracion.h`.

## Estados y transiciones

```
                     comenzar()                       llegó al último punto
   ESPERANDO ---------------------> SIGUIENDO ------------------------------> TERMINADO
       ^                              |    ^
       |                 obstáculo al |    | el obstáculo desaparece
       |                 centro N     |    | (N cuadros libres seguidos)
       |                 cuadros      v    |
       |                       DETENIDO_OBSTACULO
       |                              |   \
       |        tras la pausa, un     |    \ ambos lados bloqueados durante
       |        lado libre            |     \ NAV_TIMEOUT_ESQUIVAR_MS, o ya
       |                              v      \ esquivó NAV_MAX_ESQUIVADAS veces
       |                         ESQUIVANDO    \
       |        gira, avanza de lado,  |        \
       |        gira de vuelta         v         v
       |                       REINCORPORANDO   ERROR
       |                              |
       |                              +--> (replanifica desde la posición estimada) --> SIGUIENDO
       |
       +---- navegador_cancelar() desde cualquier estado: STOP inmediato

   En ESPERANDO, TERMINADO y ERROR el comando es SIEMPRE STOP (regla de seguridad).
```

- **SIGUIENDO**: va punto por punto de la ruta. Para cada punto planifica (con `trayectoria.c`) desde la celda
  más cercana a la posición estimada y ejecuta los pasos por tiempo. Un paso termina cuando se ejecutó su
  tiempo (redondeado a medio tick). Solo mira obstáculos mientras avanza (FORWARD), no mientras gira.
- **DETENIDO_OBSTACULO**: STOP. Si el frente se libera (N cuadros seguidos) sigue donde iba. Si no, tras
  `NAV_PAUSA_MS` mira los lados con el último cuadro: si IZQUIERDA está libre esquiva por ahí; si no, por
  la DERECHA; si ninguno, espera hasta `NAV_TIMEOUT_ESQUIVAR_MS` y entonces ERROR.
- **ESQUIVANDO**: gira `NAV_ANGULO_ESQUIVAR` grados hacia el lado libre, avanza `NAV_TRAMO_ESQUIVAR_CM` de
  lado y gira de vuelta. Si mientras avanza de lado aparece un obstáculo MUY cerca (cercanía 3) al frente:
  ERROR.
- **REINCORPORANDO**: vuelve a la línea de la ruta con la pose estimada; ver «Volver a la línea» más abajo (ya no es un solo tick: gira hacia la línea,
  avanza, gira al rumbo de la ruta, con la pausa de 300 ms entre cada movimiento). Máximo `NAV_MAX_ESQUIVADAS` esquivadas por ruta; la siguiente detención da ERROR.
- **Cancelar** deja ESPERANDO; con `navegador_comenzar` se puede retomar desde la posición estimada.

## Volver a la línea (REINCORPORANDO)

**Qué fallaba (prueba real en loseta con tijeras de obstáculo):** después de esquivar, el carro no volvía a la ruta: giraba y se iba derecho hacia un lado. El
navegador replanificaba desde la *celda* más cercana con el rumbo redondeado a 45°, así que cualquier error de giro (por ejemplo 110° por 90°) y los 40 cm que ya
se había apartado se perdían; además la posición estimada solo se movía en múltiplos de 45°, de modo que un avance en diagonal suave no cambiaba la x.

**Cómo funciona ahora** (`navegador.c`, funciones `reinc_*`, `ubicar_en_linea`, `punto_en_linea`; constantes `NAV_REINC_*` en `navegador.h`):

1. **Pose estimada** (x, y, rumbo) por tiempos de la calibración, actualizada con cada avance, giro, retroceso y frenado, también durante el esquive. La posición
   avanza ahora con el seno y el coseno del rumbo exacto (antes se redondeaba a 45°).
2. **Pasar el obstáculo.** Al terminar de apartarse y enderezarse (`VOLVER`), el carro sigue **paralelo** a la línea (`NAV_FASE_PARALELO`) hasta haber pasado el
   obstáculo: desde el punto de salida, la distancia hasta la cara del obstáculo (según la cercanía con la que frenó: 300, 500 u 800 mm, los límites de las bandas
   del detector) más `NAV_OBSTACULO_PROFUNDO_MM` (250: una caja de ~20 a 25 cm) más `NAV_PASO_MARGEN_MM` (100). Sin este tramo el carro volvía a la línea justo
   al lado del obstáculo y el camino de vuelta lo cruzaba o quedaba otra vez de frente (ver «Simulador»). Nunca llega a menos de 5 cm del final de la ruta.
3. **Punto de la línea.** Es el punto de la línea de la ruta (la línea quebrada de todos los puntos, así que vale también en una curva) a `NAV_REINC_ADELANTE_CM` = **30 cm**
   por delante de ese punto de salida. Si ya está a menos de 8 cm de la línea (`NAV_REINC_DESVIO_MIN_MM`) **solo corrige el rumbo** y sigue.
4. **Giro mínimo hacia el punto** (de -179° a 180°), **avanza la distancia estimada** y al llegar **gira al rumbo original de la línea** en ese punto. Un giro de menos de
   4° no se hace (el error de la estimación es mayor). **Nunca avanza recto sin reorientarse**, y hay `NAV_PAUSA_ASENTAR_MS` = 300 ms de STOP entre cada movimiento (y al
   empezar, para que la pose incluya el frenado). Perfil AUTO, igual que siempre.
5. Si el obstáculo bloquea ese tramo (o el carril paralelo), el carro se detiene; si el frente se libera sigue la vuelta recalculada y si no, **repite el esquive**
   (con los reintentos de siempre, y contando en `NAV_MAX_ESQUIVADAS`).
6. Al terminar: se dirige al siguiente punto de la ruta que queda por delante del punto de la línea, y sigue con SIGUIENDO.

**Registro `REINC_DIAG`** (la app lo copia al log con ese nombre; solo números): `plan` (pose antes, punto de salida, desviación, punto objetivo, distancia, giro1,
giro2, rumbo de la línea y esquivadas), `pasar` (cuánto avanza en paralelo) y `fin` (pose después y su desviación de la línea). Se lee con `navegador_diag_siguiente`.

**Pruebas en C** (`pruebas_navegador.c`, `pruebas_reincorporacion`): obstáculo al centro, a la izquierda, a la derecha, en curva (cerca de la esquina del cuadrado) y
con reintento; en todos la pose REAL del carro simulado queda a 10 cm o menos de la línea, la estimada coincide con la real, gira antes de avanzar y hay 300 ms de pausa;
desviación menor de 8 cm (solo corrige el rumbo); tramo de vuelta bloqueado (repite el esquive) o bloqueado un momento (retoma la vuelta); obstáculo que reaparece (termina en
`MAX_ESQUIVADAS`, con la causa concreta).

**Qué NO resuelve:** si el giro real se pasa de lo que cree el navegador (110° por 90°) la pose estimada se equivoca igual; eso se corrige calibrando el giro de 90°
(ver `docs/calibracion.md`). El obstáculo se supone de unos 20 a 25 cm de fondo; uno más largo o una pared necesitan otra esquivada.

## Falso «otro obstáculo mientras esquivaba»

En dos pruebas reales con mucho espacio libre (tijeras; luego patas de silla lejanas) la ruta terminó en ERROR al primer aviso, con 1 esquive hecho y el punto 0 de 7.
Las hipótesis eran: (1) el mismo obstáculo sigue visible en el borde mientras gira y cuenta como nuevo; (2) al girar la cámara barre la escena y objetos lejanos u oscuros
(patas, sombras, juntas de loseta) pasan por las filas cercanas; (3) el umbral durante el esquive era más estricto que fuera de él (un solo cuadro de cercanía 3 al centro
abortaba). Sin el log de esa prueba no se puede saber cuál fue, así que se corrigieron las tres y se agregó el registro para confirmarlo:

- **Durante un giro** de la maniobra (apartarse, enderezarse o los giros de la vuelta) las detecciones se **ignoran**; después del giro también, hasta que hayan llegado
  `NAV_ESQ_CUADROS_ESTABLES` = 2 cuadros o pasado `NAV_ESQ_MS_ESTABLE` = 150 ms (lo que ocurra primero).
- Después solo **bloquea** un obstáculo de la zona **central** con cercanía **2 o 3 confirmada** (`MS_CONFIRMAR_LEJOS` = 150 ms seguidos y al menos 2 cuadros). Uno lateral o lejano
  se anota pero no aborta; ni siquiera con cercanía 3 hace falta (ya no basta un cuadro suelto).
- Cuando sí bloquea mientras avanza de lado: **reintento** de siempre (retrocede 15 cm, gira 45° y luego 90° más del mismo lado). Solo falla al agotar `NAV_MAX_REINTENTOS` = 2.
- Mientras pasa en paralelo o vuelve a la línea y el centro se bloquea: se detiene y esquiva otra vez (o retoma la vuelta si el frente se libera).
- **El mensaje de error dice la causa concreta** (`navegador_texto_error`): «se agotaron los 2 reintentos, el obstáculo seguía en el centro», «ya esquivó 3 veces y el obstáculo
  seguía en el camino», «los dos lados estuvieron ocupados 4 segundos y no había por dónde esquivar»...
- **`ESQUIVE_DIAG`** (log): una línea cuando cambia algo (o cada 500 ms) con la fase, la cercanía, la zona, las celdas, las filas y columnas que dispararon (con la cámara real; en el
  simulador no hay imagen y salen como `?`), los ms desde el inicio del giro, los cuadros desde que terminó el giro y la acción: `ignorada_girando`, `ignorada_espera_estable`,
  `centro_cuenta`, `lateral_no_bloquea`, `lejos_no_bloquea`, `BLOQUEA_reintento` o `BLOQUEA_se_detiene`.
- Fuera del esquive **no cambia nada**: un obstáculo cercano de frente sigue frenando con el primer cuadro (probado: el STOP sale a los 1500 ms).
- **Cambio de comportamiento:** un obstáculo central permanente de cercanía 2 que antes se esquivaba sin verlo hasta el máximo de esquivadas ahora agota los 2 reintentos
  y termina en ERROR (`NAV_MOTIVO_ESQUIVANDO`). El caso `MAX_ESQUIVADAS` se prueba con un obstáculo que reaparece al volver a la línea.

## Simulador: por qué terminaba en «No llegó» con un obstáculo de una celda

**Investigación.** Con un obstáculo de una celda (20 cm) en una recta de 1.4 m el simulador terminaba en «No llegó» (3 esquivadas, 116 cm de error). Se siguió el recorrido:
la cámara virtual lo veía bien (zona central, cercanía 2 a 50 cm; ver `sr_vista`) y el carro frenaba y se apartaba como debía. El fallo estaba **en el navegador, no
en el modelo de la cámara ni en la pantalla**: tras apartarse, el navegador volvía en diagonal a la celda siguiente de la ruta, que estaba **antes** del obstáculo; en
cuanto quedaba de frente a él otra vez frenaba y esquivaba de nuevo, y a la tercera se acababan las esquivadas. Es el mismo defecto que en la prueba real («tras esquivar no
vuelve a la línea de la ruta»).

**Corrección:** pasar el obstáculo en paralelo y volver a la línea 30 cm por delante (ver arriba). Resultado en la simulación: la caja de 20 cm en cualquier fila de 1 a 5, sobre
la línea o a 20 cm, **llega** con 1 esquivada y a menos de 7 cm del último punto; también una caja de 40 cm de ancho (dos celdas) en las filas 1 a 4 (`pruebas_sim_ruta.c`).
Sigue sin llegar un obstáculo pegado al punto de partida (a 20 cm del carro): no hay espacio para apartarse y los dos lados quedan ocupados; ahora la pantalla lo dice.

**Obstáculo virtual por defecto:** cada celda que se toca en la pantalla Rutas es una **caja de ~20 cm** (una celda de la cuadrícula). La cámara virtual ya no la ve como
1 a 10 «celdas» según cuánto cae en una zona: ahora ocupa **varias celdas del detector** según su tamaño aparente (más cuanto más cerca; a 30 cm de su cara unas 80 celdas, a 70 cm
unas 30), como la ve el detector de verdad (`MIN_CELDAS` = 7).

**Cuando no llega, la pantalla dice por qué** (`sr_texto_causa`): «El obstáculo bloqueó el camino y se agotaron los 2 reintentos», «…se agotaron las 3 esquivadas», «No había por
dónde esquivar: los dos lados estaban ocupados», «No encontró cómo volver a la ruta» o «La simulación se cortó por tardar demasiado».

## Parámetros a calibrar con el carro

Todos están en `nucleo_c/navegador.h` (y `calibracion.h`, que también usa el navegador).

- **Calibración del carro** (`CalibracionCarro`, ver `docs/calibracion.md`): de ella depende toda la posición estimada. El
  navegador ya no usa `MS_POR_CM` y `MS_POR_GRADO` fijos: usa la calibración MEDIDA (arranque, ms por cm, frenado, y ms por
  grado de la izquierda y de la derecha por separado), que se le da con `navegador_poner_calibracion` y se mide con el
  asistente de la app. Si no se calibró, usa los valores por defecto de `calibracion.h` (`MS_POR_CM` 50 y
  `MS_POR_GRADO` 10) y la pantalla de control avisa. Hay que medirla con el carro real, con batería cargada, en el piso
  real, y repetirla si cambia el peso o el piso. Al encadenar tramos rectos no se repite el arranque ni se descuenta el
  frenado (solo cuenta al detenerse o girar), y al reanudar tras una detención por obstáculo se compensa.
- **Antirrebote en milisegundos** (no en cuadros: así el retraso es el mismo con 8, 12 o 30 cuadros por
  segundo). Usa el reloj `ahora_ms` que ya recibe `navegador_tick`:
  - `NAV_CERCANIA_MIN_DETENER` = 2: desde qué cercanía del detector (0 a 3), en **cualquier zona**, se considera el
    frente bloqueado. Un obstáculo de cercanía 1 (lejos de verdad, por ejemplo una escalera al fondo) solo se informa: no frena.
  - **Confirmación inmediata con UN solo cuadro**: cercanía 3 en cualquier zona, o cercanía 2 en la zona central.
  - `MS_CONFIRMAR_LEJOS` = 150: un obstáculo de cercanía 2 en una zona de los lados debe verse 150 ms seguidos y en al menos 2
    cuadros antes de detenerse. Con cuadros cada 80 ms se confirma al tercero (160 ms); cada 120 ms, a los 240 ms.
  - `MS_CONFIRMAR_LIBRE` = 250: el frente debe verse libre 250 ms seguidos (2 cuadros como mínimo) para
    reanudar tras una detención.
  - Las zonas de los lados también sirven para elegir de qué lado esquivar (el lado libre; si ambos están libres, el izquierdo).
- **Giro corregido con la cámara** (`navegador_aporte_visual`, ver `docs/giro_visual.md`): un giro se corta cuando la cámara ve el ángulo pedido
  (no antes de `NAV_GIRO_MIN_PCT` = 50 % del tiempo planeado; respaldo a `NAV_GIRO_MAX_PCT` = 150 %), si la cámara cubrió al menos
  `NAV_COBERTURA_VISUAL_PCT` = 50 % del giro; si no tiene confianza, el giro sigue por tiempo. `navegador_poner_latencia_visual` compensa el retraso de la cámara.
- `NAV_PAUSA_MS` = 500: pausa antes de decidir. `NAV_TIMEOUT_ESQUIVAR_MS` = 4000: cuánto espera con los dos
  lados bloqueados.
- `NAV_REINC_ADELANTE_CM` = 30, `NAV_REINC_DESVIO_MIN_MM` = 80, `NAV_CARA_CERCANIA_x_MM` (300, 500, 800), `NAV_OBSTACULO_PROFUNDO_MM` = 250 y `NAV_PASO_MARGEN_MM` = 100: la vuelta a la línea
  (las distancias a la cara del obstáculo son las del modelo de la cámara virtual; con la cámara real conviene medirlas).
- `NAV_ESQ_CUADROS_ESTABLES` = 2, `NAV_ESQ_MS_ESTABLE` = 150 y `NAV_ESQ_CERCANIA_BLOQUEA` = 2: qué se ignora y qué bloquea durante la esquivada.
- `NAV_ANGULO_ESQUIVAR` = 90 grados y `NAV_TRAMO_ESQUIVAR_CM` = 40 cm: cuánto se aparta. Hay que
  ajustarlos al ancho del carro y de los obstáculos típicos (40 cm de lado con celdas de 20 cm).
- `NAV_MAX_ESQUIVADAS` = 3, `NAV_TICK_MS` = 50 y `NAV_DT_MAX_MS` = 1000 (el firmware frena a los 1000 ms
  sin comandos, así que no tiene sentido contar más movimiento que eso entre dos ticks).

## Pruebas hechas

- **En PC**: `cd nucleo_c; make test` corre las pruebas de los tres módulos: 28 de trayectoria, 32 de
  obstáculos y **96 del navegador**, todas pasan (y 110 de calibración, 57 del piso, 67 de la fusión y 60 del movimiento visual). Cubren: rutas sin obstáculos (recta, cuadrado, zigzag,
  diagonal, media vuelta), ruta vacía, un solo punto y ruta inválida, antirrebote en ms (un cuadro de ruido a un lado no
  detiene; un cuadro con obstáculo cerca sí, al instante; a un lado permanente a los 150 ms; con cuadros cada 50, 80
  y 120 ms; intermitente; reanudar tras 250 ms libre), obstáculo lejano (cercanía 1) que no frena y obstáculos a los lados, obstáculo que desaparece, esquivar por la
  izquierda y por la derecha y reincorporarse, ambos lados bloqueados, obstáculo permanente (termina en ERROR),
  cancelar a mitad y durante ESQUIVANDO, reanudar tras cancelar, ninguna orden de movimiento tras ERROR o
  TERMINADO, y una deriva del 5 %. Con cambios deliberados al código (antirrebote, regla de reanudar, conteo
  del tiempo de cada paso) las pruebas fallan como deben.
- **Retraso en PC**: con el obstáculo cerca (cercanía 2 o 3 al centro) el STOP sale en el mismo tick del
  primer cuadro (0 ms de antirrebote), con cuadros cada 50, 80, 100 o 120 ms; con un obstáculo de cercanía 2 a un lado, a los 150 ms
  (cuadros cada 50 ms), 160 ms (cada 80 ms) y 240 ms (cada 120 ms). Las medidas con el Huawei de más abajo son
  del antirrebote anterior (3 cuadros) y no se repitieron. La cadena completa está en `docs/latencia.md`.
- **En el Huawei con el servidor falso** (`adb reverse`; obstáculos simulados con botones, no hay cámara):
  - Cuadrado de 40 cm de lado: `FORWARD` unos 2 s, `RIGHT` unos 0,9 s, y así cuatro lados; al final `STOP`
    (TERMINADO). Zigzag: `FORWARD`/`RIGHT`/`FORWARD`/`LEFT`... y `STOP` final.
  - Obstáculo simulado al centro a mitad de la ruta, con el lado izquierdo libre. Secuencia en el servidor
    (se quitan los comandos repetidos del heartbeat):

    ```
    14:11:28.752  FORWARD            (avanzando por la ruta)
    14:11:30.084  STOP               (obstáculo detectado)
    14:11:30.601  LEFT               (0,5 s después: pausa y gira hacia el lado libre)
    14:11:31.481  FORWARD            (0,9 s de giro; avanza 2 s de lado)
    14:11:33.481  RIGHT              (gira de vuelta)
    14:11:34.356  STOP               (REINCORPORANDO: un tick)
    14:11:34.466  RIGHT              (replanifica y continúa la ruta)
    14:11:34.910  FORWARD  ...       (sigue hasta el final)
    14:11:45.469  STOP               (TERMINADO)
    ```
  - **Retraso obstáculo → STOP** (8 repeticiones, por cable USB): la app envía el STOP **107 a 140 ms** después
    de poner el obstáculo simulado (mediana 128 ms), y el servidor lo recibe 108 a 142 ms después (mediana
    131 ms; el trayecto de ida son unos 2 ms). Son N-1 = 2 cuadros de 50 ms más la espera hasta el siguiente tick.
    Con una cámara real de pocos cuadros por segundo el retraso sería mayor: con 5 cuadros por segundo, de
    unos 400 a 600 ms.
  - **Cancelar** a mitad de la ruta: STOP inmediato y no se envía nada más.
  - **Segundo plano** (HOME) a mitad de la ruta: se cancela la ruta, se envía STOP y se cierra el socket.
  - **El servidor se cae** a mitad de la ruta: la app cancela la ruta (no hay a quién mandarle comandos),
    pasa a Reconectando y reintenta cada 2 s. El carro real frenaría solo por su watchdog de 1000 ms.

## Límites conocidos

- **La posición por tiempo se desvía.** El navegador no sabe si el carro realmente avanzó. Con la batería
  baja el carro avanza y gira menos por segundo; en una prueba simulada, un giro 5 % más corto dejó al carro
  unos 9 cm fuera de lugar tras un cuadrado de 80 cm de recorrido, y el navegador seguía "convencido" de haber
  llegado. Un 5 % menos de avance en una recta de 1 m deja al carro 5 cm antes.
- **Deslizamiento y giros de un solo motor.** La estimación supone que girar no mueve el carro del sitio, pero
  un pivote con un solo motor lo desplaza y las ruedas patinan según el piso. Hay que medirlo.
- **El tiempo real de cada comando depende de la red.** El firmware no recibe duraciones, solo comandos que se
  mantienen mientras llegan (heartbeat de 250 ms); un retraso o un salto del WiFi alarga o acorta un giro.
- **El rumbo se ajusta a múltiplos de 45°** solo al replanificar los pasos de la ruta (`planificar`); la posición estimada ya se mueve en CUALQUIER rumbo (tabla
  de senos por grado), y la vuelta a la línea sí usa los ángulos exactos. Como no hay mapa, el camino de regreso puede volver a chocar con el mismo obstáculo (por eso
  el máximo de esquivadas).
- **Esquivar es "a ciegas"**: mientras gira y avanza de lado solo vigila el frente muy cercano; no hay mapa.
- **REVERSE no se usa**: el navegador nunca retrocede (solo está en la estimación de posición).
- La **cercanía** del detector sale de la fila de la imagen, no de centímetros; hay que relacionarla con la
  distancia real de frenado del carro.

## Qué NO se probó

- Con el **carro real**: movimiento, calibración de `MS_POR_CM` y `MS_POR_GRADO`, deriva real, patinaje.
- Por **WiFi real** (192.168.4.1): solo se probó con el servidor falso por cable USB.
- Con una **cámara real** y el detector de obstáculos conectado al navegador: los obstáculos fueron botones
  en pantalla (cercanía fija en 3). Tampoco se combinó con el modelo de objetos.
- Las rutas en diagonal y los giros de 180° solo se probaron en PC.
- Qué pasa cuando llegan cuadros de cámara a un ritmo irregular.

## Cómo usarlo

- **C**: `navegador_iniciar` → `navegador_cargar_ruta` → `navegador_comenzar` y `navegador_tick` cada 50 ms.
- **Kotlin**: `NavegadorNativa.cargarRuta/comenzar/tick/cancelar/estado` (ver `NavegadorNativa.kt`).
- **App**: Control del carro → "Modo: Manual (tocar para Semiautónomo)" → elegir ruta → Iniciar. Con los tres
  botones de "Simular obstáculo" se pone o quita un obstáculo en cada zona. Cancelar o mandar la app a segundo
  plano envía STOP.
- **Probar sin carro**: servidor falso y `adb reverse` (ver `docs/pruebas_control.md`).

## Pausa entre movimientos

Entre avanzar y girar (o girar y avanzar) el navegador manda STOP `NAV_PAUSA_ASENTAR_MS` = 300 ms antes del siguiente comando. Así cada movimiento arranca desde cero, igual que en la calibración, y no hereda la inercia del anterior. Con el carro real, sin esa pausa el giro de las esquinas variaba 30 a 60 grados entre corridas de un cuadrado. Entre dos tramos rectos seguidos no hay pausa. La pausa no cuenta como tiempo del paso: la posición estimada sigue siendo por tiempo de movimiento.

## Objetos grandes: por qué terminaba en ERROR y cómo reintenta ahora

### El caso que fallaba (la mochila)

Con un obstáculo grande y cerca, el carro se detenía, giraba 90° para apartarse y **mientras avanzaba de lado** (fase 1) volvía a ver el
objeto al frente con cercanía 3. El código antiguo (`tick_esquivando`) lo trataba como "apareció algo nuevo" y pasaba a ERROR
(`NAV_MOTIVO_ESQUIVANDO`) con un solo cuadro. Había **dos causas distintas** que daban el mismo síntoma:

1. **El cuadro viejo.** Con el video del carro el cuadro llega ~320 ms tarde. Justo al terminar el giro el navegador todavía recibía cuadros
   de cuando el carro giraba (el objeto aún al frente) y los tomaba como si fueran del carro ya apuntando a otro lado. Como la cercanía 3 se
   confirma con UN cuadro, bastaba uno para el ERROR. Este caso **no es un obstáculo real**: es una falsa alarma por el retraso.
2. **El objeto sigue ahí de verdad.** Si el objeto es muy ancho (la mochila, una pared, una esquina) o el carro está muy cerca de él, tras
   girar 90° el frente SIGUE ocupado: el carro tiene el objeto al lado y sigue "viéndolo" al frente. Avanzar sería chocar.

Además, cuando los dos lados estaban ocupados (pared de lado a lado) el carro esperaba `NAV_TIMEOUT_ESQUIVAR_MS` (4 s) y terminaba en
ERROR `LADOS_BLOQUEADOS`; eso no cambia (ver "Qué no resuelve").

### La maniobra nueva (fases de ESQUIVANDO)

```
        GIRAR (90°) ──► MIRAR ──(frente libre)──► AVANZAR 40 cm ──► VOLVER ──► REINCORPORANDO
                          │
                  (frente muy cerca)
                          ▼
                    RETROCEDER 15 cm ──► ASENTAR 300 ms ──► GIRAR (45° extra, luego 90° extra) ──► MIRAR ...
                          (máximo NAV_MAX_REINTENTOS = 2; con los dos agotados y el frente aún muy cerca → STOP + ERROR)
```

- **MIRAR** (arregla la causa 1): después de cada giro el carro queda en STOP y espera `NAV_PAUSA_ASENTAR_MS` (300 ms) **más el retraso del
  video** (`latencia_visual_ms`, 320 con la cámara del carro) y decide con un cuadro que llegó **después** de esa espera, es decir, que
  muestra el carro ya quieto. Si no llega ningún cuadro en `NAV_MIRAR_MAX_ESPERA_MS` (500 ms) se decide como libre (y si la cámara se
  hubiera caído, el vigilante de video ya habría dado ERROR). Costo: ~0.6 s más por esquivada con el video del carro.
- **"Sigue muy cerca"** (causa 2) = zona central con cercanía `NAV_CERCANIA_MUY_CERCA` (3), la misma condición que antes daba ERROR.
- **Reintento:** retrocede, gira del **mismo lado** un ángulo **extra** (1.er reintento `NAV_ANGULO_REINTENTO_1` = 45°, 2.º
  `NAV_ANGULO_REINTENTO_2` = 90°; en total 90 → 135 → 225° desde la dirección original) y vuelve a MIRAR. Cada giro extra deja
  al carro mirando más lejos del objeto que tiene al lado.
- **VOLVER:** deshace el giro acumulado (`angulo_total`): hasta 180° gira al lado contrario; tras dos reintentos (225°) es más corto seguir
  girando hacia el mismo lado los 135° que faltan para completar la vuelta. Después REINCORPORANDO calcula el camino a la ruta con la posición
  estimada, como siempre.
- **No cambia:** mientras AVANZA de lado, si aparece algo muy cerca al frente sigue siendo ERROR (no se esquiva en cadena), y el máximo de
  esquivadas (`NAV_MAX_ESQUIVADAS` = 3) también.

### El retroceso es CIEGO (riesgo)

El carro no tiene cámara ni sensor atrás. Para retroceder solo manda REVERSE durante un tiempo: si detrás hay una persona, una pared, una
silla o un escalón, **lo golpea o se cae sin enterarse**. Por eso:

- La distancia está limitada por una constante visible, `NAV_RETROCESO_CM` = **15 cm** (contando lo que el carro sigue al frenar), y aún
  así hay un tope absoluto de tiempo, `NAV_RETROCESO_MAX_MS` = 2000 ms, aunque una calibración mala pidiera más (probado en PC con
  500 ms/cm hacia atrás).
- **No subir** `NAV_RETROCESO_CM` sin tener cámara o sensor trasero.
- Antes de una ruta con obstáculos grandes, deja libre al menos ~30 cm detrás del carro **y** de cada punto donde pueda esquivar.
- La app avisa al iniciar la ruta si el retroceso no está calibrado (ver abajo) y escribe `esquivar fase=RETROCEDER ...` en el log (`NAV`).

### Retroceso calibrado

`CalibracionCarro` tiene `ms_por_cm_atras_x10` (bit `CAL_MEDIDO_ATRAS` = 32, **opcional**: no cuenta para "todo calibrado"). En la pantalla
de calibración es el paso **6 «Retroceso»**: se mide **igual que el avance** (REVERSE durante 1000, 2000 y 3000 ms, 3 veces cada uno, y
escribes cuántos cm retrocedió; deja unos 80 cm libres detrás). El arranque y el frenado se toman de los del avance. Si no se calibró,
`cal_ms_por_cm_atras_x10` devuelve los ms por cm del avance y la app lo avisa (`Retroceso sin calibrar...`). La posición estimada también usa
el retroceso calibrado (`cal_ms_a_mm_atras`).

### Elección del lado de giro

Antes siempre se prefería la izquierda si estaba libre. Ahora, si los dos lados están **libres** (zona sin obstáculo) se elige el que tiene
**menos celdas de obstáculo** en la imagen (`zona_celdas`, que la app manda cuadro a cuadro desde el detector): más espacio libre. Con las mismas
celdas (empate) se mantiene el comportamiento de siempre (izquierda). Si solo un lado está libre se usa ese; si ninguno, no se gira (pared).

### Pruebas en PC (`nucleo_c/pruebas_navegador.c`, función `pruebas_esquive_objetos_grandes`)

Elección del lado (4 casos); objeto grande que ocupa el centro y la izquierda (se aparta por la derecha); pared de lado a lado (ERROR
`LADOS_BLOQUEADOS`, sin retroceder); objeto que desaparece tras el giro (no retrocede); un reintento; esquina con dos reintentos (secuencia
`RIGHT, REVERSE, RIGHT, REVERSE, RIGHT`, giros de 90, 45 y 90°); reintentos agotados (ERROR con STOP y sin tercer reintento); duración del
retroceso (750 ms = 15 cm con el ejemplo); retroceso calibrado (1500 ms con 100 ms/cm) y tope absoluto de 2000 ms; cuadro viejo por retraso de
video (falsa alarma sin avisar el retraso, sin falsa alarma al avisarlo); cancelar mientras retrocede; posición estimada con retroceso
calibrado.

### Qué no resuelve

- Pared o mueble de lado a lado (los tres lados ocupados): sigue esperando 4 s y dando ERROR; no gira ni retrocede.
- Un objeto angosto que el carro ve tarde (ver `docs/prueba_obstaculos.md`) puede tocarse antes de frenar; los reintentos no cambian eso.
- Mientras retrocede, el carro no ve lo que hay detrás (ver arriba).
- Tras 225° (dos reintentos) el carro avanza de lado 40 cm en una dirección casi opuesta a la ruta; la reincorporación usa la posición estimada
  por tiempo, que acumula error en cada giro y retroceso.

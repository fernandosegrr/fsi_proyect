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
- **REINCORPORANDO**: un solo tick con STOP; calcula el camino desde la posición estimada (rumbo ajustado
  al múltiplo de 45° más cercano) hasta el punto de la ruta al que iba, y vuelve a SIGUIENDO.
  Máximo `NAV_MAX_ESQUIVADAS` esquivadas por ruta; la siguiente detención da ERROR.
- **Cancelar** deja ESPERANDO; con `navegador_comenzar` se puede retomar desde la posición estimada.

## Parámetros a calibrar con el carro

Todos están en `nucleo_c/navegador.h` (y `calibracion.h`, que también usa el navegador).

- `MS_POR_CM` y `MS_POR_GRADO` (`calibracion.h`): de ellos depende toda la posición estimada. Hay que medirlos
  con el carro real, con batería cargada, en el piso real, y repetirlos si cambia el peso o el piso.
- **Antirrebote en milisegundos** (no en cuadros: así el retraso es el mismo con 8, 12 o 30 cuadros por
  segundo). Usa el reloj `ahora_ms` que ya recibe `navegador_tick`:
  - `NAV_CERCANIA_MIN_DETENER` = 1: desde qué cercanía del detector (0 a 3) en la zona CENTRO se considera
    el frente bloqueado (pasar a 2 si los obstáculos lejanos no deben frenar el carro).
  - **Confirmación inmediata con UN solo cuadro**: cercanía 3, o cercanía 2 en la zona central.
  - `MS_CONFIRMAR_LEJOS` = 150: un obstáculo lejano (cercanía 1) debe verse 150 ms seguidos y en al menos 2
    cuadros antes de detenerse. Con cuadros cada 80 ms se confirma al tercero (160 ms); cada 120 ms, a los 240 ms.
  - `MS_CONFIRMAR_LIBRE` = 250: el frente debe verse libre 250 ms seguidos (2 cuadros como mínimo) para
    reanudar tras una detención.
  - Las zonas de los lados no detienen (solo sirven para elegir de qué lado esquivar).
- `NAV_PAUSA_MS` = 500: pausa antes de decidir. `NAV_TIMEOUT_ESQUIVAR_MS` = 4000: cuánto espera con los dos
  lados bloqueados.
- `NAV_ANGULO_ESQUIVAR` = 90 grados y `NAV_TRAMO_ESQUIVAR_CM` = 40 cm: cuánto se aparta. Hay que
  ajustarlos al ancho del carro y de los obstáculos típicos (40 cm de lado con celdas de 20 cm).
- `NAV_MAX_ESQUIVADAS` = 3, `NAV_TICK_MS` = 50 y `NAV_DT_MAX_MS` = 1000 (el firmware frena a los 1000 ms
  sin comandos, así que no tiene sentido contar más movimiento que eso entre dos ticks).

## Pruebas hechas

- **En PC**: `cd nucleo_c; make test` corre las pruebas de los tres módulos: 28 de trayectoria, 32 de
  obstáculos y **55 del navegador**, todas pasan. Cubren: rutas sin obstáculos (recta, cuadrado, zigzag,
  diagonal, media vuelta), ruta vacía, un solo punto y ruta inválida, antirrebote en ms (un cuadro lejano suelto no
  detiene; un cuadro con obstáculo cerca sí, al instante; lejano permanente a los 150 ms; con cuadros cada 50, 80
  y 120 ms; intermitente; reanudar tras 250 ms libre) y obstáculos solo a los lados (no detienen), obstáculo que desaparece, esquivar por la
  izquierda y por la derecha y reincorporarse, ambos lados bloqueados, obstáculo permanente (termina en ERROR),
  cancelar a mitad y durante ESQUIVANDO, reanudar tras cancelar, ninguna orden de movimiento tras ERROR o
  TERMINADO, y una deriva del 5 %. Con cambios deliberados al código (antirrebote, regla de reanudar, conteo
  del tiempo de cada paso) las pruebas fallan como deben.
- **Retraso en PC**: con el obstáculo cerca (cercanía 2 o 3 al centro) el STOP sale en el mismo tick del
  primer cuadro (0 ms de antirrebote), con cuadros cada 50, 80, 100 o 120 ms; con un obstáculo lejano, a los 150 ms
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
- **El rumbo se ajusta a múltiplos de 45°** al replanificar; y el camino de regreso a la ruta es en línea
  recta o diagonal sin mapa, así que puede volver a chocar con el mismo obstáculo (por eso el máximo de
  esquivadas).
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

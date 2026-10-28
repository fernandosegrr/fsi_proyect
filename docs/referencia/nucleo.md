# Referencia del núcleo de navegación en C

**Estado:** documento de referencia · **Fecha:** 27 de octubre de 2026 · **Rama:** `docs/documentacion-completa`
**Código documentado:** `nucleo_c/` en `deae8b6`. **Fuentes:** cabeceras y archivos `.c` del núcleo, `docs/navegador.md`, `docs/obstaculos.md`, `docs/calibracion.md`, `docs/reporte_J15.md` y `nucleo_c/Makefile`.

## 1. En una frase

El núcleo es el **cerebro que planea** el movimiento: recibe una ruta dibujada en una cuadrícula, la convierte en pasos con tiempos, mira si la cámara ve un obstáculo y decide en cada instante qué comando mandar al carro (`FORWARD`, `LEFT`, `STOP`...).

No habla con el hardware ni con Android: es C estándar, sin librerías externas, sin `malloc` (memoria dinámica: pedirle memoria al sistema mientras el programa corre) y solo con números enteros. Eso permite compilarlo y probarlo en un PC, y usarlo desde la app por JNI (el puente que deja que Kotlin llame a código C) sin cambiar una línea.

## 2. Los módulos, uno por uno

El núcleo creció por capas. Esta tabla ayuda a ubicar cada archivo; después se explican las capas importantes.

| Módulo | Qué resuelve | Usado por |
|---|---|---|
| `trayectoria.c` | Convierte una lista de puntos de la cuadrícula en pasos `AVANZAR`/`GIRAR` con su tiempo | App (pantalla de ruta), navegador |
| `calibracion.c` | Guarda las medidas del carro (arranque, ms/cm, frenado, ms/grado) y convierte distancia/ángulo a tiempo | Trayectoria, navegador, app |
| `obstaculos.c` | Detector por color de piso: divide la imagen en celdas y dice si hay algo que no es piso | App (visión), cámara, navegador |
| `corredor.c` | Calcula la franja de la imagen por la que el carro va a pasar (con perspectiva) | App, navegador |
| `movimiento_visual.c` | Estima cuánto giró el carro comparando dos cuadros de la cámara | App, navegador |
| `movimiento_piso.c` | Comprueba si el piso se mueve como debería al avanzar; lo que se mueve distinto puede ser un objeto | App |
| `fusion.c` | Combina el detector de color con el modelo de objetos; el color manda en seguridad | App |
| `cualquiera.c` | Modo «cualquier objeto»: usa el modelo solo para detectar una caja, sin importar la clase | App |
| `mascara_vision.c` | Aprende artefactos fijos de la imagen (viñeteo, parte del carro) y los ignora | App |
| `calgiro.c` | Calibra el giro con el giroscopio: ocho giros guiados por lado | App |
| `navegador.c` | El cerebro: sigue la ruta, frena, esquiva, vuelve a la línea y pide ayuda | App, simulador |
| `ruta_editor.c` | Lógica del editor de rutas (vértices, deshacer, plantillas, guardar) | App |
| `sim_ruta.c` + `simulador_navegador.c` | Carro virtual para probar rutas y esquives sin el carro | App, pruebas |
| `sesion_compartida.c` | Decides cuándo abrir/cerrar un recurso compartido (video, conexión) | App |
| `video_stats.c` | Mide fps y huecos del video | App |
| `latencia_video.c` | Compara el PING con y sin video | App |
| `paquete_celdas.c` | Formato UDP del video resumido en celdas | Cámara, app |
| `pisos_sinteticos.c` | Genera pisos difíciles para pruebas | Solo pruebas |

## 3. La cadena principal: de una ruta a un comando

```text
puntos de la cuadrícula
        │  trayectoria_calcular_desde_cal()
        ▼
pasos AVANZAR/GIRAR con tiempo_ms
        │  navegador_cargar_ruta() + navegador_comenzar()
        ▼
navegador_tick() cada ~50 ms  ← resultado del detector / cámara
        │
        ▼
FORWARD / REVERSE / LEFT / RIGHT / STOP
        │
        ▼
la app lo manda por WebSocket (o el simulador lo ejecuta)
```

La ruta se dibuja en celdas de **20 cm** (`trayectoria.h:19`). Cada tramo produce como máximo un giro y un avance; una ruta válida nunca desborda el arreglo de salida (`trayectoria.h:22-24`). Los tiempos salen de la calibración; si no hay calibración, se usan 50 ms/cm y 10 ms/grado como valores de partida (`calibracion.h:32-35`).

## 4. Los estados del navegador

El navegador es una **máquina de estados**: una forma de programar en la que el sistema está siempre en uno de unos pocos estados y solo cambia a otro cuando se cumple una regla. Así, en todo momento se sabe qué está haciendo el carro y qué comando tiene permitido.

| Estado | Qué significa | Comando |
|---|---|---|
| `ESPERANDO` | Ruta cargada pero no empezada | `STOP` |
| `SIGUIENDO` | Ejecuta el paso actual | El paso |
| `DETENIDO_OBSTACULO` | Vio algo y se detuvo a decidir | `STOP` |
| `ESQUIVANDO` | Gira, avanza de lado y mira | Movimiento de esquive |
| `REINCORPORANDO` | Vuelve a la línea original | Movimiento de vuelta |
| `AYUDA` | Pide que una persona tome el control | El de la cruceta |
| `TERMINADO` | Llegó al final | `STOP` |
| `ERROR` | No pudo continuar | `STOP` |

La regla de seguridad está escrita en el código y se comprueba en cada `tick`: en `ESPERANDO`, `TERMINADO` y `ERROR` el comando es **siempre** `STOP`, pase lo que pase antes (`navegador.h:17`; `navegador.c:3772-3775`).

El flujo normal es:

1. `SIGUIENDO`: avanza o gira según el paso.
2. Si la cámara ve un obstáculo cerca: `DETENIDO_OBSTACULO`, pausa de 500 ms para decidir (`NAV_PAUSA_MS`).
3. Si hay espacio: `ESQUIVANDO` (giro de 90°, avance lateral de 40 cm, otro giro).
4. `REINCORPORANDO`: vuelve a la línea, pasando primero el obstáculo para no cruzarlo.
5. Vuelve a `SIGUIENDO` hasta `TERMINADO`.

Si no puede esquivar, por defecto termina en `ERROR`; si la app activa «pedir ayuda», pasa a `AYUDA` y espera a que la persona lo maneje (`navegador.h:18-21`).

## 5. Cómo sabe dónde está: la pose

El carro **no tiene encoders ni GPS**. Su posición se estima integrando el tiempo que estuvo moviéndose, con la calibración. Eso se llama **pose estimada**: `x`, `y` y rumbo.

- Cada paso guarda cuántos milisegundos hay que mantener el comando. Con los ms/cm se convierte en distancia; con los ms/grado, en ángulo (`navegador.c:381-405`).
- La pose se actualiza en cada `tick` y se usa para planear la esquiva y la vuelta a la línea.
- Un error de calibración se acumula: por eso la pose no es una medida, es una estimación.

**Lo que cambió J15:** antes, todo el avance se proyectaba con el último rumbo conocido. Ahora, si el giroscopio está fresco, cada incremento de avance se integra con el rumbo observado; el código lo explica en `navegador.c:345-379`. La distancia **sigue siendo por tiempo**; lo que mejora es la dirección. En simulación, el error lateral medio bajó de 13.2 a 1.9 cm y el p95 de 21.9 a 4.6 cm en 400 rutas B6, pero **la prueba física quedó NO MEDIDA** (`docs/reporte_J15.md`).

## 6. Esquivar sin chocar: las reglas que frenan

El navegador tiene varias redes de seguridad. Son parámetros en `navegador.h`, no magia:

- **Antirrebote en milisegundos:** un obstáculo lejano en los lados debe verse 150 ms seguidos antes de frenar; para volver a avanzar, el frente debe verse libre 250 ms (`NAV_CERCANIA_MIN_DETENER`, `MS_CONFIRMAR_LEJOS`, `MS_CONFIRMAR_LIBRE`).
- **Cercanía:** el detector devuelve 0 a 3. Con 2 o 3 se frena; con 1 solo se informa, salvo que se active «frenar desde lejos en el centro» (`navegador_poner_frenar_lejos_centro`).
- **Hueco de video:** si la cámara deja de mandar cuadros más de 700 ms, el carro se detiene y queda en `ERROR`; no reanuda solo (`NAV_MAX_HUECO_VIDEO_MS`). Antes eran 500 ms y un hueco normal de 503 ms detuvo una ruta real (`navegador.h:71`).
- **Retroceso ciego limitado:** el carro no tiene sensor atrás. Retrocede como máximo 15 cm y 2000 ms, y solo debe usarse con espacio libre detrás (`NAV_RETROCESO_CM`, `NAV_RETROCESO_MAX_MS`).
- **Frenos anti-locura:** un giro planeado de más de 150° que no está en la ruta, un índice que retrocede, más de 2.5 veces la distancia de la ruta o una desviación mayor de 1 m hacen parar y pedir ayuda (`navegador.h:176-180`).
- **Sondeo de obstáculo fijo:** al arrancar no se decide con el primer cuadro; se piden 4 cuadros seguidos. Si el obstáculo ya estaba y el centro está libre, avanza 4 cm y comprueba si crece: si no crece, era un artefacto fijo y se ignora en esa corrida (`NAV_ARRANQUE_CUADROS`, `NAV_SONDEO_CM`).

## 7. Calibración: por qué el carro navega por tiempo

`CalibracionCarro` guarda (`calibracion.h:52-74`):

- `arranque_ms`: lo que tarda el carro en empezar a moverse.
- `ms_por_cm_x10`: milisegundos por centímetro, multiplicados por 10.
- `frenado_mm`: lo que sigue avanzando después de `STOP`.
- `ms_por_grado_izq_x10` y `..._der_x10`: el giro no es simétrico porque cada pivote usa un motor distinto.
- `frenado_grados_izq_x10` y `..._der_x10`: la inercia del giro.
- `fov_grados_x10`: campo de visión de la cámara, para el giro visual.
- `ms_por_cm_atras_x10`: retroceso, opcional.
- El desplazamiento del centro al girar (en micrómetros por grado), porque el chasis no gira sobre un punto fijo.

La app mide estos valores con pruebas guiadas y los guarda. `cal_es_valida` rechaza rangos absurdos y `cal_validar_para_guardar` exige que el giro quede entre 3 y 20 ms/grado (`calibracion.h:77-83`, `112-117`). La explicación práctica está en `docs/calibracion.md` y en `docs/calibracion_guia.md`.

## 8. El corredor y la visión

Tres ideas se combinan para no frenar por cualquier cosa:

1. **Detector de color (`obstaculos.c`):** aprende el color del piso en una franja de abajo y marca como obstáculo las celdas que no se parecen. Es rápido (~7 ms) y ve cualquier objeto, aunque no sepa qué es.
2. **Modelo de objetos (`fusion.c` + `cualquiera.c`):** EfficientDet-Lite0 reconoce personas, sillas, botellas... pero tarda ~45 ms y a veces falla. La fusión **nunca baja** lo que ve el detector de color: si el color ve algo cerca, manda el color (`fusion.h:1-20`).
3. **Corredor (`corredor.c`):** en vez de decidir con tres tercios fijos de la imagen, calcula la franja por la que el carro va a pasar, con la perspectiva de la cámara. Un objeto fuera del corredor solo avisa; uno dentro frena.

El detalle de umbrales y resultados está en `docs/obstaculos.md`, `docs/fusion.md` y `docs/giro_visual.md`.

## 9. El simulador

`simulador_navegador.c` hace de carro y de cámara a la vez:

- recibe el comando que devuelve `navegador_tick` y lo ejecuta durante 50 ms con la calibración;
- puede simular que el carro avanza o gira un porcentaje distinto del planeado, para ver la deriva;
- genera los obstáculos que vería la cámara según eventos programados;
- cuenta las **violaciones de seguridad**: veces que el navegador mandó movimiento en `ESPERANDO`, `TERMINADO` o `ERROR`. Debe ser siempre cero.

`sim_ruta.c` envuelve ese simulador para probar una ruta completa y devolver el recorrido. Es lo que usa la app para «probar la ruta sin el carro» y lo que se usó en J15 para las 400 rutas B6. **No es una medición física**: no reproduce el deslizamiento real, ni el piso, ni la batería.

## 10. Pruebas y cobertura

La suite del núcleo son **21 programas y 1598 casos** en el último registro (`README.md`, J15). Incluye trayectoria, calibración, obstáculos, corredor, fusión, máscara, giro visual, movimiento de piso, navegador, reincorporación J15, editor de rutas, sesión compartida y los simuladores. El comando es:

```bash
cd nucleo_c
make test
```

No se volvieron a ejecutar durante el cierre documental por indicación del dueño (`docs/entrega.md` §5). Los casos concretos y qué cubre cada uno están en `docs/referencia/pruebas.md` y `docs/codigo/nucleo_c.md`.

## 11. Límites que conviene decir en voz alta

- La distancia es **tiempo disfrazado de distancia**. Sin encoders, una rueda que patina no se nota.
- El giroscopio corrige el rumbo, no el deslizamiento lateral puro (`docs/entrega.md` §6).
- La cámara tarda ~250 ms en QVGA/HQVGA; a 38 cm/s eso son varios centímetros recorridos antes de que el STOP llegue.
- El detector de color confunde sombras, brillos y pisos de dos colores; por eso existen la máscara, el corredor y el sondeo.
- El modelo de objetos no se entrenó con el piso ni los objetos del proyecto; reconoce clases generales y a veces inventa (`docs/modelo_objetos.md`).
- La simulación B6 no demuestra el resultado físico; el criterio 5/6 sigue **NO MEDIDO** (`docs/reporte_J15.md`).

## 12. Para seguir leyendo

- `docs/codigo/nucleo_c.md`: recorrido por archivo y función.
- `docs/navegador.md`: referencia histórica muy detallada de estados y parámetros.
- `docs/obstaculos.md`, `docs/fusion.md`, `docs/giro_visual.md`: visión.
- `docs/calibracion.md` y `docs/calibracion_guia.md`: cómo se miden los valores.
- `docs/referencia/simulador.md`: modelo y límites de la simulación.

# Subsistema E · Núcleo de navegación en C (nucleo_c)

## E.1 Qué es y para qué sirve

El núcleo es el «cerebro» del modo autónomo: calcula dónde cree el carro que está, sigue la ruta dibujada y, si algo estorba, frena, rodea o pide ayuda. Es una biblioteca en C99 (el estándar de 1999 del lenguaje C) dentro de `nucleo_c/`. El mismo código corre en la PC (pruebas y simulador) y en el teléfono, donde Kotlin lo llama por JNI (*Java Native Interface*, el puente entre Kotlin y C). No mueve motores: cada 50 ms devuelve un comando y la app se lo manda al carro.

Más detalle: capítulo 7 del manual (`docs/manual/capitulos/07_navegacion.md`).

## E.2 Cómo funciona, paso a paso

```
 ModoSemiautonomo.kt        puente.c (JNI)        nucleo_c/navegador.c
┌────────────────────┐   ┌────────────────┐   ┌─────────────────────────┐
│ cada 50 ms:        │──▶│ NavegadorNativa│──▶│ navegador_tick(         │
│ cuadro de visión   │   │ .tick()        │   │   ahora_ms, cuadro)     │
│ + datos del carro  │◀──│                │◀──│ devuelve UN comando     │
└────────────────────┘   └────────────────┘   └─────────────────────────┘
          │ STOP / FORWARD / REVERSE / LEFT / RIGHT
          ▼
  ControlLogica ──▶ WebSocket ──▶ carro (firmware, subsistema B)
```

1. **Reloj.** `ModoSemiautonomo.kt` llama a `NavegadorNativa.tick()` cada `TICK_MS = 50` ms, con el último cuadro nuevo de la visión si lo hay. Si el comando cambia, `ControlLogica` lo manda al carro y lo repite cada 250 ms (latido).
2. **Pose.** La pose es dónde cree el carro que está: x e y en mm y rumbo en grados (con giroscopio, en décimas: `rumbo_x10`). `navegador_tick` mide el tiempo desde el tick anterior (máximo `NAV_DT_MAX_MS = 1,000` ms) y lo convierte en distancia o ángulo con la calibración (`CalibracionCarro`: ms por cm, arranque, frenado y ms por grado de cada lado). No hay encoders. Los giros son de pivote (un lado gira, el otro queda fijo) y desplazan el centro (`docs/calibracion.md`).
3. **Vigilancias.** Más de `NAV_MAX_HUECO_VIDEO_MS = 700` ms sin cuadros de la cámara: STOP y ERROR causa 5, que no reanuda sola. Si la ruta cuenta con el detector de objetos y pasan `NAV_MODELO_CAIDO_MS = 2,500` ms sin resultado vigente: causa 17. Si el láser estaba vivo y calla `NAV_TOF_CAIDO_MS = 600` ms: causa 19.
4. **Frenos anti-locura** (`revisar_frenos`). Giro planeado de más de `NAV_LOCURA_GIRO_MAX = 150` grados que la ruta no tiene (causa 9); índice de la ruta que retrocede (10); recorrido de más de 2.5 veces la ruta (`NAV_LOCURA_RECORRIDO_X10 = 25`; 11); pose a más de 1 m de la ruta (`NAV_LOCURA_DESVIO_MM = 1000`; 12).
5. **Decisión.** Según el estado, `navegador_tick` llama a `tick_siguiendo`, `tick_detenido`, `tick_esquivando`, `tick_reincorporando` o `tick_ayuda`.
6. **Regla de seguridad.** Al final, en ESPERANDO, TERMINADO y ERROR el comando se fuerza a STOP «pase lo que pase arriba» (`nucleo_c/navegador.c`).
7. **Respuesta.** Uno de cinco comandos (`ComandoNav`): STOP, FORWARD, REVERSE, LEFT o RIGHT.

### Los ocho estados

```
┌───────────┐ comenzar  ┌───────────┐       obstáculo   ┌────────────────────┐
│ ESPERANDO │──────────▶│ SIGUIENDO │──────────────────▶│ DETENIDO_OBSTACULO │
└───────────┘           └───────────┘◀──────────────────└────────────────────┘
           llega al final │       ▲         queda libre     │ elige lado
                          ▼       │                         ▼
                    ┌───────────┐ │                   ┌────────────┐
                    │ TERMINADO │ │                   │ ESQUIVANDO │
                    └───────────┘ │ vuelve a la línea └────────────┘
                                  │                         │
                                  │                         │
                              ┌────────────────┐            │
                              │ REINCORPORANDO │◀───────────┘
                              └────────────────┘  pasó el obstáculo
```

| Estado | Qué hace | Sale hacia |
|---|---|---|
| ESPERANDO | Quieto, sin ruta en marcha | SIGUIENDO con `navegador_comenzar` (TERMINADO si la ruta tiene un solo punto) |
| SIGUIENDO | Avanza y gira siguiendo los pasos de la ruta | DETENIDO_OBSTACULO al confirmar un obstáculo; TERMINADO al llegar |
| DETENIDO_OBSTACULO | STOP de `NAV_PAUSA_MS = 500` ms y decide | SIGUIENDO si quedó libre; ESQUIVANDO si elige un lado; ERROR o AYUDA si no puede |
| ESQUIVANDO | Rodeo en 10 fases (`NAV_FASE_GIRAR` a `NAV_FASE_PAUSA_RETROCEDER`) | REINCORPORANDO; DETENIDO_OBSTACULO si algo bloquea el paso |
| REINCORPORANDO | Vuelta a la línea en 10 fases (`NAV_RF_PLAN` a `NAV_RF_DESHACER`) | SIGUIENDO (TERMINADO si ya pasó el último punto); DETENIDO_OBSTACULO si el tramo está bloqueado |
| TERMINADO | Llegó. Con un rodeo abierto, antes comprueba que el frente esté libre | ESPERANDO con ruta nueva |
| ERROR | Se detuvo por una causa. No se sale solo | ESPERANDO con ruta nueva |
| AYUDA | Quieto: el usuario maneja con la cruceta | REINCORPORANDO con «Continuar»; ERROR a los 180,000 ms |

Cómo se mide el obstáculo y se decide qué rodear (cámara y láser) es del subsistema F.

### Causas y AYUDA

Cada detención guarda una causa (`NAV_CAUSA_*`, códigos 0 a 19; `NAV_CAUSAS = 20`). La pantalla muestra «Causa N: texto» (`docs/limites_conocidos.md`; el texto sale de `navegador_texto_error`).

| Códigos | Qué pasó |
|---|---|
| 1, 2, 3 | No pudo esquivar: lados ocupados `NAV_TIMEOUT_ESQUIVAR_MS = 4000` ms (1); 6 maniobras por obstáculo o 18 por ruta (2); 2 reintentos agotados (3) |
| 4, 8 | Sin plan: el planificador no halla camino (4, siempre ERROR); la vuelta a la línea no tiene solución con giros de pivote (8) |
| 5, 17, 19 | Percepción caída: sin video (5, siempre ERROR); detector de objetos (17); láser (19) |
| 6, 7, 14 | La maniobra no cabe: arco de salida (6); correcciones de la vuelta agotadas (7); el rebase no cabe en la recta (14) |
| 9 a 12 | Frenos anti-locura (paso 4 de arriba) |
| 16, 18 | Láser al girar: pared u objeto más ancho que el rodeo máximo (16); algo dentro del arco que barre el chasis (18) |
| 0, 13, 15 | Sin causa (0); históricos que el código ya no emite (13 y 15) |

```
 cualquier estado en marcha ─── causas 4 y 5 ────────────────▶ ERROR
        │                                                        ▲
        │ las demás causas, con «pedir ayuda» activo             │ 180,000 ms
        ▼                                                        │ sin respuesta
      AYUDA ─── «Continuar» ───▶ REINCORPORANDO                  │
        └────────────────────────────────────────────────────────┘
```

El núcleo parte con `pedir_ayuda = 0`; la app lo enciende en cada ruta, salvo en la prueba de obstáculos (`ModoSemiautonomo.kt`). En AYUDA, `tick_ayuda` devuelve el comando que el usuario manda con la cruceta (`navegador_ayuda_comando`). Con «Continuar» (`navegador_ayuda_continuar`) el navegador devuelve los presupuestos a cero, olvida lo que el láser había confirmado antes y pasa a REINCORPORANDO, que calcula la vuelta con la pose estimada. Si pasan `NAV_AYUDA_MAX_MS = 180000` ms (3 minutos), termina en ERROR con el motivo original.

### Los módulos de nucleo_c

Son 22 módulos en C. Las filas con (F) se explican en el subsistema F (visión).

| Módulo | Qué hace | Quién lo usa |
|---|---|---|
| `navegador` | Máquina de estados que sigue la ruta, esquiva y pide ayuda | App (`NavegadorNativa`), simulador y pruebas |
| `trayectoria` | Convierte celdas de 20 cm en pasos AVANZAR y GIRAR con su tiempo (máximo 64 puntos) | `navegador`, `ruta_editor`; app (`TrayectoriaNativa`) |
| `calibracion` | Pasa distancia y giro a tiempo: ms por cm, arranque, frenado, ms por grado, desplazamiento del pivote | `navegador`, simulador; app (`CalibracionNativa`) |
| `calgiro` | Calibra el giro con el giroscopio: dos bloques de cuatro giros por lado | App (`CalGiroNativa`); pruebas |
| `obstaculos` (F) | Detector de obstáculos por color de piso sobre una rejilla de celdas | App (`ObstaculosNativa`); cámara en modo CELDAS; pruebas |
| `mascara_vision` (F) | Máscara de artefactos fijos de la imagen («Calibrar visión») | `navegador`; app |
| `corredor` (F) | Proyecta en la imagen la franja por donde pasa el carro | App (`CorredorNativa`); `movimiento_piso`, `cualquiera` |
| `fusion` (F) | Une el color con el modelo de objetos EfficientDet-Lite0 | App (`FusionNativa`); banco autónomo |
| `movimiento_visual` (F) | Cuánto giró el carro entre dos cuadros | App (`MovimientoVisualNativa`) |
| `movimiento_piso` (F) | Flujo del piso: cuánto se mueve cada bloque contra lo esperado | App (`FlujoNativa`); `fusion` |
| `cualquiera` (F) | Modo «cualquier objeto»: el modelo solo avisa que hay una caja sobre el piso | App (`CualquieraNativa`) |
| `vision_tele` (F) | Resume cada cuadro en números (línea `VTEL`) para la caja negra | `puente.c`, vía `ObstaculosNativa` |
| `video_stats` | fps, huecos y ventana de 10 s del video | App (`VideoStatsNativa`) |
| `latencia_video` | Compara la respuesta del control con y sin video abierto | App (`LatenciaVideoNativa`) |
| `prueba_obstaculo` | Cuentas de la prueba «Obstáculos: cámara»: tiempo de frenado y cm recorridos | App (`PruebaObstaculoNativa`) |
| `paquete_celdas` | Arma y lee el paquete UDP `CEL1` (cuadro resumido en celdas de 10×10 px) | Cámara (`paquete_armar`), app (`PaqueteCeldasNativa`), cámara falsa de PC |
| `sesion_compartida` | Recurso que varias pantallas comparten (video, conexión) | App (`SesionNativa`) |
| `ruta_editor` | Lógica del editor de rutas: vértices, deshacer, rehacer, plantillas | App (`RutaEditorNativa`) |
| `sim_ruta` | Corre el navegador real con un carro virtual para probar una ruta | App («Rutas → Simular», `SimRutaNativa`); pruebas |
| `simulador_navegador` | Carro virtual en PC: motores, frenado, giroscopio y láser | `sim_ruta`; pruebas y banco (subsistema H) |
| `mundo_v2` | Mundo sintético (objetos, cámara, láser, detector) solo para el banco | `pruebas_autonomo_v2` (subsistema H); no va en la app |
| `pisos_sinteticos` | Cuadros sintéticos de pisos difíciles | Pruebas del detector; cámara falsa de PC |

### El puente JNI

`app_android/app/src/main/cpp/puente.c` (3,644 líneas) traduce entre Kotlin y C con 145 funciones `Java_com_fsi_carro_*Nativa_*` repartidas en 17 clases Kotlin (la mayor, `NavegadorNativa`, tiene 36). Hay una sola instancia del navegador (`static Navegador navegador_global`, sin `malloc`), que solo toca el hilo principal. La rejilla de celdas que escribe el hilo de la cámara y lee el ciclo de 50 ms sí se protege con un candado (`candado_vision`, J16). `estado()` devuelve 11 enteros: estado, x, y, rumbo, objetivo, puntos, esquivadas, motivo, comando, reintentos y fase. `CMakeLists.txt` arma `libcarro_nativo.so` con `puente.c` y 20 módulos que toma de `nucleo_c/` sin copiarlos; `mundo_v2.c` y `pisos_sinteticos.c` no entran.

## E.3 Cómo lo hicimos

- **Un solo código para PC y teléfono.** El núcleo se hizo en C99 «sin duplicar código entre la computadora de desarrollo y el teléfono Android» (`docs/manual/capitulos/14_desarrollo.md`, §14.2). La app compila los `.c` desde su carpeta, sin copiarlos (`docs/referencia/compilacion.md`). Eso permitió un simulador que corre miles de recorridos en milisegundos, y la cámara reutiliza `obstaculos.c` y `paquete_celdas.c` en su modo CELDAS (`camara_prueba/camara_prueba.ino`).
- **Reglas del código.** Se compila con `-std=c99 -Wall -Wextra -Wpedantic -Werror` (`nucleo_c/Makefile`). Los módulos usan solo enteros, sin `malloc` ni recursión (lo dicen 16 cabeceras, por ejemplo `nucleo_c/navegador.h`); el README lo resume como «determinismo y seguridad» (`nucleo_c/README.md`). Con `git grep` se comprobó que ningún módulo de la app ni `puente.c` llaman a `malloc`, y que `double` solo aparece en `mundo_v2.c`, que es del banco.
- **Navegar por tiempo.** No hay encoders: la distancia se estima con tiempo y calibración (`docs/limites_conocidos.md`). El giroscopio, el láser y la cámara corrigen y vigilan (subsistemas B, C y F).
- **Estrategia de esquive.** Un análisis del propio proyecto (`docs/investigacion_autonomo/09_decision_navegacion_reactiva.md`, §6.4) compara el rodeo medido con VFH, seguimiento de borde tipo Bug, rodeo fijo (lo que hacía v1.1) y un híbrido. En el banco simulado las opciones simples pierden (con el objeto de 90 cm, v1.1 da 535 contactos en 1,000 corridas), pero el documento es una INFERENCIA y recomienda no cambiar nada hasta medir en pista.
- **Ayuda en vez de rendirse.** Con «pedir ayuda» activo, no poder esquivar deja al carro quieto y pide que el usuario tome el control (comentario de `ir_a_error_causa` en `nucleo_c/navegador.c`). Sin video no hay ayuda: el carro no ve y no reanuda solo.
- `CONFIRMAR CON EL EQUIPO: ¿por qué se eligió C99 y no Kotlin o C++ para el navegador, y se probó otra opción?`
- `CONFIRMAR CON EL EQUIPO: ¿por qué celdas de 20 cm (CELDA_CM) y no de otro tamaño?`
- `CONFIRMAR CON EL EQUIPO: ¿el equipo comparó VFH o seguimiento de borde antes de elegir el rodeo medido, o esa comparación fue posterior?`

## E.4 Cómo lo probamos y con qué resultado

| Qué se probó | Cómo | Resultado | Etiqueta |
|---|---|---|---|
| Núcleo en PC (v3) | `make -C nucleo_c test` | 24 ejecutables, 1,901 casos, 0 fallos, el 16-nov-2026 (`docs/estado_20261116.md`) | **SIMULADO** |
| Desbordes de enteros (v3) | `-ftrapv` en toda la suite y el banco; analizador estático de GCC sobre `navegador.c` | Sin fallos ni avisos, 16-nov-2026 | **SIMULADO** |
| Regla de seguridad | `pruebas_navegador.c` cuenta movimientos en ESPERANDO, TERMINADO o ERROR | La prueba exige 0 en todos los casos; pasa | **SIMULADO** |
| Banco realista (v3) | 23 escenarios × 1,000 corridas (subsistema H) | 23 compuertas en rojo: muebles con patas, caja con pared al lado, paradas con sol, sombra y reflejo | **SIMULADO** |
| Puente JNI y app (v3) | `scripts/compilar_app.ps1` | COMPILACION OK; el puente no tiene prueba de PC | **SIMULADO** (solo compilado) |
| Esquive en el carro (v1.1) | B6, 10-nov-2026, `Recta 200 cm`, caja a ~100 cm | 6 de 6 sin tocar la caja; 5 de 6 completas; error lateral aproximado de 0 a 10 cm (`docs/estado_20261110.md`) | **VALIDADO EN EL CARRO** |
| Navegador v3 en el carro | Protocolo A a D y M1 a M10 (`docs/protocolo_prueba_fisica_autonomo_v2.md`) | Ninguna corrida | **PENDIENTE** |
| ASan, UBSan y TSan sobre v3 | No se pudieron correr: el compilador de Windows no trae las bibliotecas | Sin resultado | **NO MEDIDO** |
| Velocidad real en AUTO; latencia de extremo a extremo | Mediciones M8 y M9 | Sin medición | **NO MEDIDO** |

Estas cifras son del 16-nov-2026; el código del núcleo no cambió después (último commit de `nucleo_c/`: `d29133a`, 16-nov-2026) y no se repitieron para esta entrega. Lo validado el 10-nov-2026 es el navegador de v1.1, no el de v3.

## E.5 Límites conocidos

- **Pose por tiempo.** Sin encoders el error se acumula; hay que recalibrar si cambian pilas, piso o peso (`docs/calibracion.md`). La velocidad de 19.3 cm/s puede estar obsoleta: las cajas negras del 15-nov sugieren cerca de 30 cm/s (`docs/limites_conocidos.md`). **NO MEDIDO** (medición M8).
- **Pivote asimétrico.** Derecho: radio de unos 15.8 cm. Izquierdo: unos 8.7 a 9 cm, poco medido (`docs/limites_conocidos.md`).
- **Sin sensor trasero.** Las reversas (150 a 250 mm; hasta 450 mm para deshacer una diagonal) suponen que nadie entró detrás.
- **Patas finas y objetos sin eco.** El láser es un solo haz y «sin eco» no cuenta como libre. En simulación, banquito y silla siguen con contactos y ayudas, y `bloque_sin_eco` (objeto oscuro o mate) termina en contacto en 40 de 40 corridas del conjunto reservado (`docs/estado_v3_pendiente_pruebas.md`; **SIMULADO**). Si el láser ve patas reales: **NO MEDIDO** (M2).
- **Luz.** Manchas de sol, sombras y reflejos detienen un momento al carro en 93, 82 y 126 de 1,000 corridas simuladas. «Aprender piso» es obligatorio en cada ruta y la app no detecta sola un cambio de luz.
- **Pared a un lado.** Con una pared pegada a la ruta, 386 de 1,000 corridas simuladas terminan en AYUDA (causas 16 y 18). Un objeto de más de ~55 cm de frente no se rodea.
- **«Continuar» tras AYUDA.** Cada «Continuar» devuelve los presupuestos (6 esquivadas por obstáculo, 18 por ruta, 2 reintentos) y no hay tope global (hallazgo H-12, ABIERTO en v3: `docs/investigacion_autonomo/90_hallazgos.md`). Si el usuario pulsa «Continuar» sin mover el carro y el obstáculo sigue, el navegador vuelve a pedir ayuda; con el láser caído también (causa 19). Según el hallazgo no hay riesgo de contacto, pero sí pérdida de tiempo y desgaste.

## E.6 Términos clave

- **nucleo_c**: carpeta con la lógica de navegación, calibración, visión y simulador, en C99.
- **navegador_tick**: función que se llama cada 50 ms y devuelve un comando.
- **EstadoNav**: los ocho estados (ESPERANDO, SIGUIENDO, DETENIDO_OBSTACULO, ESQUIVANDO, REINCORPORANDO, TERMINADO, ERROR, AYUDA).
- **ComandoNav**: los cinco comandos del protocolo (STOP, FORWARD, REVERSE, LEFT, RIGHT).
- **pose**: posición estimada (x, y en mm) y rumbo (grados; `rumbo_x10` en décimas).
- **CalibracionCarro**: medidas que pasan distancia y giro a tiempo.
- **celda**: cuadro de 20 cm de la cuadrícula de rutas (`CELDA_CM`).
- **causa**: código `NAV_CAUSA_*` (0 a 19) que dice por qué se detuvo.
- **AYUDA**: estado en que el carro se queda quieto y deja que el usuario maneje.
- **freno anti-locura**: revisión de cordura de las causas 9 a 12.
- **JNI**: puente entre Kotlin y C; en este proyecto es `puente.c`, con 145 funciones.
- **NDK**: herramienta de Android que compila el C para el teléfono.
- **TICK_MS**: 50 ms entre decisiones, en `ModoSemiautonomo.kt`.

## E.7 Preguntas probables del profesor

1. **¿Por qué el navegador está en C y no en Kotlin?** Los documentos dicen que así el mismo código corre en la PC y en el teléfono sin duplicarlo, lo que permitió simular miles de recorridos. No hay un texto que compare C con otras opciones: esa parte hay que confirmarla con el equipo.
2. **¿Cómo sabe el carro dónde está si no tiene encoders?** Estima: multiplica el tiempo de motor por la calibración y usa el giroscopio para los giros. Es una estimación que se desvía. En v1.1 el error lateral final fue de unos 0 a 10 cm en B6, medido de forma aproximada; la deriva en general es **NO MEDIDO**.
3. **¿Qué pasa si falla la cámara o el láser a mitad de ruta?** El carro se detiene: 700 ms sin video es ERROR; 2.5 s sin resultados del detector o 600 ms con el láser caído piden AYUDA (ERROR si «pedir ayuda» está apagado). El firmware tiene además su watchdog de 1,000 ms (subsistemas B y G).
4. **¿Qué diferencia hay entre AYUDA y ERROR?** En ERROR el carro se detiene y solo sale con una ruta nueva. En AYUDA se detiene y el usuario puede manejar con la cruceta y pulsar «Continuar»; si nadie responde en 3 minutos pasa a ERROR.
5. **¿Qué tan grande es el puente con Kotlin?** `puente.c` tiene 3,644 líneas y 145 funciones en 17 clases. Solo compila con el NDK y no tiene prueba de PC; lo cubren las pruebas del núcleo y la app con carro falso.

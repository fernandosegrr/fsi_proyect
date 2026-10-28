# Referencia del simulador de rutas

**Estado:** documento de referencia · **Fecha:** 27 de octubre de 2026 · **Rama:** `docs/documentacion-completa`
**Código documentado:** `nucleo_c/` en `deae8b6`. **Fuentes:** `simulador_navegador.h/.c`, `sim_ruta.h/.c`, `pruebas_reincorporacion_j15.c`, `navegador.h`, `docs/reporte_J15.md` y `docs/navegador.md`.

## 1. En una frase

El simulador es un **carro de mentira dentro del PC**: recibe los mismos comandos que el carro real, se mueve con la misma calibración y le cuenta al navegador lo que vería una cámara y un láser. Sirve para repetir una ruta cien veces sin gastar batería, sin mover el carro y sin riesgo de chocar.

No demuestra cómo se comporta el carro de verdad. Demuestra que **la lógica del navegador hace lo que se espera en un mundo simplificado**. La diferencia entre las dos cosas es el centro de este documento.

## 2. Las dos piezas

| Archivo | Qué hace | Se usa desde |
|---|---|---|
| `simulador_navegador.c` | Hace de **carro** (posición, rumbo, motores, frenado) y de **firmware** (giroscopio, láser, mensajes) | Pruebas del núcleo |
| `sim_ruta.c` | Hace de **cámara** y de **punto de entrada**: recibe una ruta de la cuadrícula y corre el ciclo completo | App, pantalla **Rutas → Simular**; pruebas |

La idea es la misma que en el resto del proyecto: el navegador (`navegador.c`) no sabe si le habla un carro real o uno simulado. Recibe un comando, devuelve otro y consulta sensores por la misma interfaz. Por eso una prueba de simulación ejercita **el código real** de navegación, no una copia.

```text
ruta de celdas
     │
     ▼
sr_simular()  ──────────────────────────────────────────────┐
     │                                                      │
     │  cada 50 ms:                                        │
     │    sr_vista()  → ¿qué ve la cámara virtual?          │
     │    sim_correr() → navegador_tick() → mover el carro  │
     │    anotar posición y comando                         │
     └──────────────────────────────────────────────────────┘
     │
     ▼
resultado: llegó, tiempo, muestras, cambios, advertencias
```

## 3. El carro virtual

`sim_iniciar()` coloca el carro en el centro de una celda con un rumbo (`simulador_navegador.c:53`). A partir de ahí, cada 50 ms `sim_correr()` hace lo que haría el firmware:

- **Avanzar o retroceder:** convierte el tiempo en distancia con `ms_por_cm_x10` de la calibración. Si el carro acaba de arrancar, espera `arranque_ms` antes de moverse; al soltar el comando, sigue un poco por el frenado (`frenado_mm`).
- **Girar:** usa `ms_por_grado_izq_x10` o `..._der_x10` según el lado. El giro no es simétrico porque cada pivote usa un motor distinto.
- **Desplazarse al girar:** aplica el desplazamiento del centro del chasis (`desp_*`), porque el carro no gira sobre un punto fijo.
- **Torcerse:** el desbalance entre los dos lados se puede configurar. Con `desbalance_mil × balance_mil = 1000 × 1000` va recto; si no, se desvía como un carro mal ajustado (`simulador_navegador.c:182`).
- **Fallar a propósito:** `pct_avance` y `pct_giro` permiten que el carro real recorra, por ejemplo, un 95 % de lo planeado. Así se prueba la deriva.

El simulador puede usar una calibración **distinta** de la que cree el navegador (`sim.real`). Eso reproduce el caso incómodo: el carro está mal calibrado, pero el cerebro cree que está bien. Es una de las formas más útiles de encontrar un error de diseño.

## 4. La cámara virtual

`sr_vista()` (`sim_ruta.c:87`) responde una pregunta sencilla: **si el carro está aquí y mira hacia allá, ¿qué obstáculos ve y en qué zona?**

- Los obstáculos son celdas de la cuadrícula de 20 × 20 cm. Se tratan como cajas de 20 cm de alto.
- Solo se ven si están **adelante**, dentro de unos **±30°** (60° de campo visual) y a menos de **80 cm** de la cara.
- La **zona** (izquierda, centro, derecha) sale de a qué lado del carro cae la celda, con los mismos tercios que usa el detector.
- La **cercanía** sale de la distancia a la cara: 3 hasta 30 cm, 2 hasta 50 cm y 1 más lejos.
- El número de **celdas** que ocuparía la caja crece al acercarse. Eso permite probar las reglas que distinguen un objeto real de un artefacto fijo.

El modelo es **aproximado**: no simula píxeles, ni colores, ni sombras, ni el detector por color. Solo entrega el resultado que el detector habría producido. Por eso no sirve para validar la visión; sirve para validar **cómo reacciona el navegador** a un resultado dado.

En la pantalla **Rutas** de la app se eligen obstáculos tocando celdas (`EditorRutaView.kt:7`). El simulador los convierte a esa vista y anima el recorrido con las muestras. La app usa la calibración y el balance guardados, si existen (`RutasFragment.kt:208-234`).

## 5. El firmware virtual

Además de la cámara, el simulador puede hacer de firmware con sensores:

| Pieza | Qué simula | Interruptor |
|---|---|---|
| Giroscopio | Contesta los giros por ángulo, con error de escala, sesgo, ruido y anticipación que se corrige sola | `fw_giroscopio` |
| Láser | Manda `SENS` cada 100 ms y aplica la parada de emergencia a menos de 120 mm | `tof_activo` |
| Caja para el láser | Distancia calculada contra una caja del mundo, independiente de la cámara | `laser_externo` |
| Fallos | El firmware no contesta, el giroscopio falla en el giro N, se pierde un `GIRO_OK`, el láser está caído | `fw_sin_respuesta`, `fw_falla_desde`, `fw_pierde_mensaje`, `tof_perdido` |

Con eso se prueban caminos que en el carro serían caros o peligrosos: «¿qué pasa si el giroscopio falla en el tercer giro?», «¿y si el mensaje de fin de giro se pierde pero el carro sí giró?».

## 6. La simulación de una ruta completa

`sr_simular()` (`sim_ruta.c:164`) es la función que usa la app. Recibe:

- la ruta (celdas vecinas),
- la calibración (o `NULL` para usar valores de ejemplo),
- hasta **16 obstáculos** (celdas),
- un **ruido de giro** en grados,
- una **semilla**.

Y devuelve un `ResultadoSimRuta` con:

| Campo | Qué dice |
|---|---|
| `llego` | Si terminó en `TERMINADO` |
| `duracion_ms` | Cuánto tardó |
| `n_comandos` | Cuántas veces cambió el comando |
| `esquivadas` | Cuántas esquivas hizo |
| `x_final_mm`, `y_final_mm` | Dónde terminó |
| `error_final_mm` | Distancia al último punto de la ruta |
| `muestras[]` | Posición, rumbo, comando y estado cada 100 ms (hasta 1500) |
| `cambios[]` | Cada cambio de comando con su hora |
| `advertencias` | Salió de la cuadrícula, no llegó, se cortó por tiempo o usó calibración de ejemplo |

El ciclo se corta a los **150 s**, si el navegador llega a `TERMINADO` o `ERROR`, o si se acaba la memoria de muestras. La **semilla** es el número con el que arranca el generador de ruido: con la misma semilla sale exactamente el mismo resultado, y con otra sale una variación distinta. Eso hace que cualquier fallo se pueda repetir y depurar.

`sr_dispersion()` repite la ruta hasta **24 veces** con ruidos distintos y resume cuántas llegaron, el error medio, el máximo y la dispersión entre finales. Es útil para ver si una ruta es robusta o si depende de que todo salga perfecto.

## 7. El banco de J15: 400 rutas B6

La corrección de pose de J15 se evaluó con `pruebas_reincorporacion_j15.c`, no con el carro. El escenario simulado es:

- una recta de **2 m** (11 puntos de una cuadrícula de 20 cm);
- una **caja de 20 × 20 cm** a **1 m** del punto de partida;
- el chasis aproximado por un rectángulo de **20 × 24 cm**;
- **400 corridas** con semillas distintas;
- video a **10 cuadros/s** con **200-300 ms** de retraso;
- giroscopio con ruido y error de cierre por lado;
- láser con alcance de **1.2 m** y autoridad de parada;
- desbalance del carro entre 1006 y 1018 milésimas y variación de ±5 % en el pivote (supuesta, no medida).

En la tabla, **media** es el promedio del error; **p95** significa que el 95 % de las corridas quedó por debajo de ese error (deja fuera el 5 % peor); **contactos** son las veces que el rectángulo del carro tocó el de la caja.

Resultados registrados (`docs/reporte_J15.md` §4):

| Métrica | Antes de J15 | Después de J15 |
|---|---|---|
| Error lateral medio | 13.2 cm | **1.9 cm** |
| Error lateral p95 | 21.9 cm | **4.6 cm** |
| Contactos con la caja | — | **0 en 400** |
| Rutas que llegan | — | 400 de 400 |

Estas cifras son **SIMULADAS**. El propio reporte lo dice: el escenario reproduce errores de decenas de centímetros, pero no reproduce exactamente los 40 cm de B6-4 ni las esquivas múltiples de J14, y la mejora simulada **no demuestra** una mejora física. El criterio de 5 de 6 corridas reales sigue **NO MEDIDO**.

## 8. Las violaciones de seguridad

El simulador cuenta una cosa que nunca debe pasar: **que el navegador mande un movimiento estando en `ESPERANDO`, `TERMINADO` o `ERROR`** (`simulador_navegador.c:499`). En esos estados el comando debe ser siempre `STOP`.

Ese contador es una prueba de propiedad, no un caso concreto: se revisa en todas las corridas. Si alguna vez sube de cero, hay un fallo de seguridad, aunque la ruta haya llegado al final.

## 9. Cómo se ejecuta

Las pruebas del núcleo compilan el simulador con el resto:

```bash
cd nucleo_c
make pruebas_reincorporacion_j15
./pruebas_reincorporacion_j15
```

El programa imprime una línea por escenario con media, p50, p95, máximo, contactos, llegadas, éxitos y esquivas, y termina con `N casos, M fallos`. No se volvió a ejecutar durante el cierre documental por indicación del dueño; los números de la tabla son los del reporte de J15.

Para una ruta cualquiera desde la app: **Rutas → dibujar → Simular**. La animación usa las muestras reales del simulador, no una interpolación inventada.

## 10. Lo que el simulador NO prueba

- **Deslizamiento de ruedas.** El carro virtual avanza exactamente lo que dice la calibración (salvo que se cambie `pct_avance` a mano). Un carro real patina y no se enteró.
- **El detector de color real.** La cámara virtual entrega zonas y cercanías ya calculadas; no ve sombras, reflejos ni pisos de dos colores.
- **El modelo de objetos.** EfficientDet no participa en la simulación.
- **La física del choque.** El rectángulo del chasis sirve para contar contactos en el banco J15, pero no hay rebote, ni empuje, ni daño.
- **El WiFi, la batería y el calentamiento.** Nada de eso existe en el mundo simulado.
- **La latencia real de extremo a extremo.** El retraso de video se configura a mano; no se mide.
- **El comportamiento de la cámara montada y en movimiento.** No hay vibración ni cambios de luz.

Por eso la simulación responde «¿la lógica es coherente?», no «¿el carro va a funcionar en el piso?». Para lo segundo hacen falta las corridas físicas, que en J15 fueron cero.

## 11. Para seguir leyendo

- `docs/referencia/nucleo.md`: el navegador y sus estados.
- `docs/reporte_J15.md`: el banco completo y sus límites.
- `docs/navegador.md`: referencia histórica de parámetros y diagramas.
- `docs/referencia/pruebas.md`: las suites y qué cubre cada una.
- `docs/codigo/nucleo_c.md`: recorrido por las funciones del simulador.

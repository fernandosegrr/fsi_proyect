# 8. Visión y percepción

Este capítulo explica cómo el sistema transforma las imágenes de la cámara `Freenove` y las lecturas del sensor láser frontal `VL53L0X` en una decisión geométrica única para el navegador. Expone por qué ningún sensor individual basta por sí solo, cómo operan el clasificador de piso multiclúster (`obstaculos.c`), la red neuronal de objetos (`DetectorTflite.kt`), el corredor de perspectiva (`corredor.c`) y el módulo de fusión (`fusion.c`), y cómo el cierre **J16** protegió la concurrencia entre hilos mediante el candado mutex `candado_vision`.

## 8.1 Por qué se fusionan color del piso, red neuronal y sensor láser

En un entorno interior real, confiar la detección de obstáculos a un solo principio físico produce puntos ciegos graves:

1. **Limitación del color del piso:** un clasificador cromático detecta cualquier objeto cuyo color difiera del suelo aprendido, pero puede confundirse ante reflejos intensos de lámparas sobre baldosas brillantes, sombras pronunciadas o tapetes de dos tonos, y no distingue una caja del color exacto del piso.
2. **Limitación de la red neuronal (`EfficientDet-Lite0`):** el modelo preentrenado en `COCO` reconoce 90 categorías cotidianas (sillas, botellas, mochilas, personas), pero una caja de cartón lisa o un bloque de madera sin textura puede no superar el umbral de confianza de ninguna clase estándar.
3. **Limitación del láser puntual (`VL53L0X`):** mide con exactitud milimétrica la distancia física sin depender de la iluminación ni del color, pero su cono óptico es estrecho (`~25°` centrados en el eje longitudinal) y no informa si el camino está despejado a la izquierda o a la derecha para poder esquivar.

Al combinar las tres fuentes dentro de un marco geométrico común (**el corredor de perspectiva**), el vehículo aprovecha la cobertura espacial de la cámara (`240×176` píxeles) junto con la certidumbre de distancia del láser frontal.

[FIGURA:percepcion|Tubería multimodal de visión y percepción: clasificador de piso YCbCr/RGB, red neuronal EfficientDet-Lite0, corredor de perspectiva y láser VL53L0X integrados bajo candado_vision.]

## 8.2 Clasificador cromático de piso multiclúster (`obstaculos.c`) y máscara fija

El módulo en `C99` `obstaculos.c` divide la imagen en una rejilla de **`10×10` celdas lógicas** (o procesa directamente los paquetes `UDP` de `24×17` bloques RGB provenientes del puerto `4210`). Para separar la luminosidad del matiz cromático, convierte cada celda del espacio $(R, G, B)$ al espacio luma-crominancia **$(Y, C_b, C_r)$** en aritmética entera rápida.

### Aprendizaje de hasta tres clústeres de piso y regla J10c

Durante el paso de aprendizaje de suelo (cuando el usuario apunta el carro hacia un tramo despejado y pulsa `Aprender piso`), el algoritmo muestrea la franja central-inferior de la imagen (`filas 6 a 8`, `columnas 2 a 7`) y agrupa las celdas en **hasta `3` clústeres cromáticos** (`OBS_MAX_CLUSTERS = 3`). Esto permite modelar pisos con vetas, mosaicos o juntas de distinto tono.

Además, para evitar falsas alarmas cuando el carro avanza hacia una sombra o una baldosa ligeramente distinta situada en la mitad superior de la vista, la **regla J10c** relaja el umbral cromático en las filas lejanas (`fila < 7`, lejos del parachoques) si la diferencia respecto al modelo principal es moderada y la textura vertical sigue siendo uniforme.

### Máscara de artefactos fijos (`mascara_vision.c`)

Si una esquina del soporte físico de la cámara o un reflejo permanente del lente cae siempre sobre las mismas celdas de la rejilla, el módulo `mascara_vision.c` permite marcar esas celdas en una máscara de bits (`10×10`) para que el clasificador las ignore sin perder sensibilidad en el resto del campo visual.

## 8.3 Detector neuronal `EfficientDet-Lite0` en hilo dedicado (`ModeloEnHilo.kt`)

En paralelo al clasificador de piso en `C99`, la aplicación Android ejecuta el modelo cuantizado **`efficientdet_lite0.tflite`** (`4.4 MB` en `assets/`) mediante `DetectorTflite.kt`. Dado que una inferencia de red neuronal tarda más que el cálculo cromático de celdas, `ModeloEnHilo.kt` la corre en un **hilo de fondo exclusivo** para no frenar la recepción de video ni el ciclo del navegador:

- El hilo toma el cuadro más reciente, ejecuta el modelo y publica una lista de cajas delimitadoras $(x_{\min}, y_{\min}, x_{\max}, y_{\max})$ con su clase e índice de confianza (`score`), estampando la marca de tiempo de finalización.
- El módulo de fusión considera vigentes esos resultados únicamente durante **`300 ms`** (`VIGENCIA_MODELO_MS`); si un resultado supera esa edad, se descarta automáticamente para no reaccionar ante objetos que ya quedaron atrás.
- Adicionalmente, el módulo en `C99` `cualquiera.c` ofrece el modo opcional **cualquier objeto (`obj-any`)**, que evalúa detecciones crudas por encima de un umbral reducido (`score >= 0.25`) filtrando cajas demasiado pequeñas (`área < 2 %`) o que cubran casi toda la imagen (`área > 75 %`, típicamente el propio piso).

## 8.4 Corredor de perspectiva (`corredor.c`) y fusión multimodal (`fusion.c`)

Un objeto visible cerca de los bordes izquierdo o derecho de la imagen no estorba el paso si el carro va a seguir recto. Para distinguir un objeto **en la vía** de un objeto **al costado**, `corredor.c` proyecta el ancho físico del vehículo (`150 mm` más un margen lateral de seguridad de `50 mm` a cada lado = `250 mm` totales) sobre cada fila $r \in [0, 9]$ de la imagen usando la altura de la cámara (`115 mm`), su inclinación hacia el piso (`12°`) y su campo visual horizontal (`56°`).

Como resultado de la perspectiva cónica, el corredor proyectado forma un **trapecio** que abarca desde las columnas `2 a 7` en la parte inferior cercana (`fila 9`, junto a las ruedas) hasta estrecharse en las columnas centrales (`4 a 5`) cerca del horizonte (`filas 3 y 4`).

### Reglas de decisión en `fusion.c` y autoridad del láser

El módulo `fusion.c` cruza las celdas anómalas del clasificador de piso y las cajas vigentes de la red neuronal contra el trapecio del corredor:

| Condición observada en las fuentes | Salida de `fusion_decidir` | Acción resultante en `navegador.c` |
|---|---|---|
| Sin anomalías en el corredor ni cajas vigentes | `FUSION_LIBRE` (`0`) | Continúa avanzando por la ruta con control `RUMBO`. |
| Objeto o anomalía detectada solo fuera del trapecio del corredor | `FUSION_FUERA_CORREDOR` (`1`) | Muestra aviso visual en pantalla pero **no frena** el avance recto. |
| Anomalía de piso o caja de red neuronal dentro del corredor en filas de riesgo (`fila >= 5`) | `FUSION_EN_CORREDOR` (`2`) + `lado_libre` (`+1` izq / `-1` der) | Pasa a `NAV_DETENIDO_OBSTACULO`, confirma durante `300 ms` y lanza el esquive hacia el costado más despejado. |
| **Autoridad opcional del láser frontal (`usar_tof_en_ruta = 1`):** `VL53L0X` mide `< 400 mm` (`NAV_TOF_ESQUIVE_MM`) | Inyecta obstáculo en columnas centrales (`3 a 6`, `fila 7`) | Garantiza la detención y el inicio del esquive aunque la caja sea del mismo color que el piso y la red neuronal no la reconozca. |

## 8.5 Sincronización multihilo en JNI (`candado_vision` — Cierre J16)

En la aplicación Android intervienen concurrentemente varios hilos que invocan funciones nativas del puente `JNI` (`puente.c`): el hilo de recepción de cuadros de cámara (que procesa imágenes y alimenta el clasificador `g_obs`), el hilo de la interfaz gráfica (que consulta las celdas, actualiza umbrales desde `VisionActivity` o pulsa `Aprender piso`) y el ciclo de supervisión de ruta.

En la auditoría previa al cierre **J16** se identificó que las estructuras globales de visión en `puente.c` (`g_obs`, `g_cfg_corredor`, `g_cfg_fusion`, `g_cfg_cualq` y `g_mascara`) podían ser leídas y modificadas al mismo tiempo desde dos hilos de Kotlin sin exclusión mutua. Para eliminar cualquier riesgo de corrupción de memoria o lectura desgarrada (*torn read*), el cierre **J16** introdujo en `puente.c` un **candado mutex multiplataforma (`candado_vision`)** —implementado con `pthread_mutex_t` en Android/Linux y `CRITICAL_SECTION` en Windows— junto con las primitivas `vision_lock()` y `vision_unlock()` que envuelven todas las entradas `JNI` de visión y aprendizaje de suelo.

Dónde está en el código: `obstaculos.h`, `obstaculos.c`, `corredor.h`, `corredor.c`, `fusion.h`, `fusion.c`, `cualquiera.c`, `mascara_vision.c`, `puente.c`, `DetectorTflite.kt`, `ModeloEnHilo.kt`.

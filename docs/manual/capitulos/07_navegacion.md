# 7. Núcleo de navegación en C

Este capítulo explica la arquitectura matemática del núcleo de navegación escrito en estándar `C99` (`nucleo_c`), que se compila tanto dentro de la aplicación Android (vía `JNI`) como en computadora personal para las suites de pruebas y el simulador Monte Carlo. Describe el modelo de cuadrícula, las ecuaciones de calibración longitudinal y angular, la máquina de estados del navegador y las mejoras introducidas en los cierres **J15** y **J16**: integración de pose en décimas de grado (`rumbo_x10`), guiado continuo por punto de mira (`RUMBO`), geometría de esquive con margen longitudinal de `150 mm`, corrección de `override_largo_mm` y protección ante caída a giro por tiempo (`descartar_dt`).

## 7.1 Por qué se navega por tiempo con giroscopio y cómo funciona la cuadrícula

Los cuatro motores `TT` del vehículo carecen de codificadores ópticos o magnéticos (*encoders*) en sus ejes, y el entorno interior tampoco dispone de balizas externas. Por ello, el sistema divide la estimación cinemática en dos variables complementarias:

1. **Distancia longitudinal por tiempo calibrado:** cada milímetro de avance se relaciona con un número de milisegundos de activación de los motores en perfil `AUTO`, compensando el retardo de arranque inicial y el deslizamiento por inercia al frenar.
2. **Orientación por integración del giroscopio (`MPU6050`):** el ángulo real del vehículo se mide a `200 Hz` en el firmware y se transmite al navegador cada `100 ms` en décimas de grado (`SENS`), lo que permite cerrar los giros por ángulo medido (`GIRO:<grados>`), mantener el rumbo en rectas (`RUMBO:<décimas>`) y proyectar cada incremento de avance en la dirección verdadera a la que apunta el chasis.

### Cuadrícula de planificación (`trayectoria.c` y `ruta_editor.c`)

El usuario dibuja o selecciona rutas sobre una cuadrícula cartesiana donde **cada celda mide `20 cm × 20 cm` (`200 mm`)**. El eje $+Y$ apunta hacia el frente inicial del carro (`rumbo = 0°`), el eje $+X$ apunta hacia la derecha (`rumbo = -90°`) y los ángulos positivos ($+\theta$) miden giros hacia la izquierda (sentido antihorario visto desde arriba).

Dada una lista de celdas consecutivas $(c_k, f_k)$, el módulo `trayectoria.c` fusiona los segmentos colineales y genera una lista compacta de pasos elementales (`Paso`):

- **`PASO_GIRAR`:** contiene el cambio de orientación requerido en grados (`grados`) y su duración de respaldo por tiempo (`duracion_ms`).
- **`PASO_AVANZAR`:** contiene la distancia longitudinal del tramo en milímetros (`distancia_mm`), su duración calibrada (`duracion_ms`) y la celda destino.

[FIGURA:navegacion|Ciclo de control del núcleo de navegación en C99: planificación en cuadrícula, guiado en recta con RUMBO, giro cerrado con MPU6050 e integración continua de pose en décimas de grado.]

## 7.2 Modelos de calibración longitudinal (`calibracion.c`) y angular (`calgiro.c`)

### Conversión entre distancia y tiempo (`calibracion.c`)

Un tramo recto de longitud $d_{\text{mm}}$ (o $d_{\text{cm}} = d_{\text{mm}}/10$) no es simplemente proporcional al tiempo: al energizar los motores existe un tiempo muerto de arranque $t_{\text{arr}}$ (`arranque_ms`, por defecto `120 ms` de latencia mecánica efectiva) y, al enviar `STOP`, el vehículo recorre todavía una distancia de frenado por inercia $d_{\text{freno}}$ (`frenado_mm`). Con una pendiente calibrada $m$ en milisegundos por centímetro (`ms_por_cm`, valor base `50 ms/cm`, equivalente a `200 mm/s`), el tiempo de comando $T(d_{\text{mm}})$ y su función inversa $\hat{d}(t)$ son:

$$T(d_{\text{mm}}) = t_{\text{arr}} + \max\left(0,\ d_{\text{mm}} - d_{\text{freno}}\right) \cdot \frac{m}{10}, \qquad \hat{d}(t) = \max\left(0,\ t - t_{\text{arr}}\right) \cdot \frac{10}{m}$$

Por ejemplo, con $m = 50\text{ ms/cm}$, $t_{\text{arr}} = 120\text{ ms}$ y $d_{\text{freno}} = 20\text{ mm}$, para recorrer una celda de $200\text{ mm}$ ($20\text{ cm}$) el navegador programa $T(200) = 120 + (200 - 20) \times 5 = 1{,}020\text{ ms}$.

En el cierre **J16** se corrigió además una sutileza importante en las rutas predefinidas de un solo paso con longitud personalizada (`override_largo_mm > 0`, como `Recta 150 cm` que va de `(0,0)` a `(0,1)` pero mide `1,500 mm` en lugar de `200 mm`): antes de J16, `ModoSemiautonomo.kt` recalculaba la duración a partir de los dos puntos de cuadrícula (`200 mm`) y sobrescribía los `1,500 mm`. Ahora `ModoSemiautonomo` respeta `override_largo_mm` y calcula tanto `distancia_mm = 1500` como su `duracion_ms` real a partir de la calibración activa.

### Cuerda de pivote excéntrico y escala del giroscopio (`calgiro.c`)

Si el centro de giro efectivo del vehículo no coincide exactamente con el punto de referencia de medición (excentricidad o radio de pivote $R$ en milímetros), al girar un ángulo $\Delta\theta$ sobre su sitio el punto de referencia describe un arco cuya **cuerda geométrica** es:

$$c(\Delta\theta) = 2 R \sin\left(\frac{|\Delta\theta|}{2}\right)$$

El módulo `calgiro.c` permite estimar este radio a partir de la cuerda medida tras un giro de `90°` ($R = c_{90} / \sqrt{2}$) y proyecta ese desplazamiento al integrar giros grandes (`pivote_mm`). Asimismo, en el cierre **J16** se incorporó la función **`calgiro_escala`**, que calcula el ajuste en partes por millón (`escala_ppm`) para el giroscopio a partir del ángulo reportado por el `MPU6050` tras una vuelta física completa de `360°` sobre una marca en el piso (o del promedio de giros en ambos sentidos para cancelar asimetrías de colocación):

$$\Delta\text{ppm} = \text{round}\left(\frac{360.0 - |\theta_{\text{medido}}|}{|\theta_{\text{medido}}|} \times 1{,}000{,}000\right)$$

Por ejemplo, si tras girar exactamente `360.0°` físicos el giroscopio reporta `354.6°` (subestima un `1.5 %`), `calgiro_escala` devuelve `+15,228 ppm`, que se suma a la escala vigente y se guarda en `NVS` con `IMU_ESCALA:15228`.

## 7.3 Máquina de estados, integración en décimas de grado y guiado `RUMBO`

El módulo `navegador.c` gobierna la ejecución de la ruta mediante siete estados explícitos:

| Estado (`EstadoNav`) | Comando emitido | Condición de entrada y función principal |
|---|---|---|
| `NAV_ESPERANDO` | `STOP` | Estado inicial o tras cancelar; espera `navegador_comenzar`. |
| `NAV_SIGUIENDO` | `FORWARD` / `LEFT` / `RIGHT` + `RUMBO` | Ejecuta la secuencia de pasos (`PASO_GIRAR` y `PASO_AVANZAR`) hacia el siguiente nodo de la ruta. |
| `NAV_DETENIDO_OBSTACULO` | `STOP` | Pausa de confirmación (`300 ms`) al detectar un obstáculo dentro del corredor frontal antes de decidir esquivar. |
| `NAV_ESQUIVANDO` | Giros y avances de desvío | Ejecuta el rodeo lateral (`400 mm` a izquierda o derecha) y el avance paralelo para rebasar la caja. |
| `NAV_REINCORPORANDO` | Giro y avance hacia la ruta | Regresa desde el carril paralelo hacia un punto situado `300 mm` por delante del obstáculo sobre la línea original. |
| `NAV_AYUDA` | Manual del operador | Cede el mando al usuario cuando ambos lados están bloqueados o se activa un paro de supervisión; espera hasta `3 min` a que pulse `Continuar`. |
| `NAV_TERMINADO` / `NAV_ERROR` | `STOP` | Fin exitoso del último paso de la ruta o aborto por fallo irrecuperable. |

### Integración de pose en décimas de grado (`rumbo_x10` — Cierres J15 y J16)

Antes del cierre J15, el navegador suponía que durante todo un paso de avance el carro se movía exactamente con el ángulo ideal del tramo ($0^\circ, 90^\circ, 180^\circ$) y solo actualizaba $(x, y)$ al terminar el paso. En J15 se introdujo la integración incremental cada `100 ms` usando el ángulo del giroscopio, y en **J16** se eliminó el truncamiento a grados enteros: ahora `PoseNav` almacena y propaga **`rumbo_x10` en décimas de grado** (por ejemplo, `15` equivale a `1.5°`, mientras que en entero se truncaba a `1°` perdiendo un tercio del ángulo). En cada intervalo $\Delta t$ de avance activo con distancia incremental $\Delta s = \hat{d}(t) - \hat{d}(t_{\text{ant}})$, la posición estimada se actualiza con el ángulo real medido:

$$x_{\text{mm}} \leftarrow x_{\text{mm}} - \Delta s \cdot \sin\left(\frac{\text{rumbo\_x10}}{10} \cdot \frac{\pi}{180}\right), \qquad y_{\text{mm}} \leftarrow y_{\text{mm}} + \Delta s \cdot \cos\left(\frac{\text{rumbo\_x10}}{10} \cdot \frac{\pi}{180}\right)$$

*(El signo menos en $x_{\text{mm}}$ obedece a que $\text{rumbo\_x10} = 0$ apunta hacia $+Y$ y los giros positivos $+\theta$ van hacia $-X$, es decir, hacia la izquierda).*

Además, el cierre **J16** corrigió una condición de carrera sutil al caer de giro por giroscopio a giro por tiempo (`descartar_dt`): si el sensor `MPU6050` deja de responder en mitad de un giro (`GIRO_ERR_IMU` o silencio `> 500 ms`), el navegador conmuta a giro por tiempo y activa la bandera `descartar_dt = 1` para que el primer ciclo de respaldo no multiplique el tiempo acumulado previo por la velocidad angular nominal.

### Guiado continuo en rectas por punto de mira (`RUMBO` — Cierre J16)

Incluso si el controlador `PI` del firmware mantiene un ángulo constante, un pequeño error inicial al terminar el giro previo haría que el carro caminara en una recta paralela o divergente respecto al segmento deseado. Por ello, durante cada avance en `NAV_SIGUIENDO`, `navegador_rumbo_Correccion_x10` calcula un **punto de mira (*pure pursuit / lookahead*) situado `400 mm` por delante** sobre el segmento ideal de la ruta (o en el vértice final del tramo si faltan menos de `400 mm`):

1. Proyecta la pose actual $(x_{\text{mm}}, y_{\text{mm}})$ sobre el vector unitario del tramo para obtener la coordenada longitudinal alcanzada y el error transversal respecto a la línea.
2. Ubica el punto objetivo $P_{\text{mira}}$ a `400 mm` por delante sobre la recta original y calcula su acimut deseado $\theta_{\text{mira}}$.
3. Acota la corrección máxima a **`±12.0°`** (`NAV_RUMBO_MAX_CORR_X10 = 120`) respecto al rumbo nominal del tramo para evitar virajes bruscos, y envía la consigna **`RUMBO:<décimas>`** al firmware.
4. **Supresión de giros detenidos pequeños ($\le 8.0^\circ$):** al llegar al inicio de un nuevo tramo en `NAV_SIGUIENDO`, si el carro tiene `RUMBO` disponible y el cambio de orientación requerido no supera **`8.0°`** (`NAV_GIRO_EN_MARCHA_MAX_X10 = 80`), el navegador **omite el paso `PASO_GIRAR` detenido** (que antes perdía precisión por la banda muerta de `±2°` y el frenado/arranque extra) y deja que el controlador `PI` de `RUMBO` absorba esa pequeña desviación suavemente en marcha.

## 7.4 Geometría de esquive en tres tramos y margen J16 de `150 mm`

Cuando el vehículo avanza en `NAV_SIGUIENDO` y el subsistema de percepción confirma durante `300 ms` un obstáculo dentro del corredor frontal a una distancia estimada $d_{\text{obs}}$ (acotada al rango `[150, 700] mm`), el navegador construye una maniobra de rodeo y reincorporación en tres tramos:

[FIGURA:esquive|Geometría de la maniobra de esquive en tres tramos con margen longitudinal J16 de 150 mm y estado validado físicamente en el carro (escenario B6).]

1. **Tramo 1 — Desvío lateral (`NAV_TRAMO_ESQUIVAR_CM = 40 cm` = `400 mm`):** el navegador consulta qué costado tiene más espacio libre en la imagen (`lado_libre`: izquierda `+1` o derecha `-1`). Gira `90°` hacia ese lado, avanza `400 mm` para salir de la línea del obstáculo y gira `-90°` para quedar nuevamente paralelo a la ruta original.
2. **Tramo 2 — Avance paralelo de rebase con margen J16 (`NAV_PASO_MARGEN_MM = 150 mm`):** para rebasar por completo el cuerpo del obstáculo (cuya profundidad supuesta es `NAV_PROFUNDIDAD_OBS_MM = 250 mm`), el carro avanza en paralelo una distancia:
   $$d_{\text{paso}} = \text{clamp}_{[400, 1100]}\left(d_{\text{obs}} + 250\text{ mm} + 150\text{ mm}\right)$$
   En el cierre **J16** se amplió este margen de seguridad de `100 mm` a **`150 mm`** (`+5 cm` adicionales) porque cuando la distancia longitudinal se estima por tiempo con baterías parcialmente descargadas, un margen corto hacía que el vehículo iniciara el regreso diagonal rozando la esquina trasera de la caja.
3. **Tramo 3 — Reincorporación calculada desde la pose integrada (`NAV_REINC_ADELANTE_CM = 30 cm` = `300 mm`):** al concluir el rebase paralelo, en lugar de ejecutar un regreso ciego de `-90°` y `400 mm` (que ignoraba cualquier desviación sufrida durante el rodeo), `iniciar_reincorporacion` toma la pose real integrada $(x_{\text{actual}}, y_{\text{actual}}, \text{rumbo\_x10})$ y calcula el vector directo $(\Delta x, \Delta y)$ hacia un punto de reincorporación situado sobre la línea original **`300 mm` por delante** del borde trasero del obstáculo (o hacia el destino final de la ruta si está más cerca). A partir de $(\Delta x, \Delta y)$ genera un giro hacia $\text{atan2}(-\Delta x, \Delta y)$, el avance por la hipotenusa $\sqrt{\Delta x^2 + \Delta y^2}$ y la alineación final con la ruta.

Dónde está en el código: `navegador.h`, `navegador.c`, `trayectoria.h`, `trayectoria.c`, `calibracion.h`, `calibracion.c`, `calgiro.h`, `calgiro.c`, `ruta_editor.c`, `simulador_navegador.c`.

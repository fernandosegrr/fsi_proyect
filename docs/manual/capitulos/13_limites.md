# 13. Límites y trabajo futuro

Este capítulo analiza con rigor las limitaciones físicas, sensoriales y algorítmicas inherentes al diseño actual del vehículo **LIDIA**, explica por qué ocurren y propone las líneas de trabajo futuro (`PENDIENTE`) para evoluciones posteriores del proyecto. Reconocer explícitamente estos límites impide extrapolar los resultados fuera de las condiciones reales en que fueron obtenidos.

## 13.1 Límites estructurales de locomoción y odometría

### 1. Estimación de distancia longitudinal en lazo abierto (`ms_por_cm`)

El límite más importante del prototipo es la ausencia de codificadores de giro (*encoders*) en los cuatro motores `TT`. Aunque el cierre **J16** cerró el lazo de orientación en rectas y giros mediante el giroscopio `MPU6050` (`RUMBO` y `GIRO`), **la distancia recorrida hacia adelante sigue calculándose a partir del tiempo de activación (`ms_por_cm`)**. En consecuencia:

- Cuando el voltaje del portapilas `4×AA` disminuye con el uso, o si el carro pasa de un piso liso a una superficie más rugosa, la velocidad lineal baja y el vehículo recorre menos milímetros de los que el modelo de tiempo estima.
- Este fenómeno explica por qué fue necesario ampliar en J16 el margen longitudinal de esquive a `150 mm` (`NAV_PASO_MARGEN_MM`): con un margen menor, una caída del `8 %` en la velocidad real hacía que el carro iniciara la reincorporación antes de librar por completo la caja.

### 2. Deslizamiento lateral de rodillos *mecanum* invisible al giroscopio

Las cuatro llantas del chasis poseen rodillos oblicuos a `45°` propios de una rueda *mecanum*. Cuando el vehículo frena, arranca bruscamente o corrige su rumbo, el rozamiento desigual entre los rodillos puede producir un pequeño **desplazamiento lateral puro (*side-slip*) de algunos centímetros sin que cambie el ángulo de guiñada $\theta$ del chasis**. Como el giroscopio `MPU6050` únicamente mide velocidad angular de rotación ($\omega_z$) y no aceleración ni velocidad lineal transversal, ese deslizamiento lateral resulta invisible para el estimador de pose y contribuye a la dispersión lateral residual de `~0 cm` a `~10 cm` observada físicamente en las corridas B6.

### 3. Giros residuales menores a `5°` ejecutados por tiempo

Para evitar que el controlador `GIRO:<grados>` del firmware oscile dentro de su ventana de tolerancia (`±2.0°`), el navegador en `C99` solo envía `GIRO:<grados>` cuando el giro solicitado es de al menos **`5°`** (`|grados| >= 5`). Cuando al final de una reincorporación queda un ajuste angular menor a `5°`, el navegador recurre a un pulso corto por tiempo (`calgiro_duracion_ms`). Como se observó en la **Corrida 2** del escenario físico B6, un pulso tan breve puede no vencer de manera uniforme la fricción estática de las cuatro ruedas, dejando una pequeña inclinación residual (como ocurrió también en la Corrida 3). Con el control `RUMBO` activo, si después de ese giro sigue un tramo recto, el propio lazo `PI` corrige la inclinación en marcha; pero si es el último paso de la ruta, la orientación final conserva ese pequeño residuo.

## 13.2 Límites geométricos, visuales y de protocolo

| Límite identificado en el código / hardware | Valor o condición actual | Consecuencia operativa y mitigación vigente |
|---|---|---|
| **Profundidad fija supuesta del obstáculo (`NAV_PROFUNDIDAD_OBS_MM`)** | `250 mm` (`25 cm`) y desvío lateral fijo de `400 mm` (`40 cm`) | Diseñado para cajas estándar de `20×20 cm`. Ante un obstáculo mucho más profundo (> `40 cm`) o más ancho (`40×20 cm`, `NO MEDIDO`), el rebase de un solo tramo puede ser insuficiente y requerir un segundo esquive o entrar en `AYUDA`. |
| **Objetos del mismo color que el piso y no reconocidos por `COCO`** | Clasificador `YCbCr` + `EfficientDet-Lite0` | Si un obstáculo tiene exactamente la misma crominancia que la baldosa y no pertenece a una clase reconocible por la red neuronal, la cámara puede no marcarlo; la defensa es habilitar `usar_tof_en_ruta` (esquive láser a `400 mm`) y el freno local `TOF_PARO` a `120 mm`. |
| **Geometría puntual del láser `VL53L0X`** | Cono frontal de `~25°` a la altura del borde delantero | Objetos muy bajos (< `4 cm`, por debajo del haz) o muy estrechos desplazados hacia una rueda pueden no reflejar el pulso láser. |
| **Límite de 6 dígitos en el comando `RUMBO:<décimas>`** | Máximo `±99999` décimas (`~±10,000°` $\approx 27.7$ vueltas completas) | En cualquier ruta normal o cuadrado los ángulos se mantienen dentro de `±360°`; solo una rotación continua en espiral de más de 27 vueltas sin recalibrar `CAL_IMU` excedería el formato de 6 caracteres. |
| **Transición a `AYUDA` en 1 de 6 corridas físicas B6** | Corrida 5 (`J16_B6_r5`, paso `4/10`) | El vehículo se detuvo de forma segura sin tocar la caja pero solicitó ayuda teniendo espacio físico. Para diagnosticar su causa exacta se requiere un registrador persistente de telemetría por corrida. |

## 13.3 Resumen de elementos NO MEDIDOS y trabajo futuro (`PENDIENTE`)

Para orientar a futuros equipos o siguientes iteraciones del proyecto, las mejoras pendientes se ordenan por impacto técnico:

1. **Completar la campaña de pruebas físicas en el carro (`NO MEDIDO` → `VALIDADO`):**
   - Ejecutar con baterías al 100 % las series físicas de **recta de 150 cm con y sin `RUMBO`** (`B5`), **ruta en L** (`B7`), **cuadrado de 60 cm** (`B8`), **calibración en piso de `IMU_ESCALA`** (giro de `360°`), **corte inducido de WiFi en marcha** (`B9`), **freno autónomo aislado por láser** (`120 mm`) y **obstáculo ancho de `40×20 cm`**.
2. **Incorporar odometría de rueda o flujo óptico calibrado (`PENDIENTE`):**
   - Añadir sensores Hall o codificadores ópticos de ranura en al menos dos ejes de motor `TT` para medir directamente los milímetros recorridos y independizar la distancia longitudinal del voltaje de las pilas `AA`.
3. **Reemplazar el puente H doble por cuatro canales independientes (`PENDIENTE`):**
   - Sustituir el driver de dos canales `TC1508A` por dos puentes H dobles (4 canales PWM independientes) para aprovechar la cinemática omnidireccional completa de las ruedas *mecanum* (desplazamiento lateral puro sin necesidad de girar `90°` para esquivar).
4. **Registrador automático de caja negra por corrida (`PENDIENTE`):**
   - Guardar en almacenamiento interno del teléfono un archivo `.jsonl` o `.csv` por cada ejecución de ruta con todas las tramas `SENS`, decisiones de `fusion.c` y estados de `navegador.c` para diagnosticar con certeza eventos como la parada en `AYUDA` de la Corrida 5.
5. **Medición eléctrica instrumentada (`NO MEDIDO`):**
   - Caracterizar con amperímetro el perfil de corriente en arranque (`250 ms` a `PWM 255`), avance continuo y bloqueo, así como verificar en banco el circuito externo de `GPIO 2`.

Dónde está en el código: `limites_conocidos.md`, `estado_20261110.md`, `reporte_J16.md`, `navegador.h`, `config.h`.

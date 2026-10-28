# Recorrido de código: Fusión sensorial (`fusion.c`)

**Ubicación:** `nucleo_c/fusion.h` y `nucleo_c/fusion.c`  
**Lenguaje:** C estándar (C99)  
**Propósito:** Integrar y corroborar las salidas de múltiples modalidades de percepción (segmentación de piso, cajas delimitadoras de redes neuronales y telemetría ToF) para una toma de decisiones robusta.

---

## 1. Motivación y redundancia sensorial

Cada método de detección de obstáculos por separado presenta fortalezas y debilidades particulares:

| Sensor / Algoritmo | Ventajas | Limitaciones y puntos débiles |
|---|---|---|
| **Segmentación de color de piso** | Computacionalmente muy ligero; detecta cualquier objeto independientemente de su forma. | Sensible a reflejos intensos, sombras oscuras arrojadas por muebles o variaciones de luz solar. |
| **Detector neuronal (MobileNet/SSD)** | Inmune a sombras suaves; clasifica objetos específicos (cajas, personas, botellas). | Exige mayor consumo de CPU/GPU en el móvil; latencia de inferencia de 30 a 60 ms por cuadro; puede ignorar objetos atípicos fuera de su conjunto de entrenamiento. |
| **Sensor láser ToF (VL53L0X)** | Determinista, independiente de la iluminación ambiente; medición directa milimétrica. | Campo de visión muy estrecho (cono de ~25°); no detecta objetos ubicados hacia los costados del vehículo. |

`fusion.c` combina estas tres fuentes para maximizar la fiabilidad y minimizar tanto falsos positivos (frenados fantasma) como falsos negativos (colisiones).

---

## 2. Estructuras de datos

Definidas en `nucleo_c/fusion.h`:

```c
typedef struct {
    int x1, y1, x2, y2;     /* Coordenadas en píxeles del cuadro delimitador de la red neuronal */
    int clase;              /* Identificador de categoría (ej. caja de cartón) */
    float confianza;        /* Nivel de probabilidad de la inferencia (0.0 a 1.0) */
} DeteccionObjeto;

typedef struct {
    int obstaculo_presente;     /* 1 si se confirma presencia de obstáculo que requiere acción */
    int distancia_mm;           /* Distancia fusionada más representativa */
    float certeza_combinada;    /* Índice de confianza de 0.0 a 1.0 */
    int origen_principal;       /* 1 = láser ToF, 2 = visión color, 3 = red neuronal, 4 = corroborado */
} FusionPercepcion;
```

---

## 3. Lógica de arbitraje y combinación

El algoritmo de fusión implementado en `nucleo_c/fusion.c` aplica la siguiente jerarquía de decisión:

1. **Prioridad 1: Veto inmediato por ToF:**  
   Si el sensor láser reporta una distancia válida menor al umbral de peligro inminente ($d_{\text{ToF}} \le \text{UMBRAL\_CRITICO}$), el sistema activa de inmediato `obstaculo_presente = 1` con `certeza = 1.0` y origen ToF, sin esperar la llegada del siguiente cuadro de video.
2. **Prioridad 2: Corroboración multimodal (Color + Red Neuronal):**  
   Si la red neuronal reporta una caja delimitadora con confianza $\ge 0.50$ y, simultáneamente, la segmentación cromática detecta acumulación de píxeles *no-piso* dentro de esa misma región y en el interior del corredor trapezoidal:
   - Se confirma el obstáculo con alta certeza (`origen = 4`).
   - Se estima la distancia al frente combinando la posición vertical del contacto de la caja con el suelo y la lectura del láser.
3. **Prioridad 3: Anomalía visual aislada:**  
   Si solo la segmentación de color detecta una anomalía pero la red neuronal no reconoce un objeto y el láser marca camino despejado:
   - Se requiere un mayor número de fotogramas de persistencia temporal para descartar sombras pasajeras antes de ordenar un frenado preventivo.

---

## 4. Pruebas unitarias asociadas

- `nucleo_c/pruebas_fusion.c`: Simula combinaciones de entradas concurrentes (detección por visión sin láser, láser activo con cámara ciega, coincidencia plena y contradicciones) para verificar que el arbitraje responda siempre de la forma más segura.

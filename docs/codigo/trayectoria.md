# Recorrido de código: Planificación de trayectoria (`trayectoria.c` y `ruta_editor.c`)

**Ubicación:** `nucleo_c/trayectoria.h`, `nucleo_c/trayectoria.c`, `nucleo_c/ruta_editor.h`, `nucleo_c/ruta_editor.c`  
**Lenguaje:** C estándar (C99)  
**Propósito:** Convertir una secuencia de waypoints en una cuadrícula métrica en una lista ordenada de pasos cinemáticos temporizados listos para su ejecución.

---

## 1. Estructuras fundamentales

Definidas en `nucleo_c/trayectoria.h`:

```c
typedef struct {
    int32_t x_mm;   /* Coordenada X en milímetros */
    int32_t y_mm;   /* Coordenada Y en milímetros */
} Punto;

typedef struct {
    ComandoNav comando;     /* NAV_CMD_FORWARD, NAV_CMD_LEFT, NAV_CMD_RIGHT, NAV_CMD_STOP */
    uint32_t   duracion_ms; /* Tiempo nominal de encendido del comando en milisegundos */
    int32_t    grados_x10;  /* Si es giro, ángulo acumulado esperado multiplicado por 10 */
    int32_t    distancia_mm;/* Si es avance, distancia lineal prevista en milímetros */
} Paso;
```

---

## 2. Algoritmo de generación de pasos (`trayectoria_calcular_pasos`)

`nucleo_c/trayectoria.c:45-160`:
1. **Validación inicial:** Verifica que la lista contenga al menos dos puntos y que los punteros sean válidos.
2. **Seguimiento de rumbo:** Mantiene un registro del rumbo actual del vehículo (inicialmente 0° apuntando hacia el eje Y positivo).
3. **Por cada segmento consecutivo $(P_i \rightarrow P_{i+1})$:**
   - Calcula el vector desplazamiento:
     $$\Delta x = P_{i+1}.x - P_i.x, \quad \Delta y = P_{i+1}.y - P_i.y$$
   - Calcula la distancia euclidiana del tramo:
     $$d = \sqrt{\Delta x^2 + \Delta y^2}$$
   - Determina el ángulo objetivo del segmento mediante arcotangente (`atan2`).
   - Si el rumbo actual difiere del ángulo objetivo:
     - Calcula la diferencia angular más corta ($[-180^\circ, +180^\circ]$).
     - Determina el sentido del pivote (`NAV_CMD_LEFT` si es positivo, `NAV_CMD_RIGHT` si es negativo).
     - Utiliza los parámetros de calibración (`ms_por_grado`) para determinar la duración del giro o marca el ángulo para el bucle cerrado del firmware.
     - Agrega un paso de giro a la lista y actualiza el rumbo de referencia.
   - Si la distancia lineal $d$ es mayor a cero:
     - Aplica la calibración de avance: resta el arrastre por inercia esperado (`frenado_mm`), añade el tiempo de arranque pleno (`arranque_ms`) y calcula la duración del comando `NAV_CMD_FORWARD` con la pendiente `ms_por_cm`.
     - Agrega el paso de avance a la lista.
4. **Cierre de trayectoria:** Inserta un paso final `NAV_CMD_STOP` al concluir el último punto.

---

## 3. Módulo `ruta_editor.c`: Adaptación de la cuadrícula interactiva

`nucleo_c/ruta_editor.c` actúa como intermediario con el lienzo de dibujo de la aplicación Android:
- Transforma índices discretos de celdas $(columna, fila)$ en coordenadas métricas continuas ($1\text{ celda} = 200\text{ mm}$).
- Aplica filtros de consistencia: detecta y rechaza saltos incoherentes entre celdas no contiguas.
- Permite simplificar trayectorias continuas colineales: une dos avances consecutivos en la misma dirección en un solo paso de mayor longitud para evitar frenados innecesarios.

---

## 4. Pruebas unitarias asociadas

- `nucleo_c/pruebas_trayectoria.c`: Evalúa trayectorias rectas, trayectorias en forma de «L», cuadrados cerrados y circuitos en zigzag.
- `nucleo_c/pruebas_ruta_editor.c`: Valida la conversión bidireccional entre la cuadrícula de la pantalla táctil y el espacio métrico real.

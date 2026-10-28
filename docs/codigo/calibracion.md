# Recorrido de código: Calibración cinemática (`calibracion.c` y `calgiro.c`)

**Ubicación:** `nucleo_c/calibracion.h`, `nucleo_c/calibracion.c`, `nucleo_c/calgiro.h`, `nucleo_c/calgiro.c`  
**Lenguaje:** C estándar (C99)  
**Propósito:** Definir los modelos matemáticos y algoritmos de ajuste lineal que transforman distancias métricas y ángulos geométricos en tiempos de activación de motores.

---

## 1. La estructura `CalibracionCarro`

Definida en `nucleo_c/calibracion.h`:

```c
typedef struct {
    int ms_por_cm_x10;          /* Milisegundos por centímetro multiplicados por 10 (ej. 400 = 40.0 ms/cm) */
    int frenado_mm;             /* Distancia de deslizamiento por inercia tras el corte de motor en mm */
    int arranque_ms;            /* Duración del pulso de arranque pleno para vencer fricción estática */
    int ms_por_grado_izq_x10;   /* Milisegundos por grado en pivote a la izquierda x10 */
    int ms_por_grado_der_x10;   /* Milisegundos por grado en pivote a la derecha x10 */
    int frenado_giro_izq_x10;   /* Deslizamiento inercial angular a la izquierda (grados x10) */
    int frenado_giro_der_x10;   /* Deslizamiento inercial angular a la derecha (grados x10) */
    int balance_auto;           /* Factor de balance del lado izquierdo en AUTO (800 a 1000) */
    int balance_manual;         /* Factor de balance del lado izquierdo en MANUAL (800 a 1000) */
} CalibracionCarro;
```

---

## 2. Algoritmos de cálculo lineal

### 2.1 Ajuste de avance lineal (`calibracion_calcular_avance`)
`nucleo_c/calibracion.c`:
- **Entrada:** Dos puntos experimentales $(t_1, d_1)$ y $(t_2, d_2)$ obtenidos en pista con cinta métrica, donde $t$ es el tiempo de comando en milisegundos y $d$ es la distancia física recorrida en milímetros.
- **Validación de datos:** Comprueba que $t_2 > t_1$ y que $d_2 > d_1$. Si las mediciones son incoherentes o idénticas, devuelve `CAL_ERR_DATOS` sin corromper la calibración previa.
- **Cálculo de la pendiente:**
  $$\text{ms/cm} = \frac{(t_2 - t_1) \times 10}{d_2 - d_1}$$
  Guarda el resultado en `ms_por_cm_x10`.

### 2.2 Estimación de frenado inercial (`calibracion_calcular_frenado`)
- A partir de la velocidad calibrada, predice la distancia teórica que debió recorrerse durante el tiempo $t$.
- La diferencia entre la distancia medida físicamente y la distancia calculada se asigna al arrastre por inercia (`frenado_mm`).
- Si el valor excede límites físicos razonables (por ejemplo, más de 30 cm de arrastre), se rechaza con `CAL_ERR_RANGO`.

### 2.3 Calibración de pivotes angulares (`calibracion_calcular_giro`)
- Calcula la constante de milisegundos por grado sexagesimal de manera asimétrica para giros a la izquierda y derecha:
  $$\text{ms/grado} = \frac{t_{\text{giro}} - t_{\text{arranque}}}{\theta_{\text{medido}}}$$

---

## 3. Módulo `calgiro.c`: Calibración adaptativa de giros con IMU

`nucleo_c/calgiro.c` complementa la cinemática básica cuando se dispone del giroscopio MPU6050:
- **Cálculo de anticipación (*lead time*):** Al ordenar un giro de 90°, si el motor se apaga exactamente al leer 90° en el sensor, la inercia del vehículo hace que se pase varios grados (sobretiro).
- `calgiro.c` compara el ángulo comandado con el ángulo final en reposo y calcula la **anticipación angular**: el número de grados antes del objetivo en los que debe cortarse el motor para que el carro se detenga exactamente en el ángulo deseado.

---

## 4. Pruebas unitarias asociadas

- `nucleo_c/pruebas_calibracion.c`: Valida el cálculo de pendientes, rechazo de punteros nulos, tratamiento de divisiones por cero y límites de rango.
- `nucleo_c/pruebas_calgiro.c`: Evalúa la convergencia de la anticipación de giro a lo largo de sucesivas maniobras.

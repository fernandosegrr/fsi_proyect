# Recorrido de código: Detección de obstáculos (`obstaculos.c`, `mascara_vision.c` y `pisos_sinteticos.c`)

**Ubicación:** `nucleo_c/obstaculos.h`, `nucleo_c/obstaculos.c`, `nucleo_c/mascara_vision.h`, `nucleo_c/mascara_vision.c`, `nucleo_c/pisos_sinteticos.h`, `nucleo_c/pisos_sinteticos.c`  
**Lenguaje:** C estándar (C99)  
**Propósito:** Segmentar el piso visual, identificar anomalías cromáticas o de profundidad en la trayectoria de avance y confirmar la presencia de obstáculos para la toma de decisiones evasivas.

---

## 1. Principio de detección por color de piso

En entornos interiores homogéneos (aulas, laboratorios, pasillos), el suelo presenta una tonalidad relativamente uniforme. La estrategia de visión implementada parte de la hipótesis del plano de apoyo:
1. Se aprende el color promedio de referencia del suelo del recinto $(R_{\text{ref}}, G_{\text{ref}}, B_{\text{ref}})$.
2. Cualquier objeto elevado (una caja de cartón, un zapato, una pared) que se interponga en el campo visual reflejará la luz con una intensidad y crominancia distinta a la del suelo.
3. Aquellas regiones que se desvíen de la tolerancia del piso son marcadas como obstáculos potenciales.

---

## 2. Estructura `ResultadoObstaculos`

Definida en `nucleo_c/obstaculos.h`:

```c
typedef struct {
    int obstaculo_confirmado;   /* 1 si el obstáculo ha persistido en varios cuadros consecutivos */
    int distancia_frente_mm;    /* Distancia estimada al obstáculo más cercano al frente */
    int celdas_bloqueadas;      /* Conteo de celdas anómalas dentro del corredor */
    int lado_libre;             /* Lado recomendado para esquivar: -1 = izquierda, +1 = derecha, 0 = ninguno */
    int margen_izq_cm;          /* Espacio libre estimado a la izquierda del obstáculo en cm */
    int margen_der_cm;          /* Espacio libre estimado a la derecha del obstáculo en cm */
} ResultadoObstaculos;
```

---

## 3. Funciones y algoritmos principales

### 3.1 `mascara_vision.c`: Segmentación cromática y filtrado
Calcula la distancia de color Euclídea o por Manhattan entre el píxel observado y el piso aprendido:
$$\Delta C = |R - R_{\text{ref}}| + |G - G_{\text{ref}}| + |B - B_{\text{ref}}|$$
- Si $\Delta C > \text{UMBRAL\_COLOR}$, el bloque se clasifica como *no-piso*.
- Incluye filtrado morfológico de ruido: elimina píxeles anómalos aislados para evitar falsos positivos producidos por brillo de lámparas o reflejos del piso encerado.

### 3.2 `obstaculos.c`: Análisis espacial y confirmación temporal
- **Intersección con el corredor:** Filtra los bloques no-piso; solo aquellos ubicados dentro del trapecio de trayectoria del carro se consideran peligrosos.
- **Ventana de persistencia temporal:** Para evitar frenados bruscos ante caídas de fotogramas o sombras pasajeras, un obstáculo debe detectarse en al menos 2 o 3 cuadros sucesivos (`CONFIRMAR_MUESTRAS`) antes de activar la bandera `obstaculo_confirmado`.
- **Evaluación de laterales:** Suma las celdas despejadas a la izquierda y derecha de la caja para recomendar el sentido de giro evasivo con mayor margen de maniobra.

### 3.3 `pisos_sinteticos.c`: Generador de entornos virtuales de prueba
Dado que no siempre se cuenta con la cámara física montada durante el desarrollo en PC, este módulo genera matrices sintéticas con patrones realistas:
- Pisos de baldosas con líneas de junta oscuras.
- Pisos de madera con vetas de variación cromática.
- Cajas tridimensionales proyectadas con caras iluminadas y sombras proyectadas.
Permite someter los algoritmos de detección a miles de variaciones de contraste de manera automática en las pruebas unitarias.

---

## 4. Pruebas unitarias asociadas

- `nucleo_c/pruebas_obstaculos.c`: Comprueba detección frontal, detección lateral y límites de la matriz.
- `nucleo_c/pruebas_mascara.c`: Valida el clasificador binario de color ante variaciones de iluminación.
- `nucleo_c/pruebas_pisos_dificiles.c`: Prueba la robustez del detector ante pisos con texturas complejas y sombras.

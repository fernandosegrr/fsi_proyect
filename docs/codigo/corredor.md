# Recorrido de código: Corredor de seguridad trapezoidal (`corredor.c`)

**Ubicación:** `nucleo_c/corredor.h` y `nucleo_c/corredor.c`  
**Lenguaje:** C estándar (C99)  
**Propósito:** Proyectar sobre el plano bidimensional de la imagen de la cámara la franja geométrica de paso del vehículo en función de la altura e inclinación óptica.

---

## 1. Fundamento geométrico

Una cámara orientada hacia adelante con cierta inclinación hacia abajo observa el suelo bajo una proyección en perspectiva:
- Los objetos cercanos aparecen en la parte inferior de la imagen con una escala aparente mayor.
- Los objetos lejanos aparecen en la parte superior (cerca de la línea del horizonte) con una escala menor.
- Un carril recto de ancho constante en el mundo físico (el ancho del carro más sus márgenes de seguridad) no se proyecta como un rectángulo en la pantalla, sino como un **trapecio que se estrecha hacia la parte superior**.

```
    Vista de la imagen (Pantalla)
    ┌──────────────────────────┐
    │ ── ── ── ── ── ── ── ──  │  Línea del horizonte
    │        /        \        │
    │       /          \       │  Zona lejana: corredor estrecho
    │      /  Corredor  \      │
    │     /              \     │
    │    /   de seguridad \    │
    │   /                  \   │  Zona cercana: corredor ancho
    └──/────────────────────\──┘
```

---

## 2. Parámetros y estructura `Corredor`

Definida en `nucleo_c/corredor.h`:

- `ancho_carro_mm`: Ancho físico entre las caras exteriores de las ruedas (~180 mm).
- `margen_lateral_mm`: Margen de tolerancia a cada lado para evitar rozar obstáculos (~60 mm).
- `altura_camara_mm`: Altura del lente de la cámara sobre el nivel del suelo.
- `inclinacion_grados`: Ángulo de cabeceo (*pitch*) del eje óptico hacia abajo respecto a la horizontal.
- `limite_cercano_mm` y `limite_lejano_mm`: Rango de distancias en el que el vehículo evalúa obstáculos (típicamente desde 15 cm hasta 120 cm).

---

## 3. Funciones principales

### 3.1 `corredor_iniciar`
Calcula analíticamente los cuatro vértices del trapecio en coordenadas de píxeles:
1. Emplea las ecuaciones de la cámara estenopeica (*pinhole camera model*).
2. Intersecta los rayos ópticos que pasan por los bordes laterales del carro en el suelo con el plano del sensor.
3. Almacena las pendientes de las dos aristas oblicuas del trapecio (lado izquierdo y lado derecho).

### 3.2 `corredor_contiene_punto`
`nucleo_c/corredor.c:110-145`:
- Recibe las coordenadas de un píxel $(x, y)$ o el centro de un bloque de celdas.
- Evalúa si el punto se ubica verticalmente entre la línea del parachoques y el límite lejano.
- Comprueba si su coordenada horizontal se encuentra comprendida entre el límite izquierdo y derecho del trapecio para esa fila de escaneo específica:
  $$X_{\text{izq}}(y) \le x \le X_{\text{der}}(y)$$
- **Retorno:** Devuelve `1` si el punto cae dentro de la franja de peligro, o `0` si está fuera.

---

## 4. Importancia en la navegación reactiva

El corredor es la herramienta fundamental para evitar frenados erráticos:
- Una caja o pared situada a 40 cm a la derecha del vehículo es vista claramente por la cámara, pero cae fuera del trapecio proyectado.
- El sistema sabe que esa caja **no obstruye la trayectoria directa del chasis**, permitiendo al carro continuar a velocidad crucero sin detenerse falsamente.
- Solo los objetos que invaden el interior del trapecio activan la secuencia de freno preventivo (`NAV_DETENIDO_OBSTACULO`).

---

## 5. Pruebas unitarias asociadas

- `nucleo_c/pruebas_corredor.c`: Evalúa la proyección del trapecio ante distintas alturas e inclinaciones de cámara y comprueba la pertenencia de puntos de prueba en los límites interior, exterior y bordes.

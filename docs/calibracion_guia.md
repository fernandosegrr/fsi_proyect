# Guía de calibración física del carro FSI

**Materia:** Fundamentos de los Sistemas de Información (FSI)  
**Propósito:** Protocolo práctico de calibración cinemática y metrológica para garantizar precisión en avances y maniobras de giro.  
**Fecha:** 27 de octubre de 2026  

---

## 1. Por qué es indispensable calibrar

El carro no dispone de encoders en las ruedas ni de posicionamiento por satélite (GPS). Todo movimiento en modo autónomo se planifica en función del **tiempo de activación de los motores** y la **retroalimentación angular del giroscopio**.

Debido a que ningún motor eléctrico es exactamente idéntico a otro y a que las reductoras mecánicas de plástico presentan holguras y fricciones variables, un comando de avance con la misma potencia en ambos lados hace que el carro se desvíe hacia el lado con mayor rozamiento. La calibración cuantifica y compensa matemáticamente estas asimetrías.

---

## 2. Calibración del balance de tracción (Línea recta)

### 2.1 Fundamento físico
El chasis cuenta con cuatro motores agrupados en dos canales: par izquierdo y par derecho. El canal derecho se toma como referencia fija (al 100 % de la potencia del perfil activo, sea 255 o 210), mientras que el canal izquierdo se atenúa multiplicándolo por un factor de balance en milésimas (`factor / 1000`).

$$\text{Potencia Izquierda} = \frac{\text{Tope Derecho} \times \text{Factor}}{1000}$$

### 2.2 Procedimiento de medición con cinta métrica
1. Extender una cinta métrica de al menos 2 metros en un piso plano y limpio.
2. Alinear el centro del vehículo sobre el origen (0 cm), orientado exactamente paralelo a la cinta.
3. En la app Android, ingresar a la pantalla `CalibracionActivity`.
4. Ejecutar una corrida de prueba de avance de 150 cm en modo MANUAL.
5. Medir con escuadra o flexómetro la desviación lateral en el punto de llegada:
   - **Si el carro se desvía a la DERECHA:** El lado izquierdo está empujando con mayor fuerza que el derecho. Se debe reducir el factor de balance (por ejemplo, de 1000 a 950).
   - **Si el carro se desvía a la IZQUIERDA:** El lado izquierdo empuja menos. Se debe incrementar el factor hacia 1000.
6. Repetir la prueba hasta que la desviación final sea inferior a 5 cm.

### 2.3 Valores de balance registrados
- **Perfil MANUAL (255):** Validado físicamente en la jornada J11. Con balance 1000 (255/255) el carro se desviaba 14.5 cm a la derecha a 150 cm; con balance 988 (252 a la izquierda y 255 a la derecha) la desviación se redujo a **3.0 cm** (**VERIFICADO**).
- **Perfil AUTO (210):** Se calculó un factor teórico de 986 (207 a la izquierda y 210 a la derecha) manteniendo la proporción de MANUAL (**SIMULADO / NO MEDIDO en pista física**).

---

## 3. Calibración de avance por tiempo (`ms/cm` y frenado)

El desplazamiento de un tramo recto consta de tres fases físicas:
1. **Arranque a plena potencia (`ARRANQUE_PLENO_MS` = 250 ms):** Ambos motores reciben 255 para vencer la inercia estática sin que los motores zumben parados.
2. **Avance a velocidad constante:** La distancia recorrida es proporcional al tiempo transcurrido según la pendiente de calibración ($t = d \times \text{ms/cm}$).
3. **Inercia y deslizamiento (`frenado_mm`):** Al apagarse los motores, el peso del chasis provoca un arrastre por inercia de varias decenas de milímetros antes del reposo total.

```
 Distancia
    ▲
    │                      /──────────────── (reposo: distancia + frenado_mm)
    │                     /  velocidad constante (ms/cm)
    │                    /
    │      ─────────────/
    │      Arranque pleno (250 ms)
    └────────────────────────────────────────► Tiempo (ms)
```

### 3.1 Asistente guiado de la app
El módulo `nucleo_c/calibracion.c` implementa el ajuste lineal automatizado mediante dos corridas:
- **Paso 1:** El operador ordena una corrida de corta duración (ej. 1000 ms) y mide la distancia física recorrida con cinta métrica ($d_1$).
- **Paso 2:** El operador ordena una corrida de mayor duración (ej. 2500 ms) y mide la distancia ($d_2$).
- **Cálculo:** La app calcula automáticamente la pendiente en milisegundos por centímetro y el desplazamiento por inercia residual (`frenado_mm`), guardando los valores en la memoria de la aplicación.

---

## 4. Calibración de giros y pivotes angulares

El carro ofrece dos estrategias para girar:

### 4.1 Giro por ángulo cerrado con giroscopio MPU6050 (`GIRAR:<grados>`)
Es la estrategia principal recomendada.
1. El carro debe encontrarse totalmente detenido sobre el piso durante al menos 300 ms.
2. El firmware ejecuta el comando de calibración de sesgo (`IMU_CAL`), promediando 250 lecturas a 100 Hz para fijar la velocidad angular en cero.
3. Al enviar `GIRAR:90`, el ESP32 enciende el motor opuesto y muestrea continuamente la tasa de giro en el eje Z.
4. Cuando el ángulo integrado alcanza el valor objetivo menos la anticipación aprendida, corta la potencia del motor.
5. El firmware confirma la finalización reportando `GIRO_OK`.

### 4.2 Calibración en tiempo por grado (`ms/grado`)
Utilizada como respaldo por el núcleo en C cuando se realizan simulaciones puras o si el giroscopio sufriera una falla de hardware:
- Se utiliza la pantalla `CalibrarGiroActivity`.
- Se mide el tiempo necesario para completar 90° físicos hacia la izquierda y hacia la derecha de manera independiente, calculando la constante de milisegundos por grado para cada lado.

---

## 5. Referencias visuales

- [`docs/figuras/fig_12_calibracion.png`](file:///e:/Dev/fsi_proyecto/docs/figuras/fig_12_calibracion.png): Gráficas de perfil de velocidad, tiempo de arranque y recta de calibración distancia-tiempo.

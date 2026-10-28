# Recorrido de código: Simulador cinemático y de rutas (`simulador_navegador.c` y `sim_ruta.c`)

**Ubicación:** `nucleo_c/simulador_navegador.h`, `nucleo_c/simulador_navegador.c`, `nucleo_c/sim_ruta.h`, `nucleo_c/sim_ruta.c`  
**Lenguaje:** C estándar (C99)  
**Propósito:** Modelar la física virtual del carro, el entorno de obstáculos y la inyección de perturbaciones aleatorias para evaluar de manera reproducible miles de escenarios de navegación en PC.

---

## 1. Justificación del entorno de simulación

Probar físicamente un carro autónomo en pista conlleva limitaciones prácticas:
- Las baterías se agotan rápidamente (máximo 20 corridas por juego de pilas).
- La colocación manual de la caja de obstáculo y la alineación inicial varían entre intentos por error humano.
- Medir desviaciones milimétricas al final de cientos de trayectorias requiere horas de trabajo manual.

El simulador cinemático en C reproduce el comportamiento del chasis y de los sensores con fidelidad matemática, ejecutando **400 simulaciones completas en menos de 2 segundos** en cualquier computadora.

---

## 2. Modelo cinemático virtual (`simulador_navegador.c`)

El simulador implementa un modelo de vehículo diferencial con perturbaciones físicas realistas:

1. **Estado dinámico continuo:**  
   Mantiene la pose real del vehículo virtual $(X_{\text{mundo}}, Y_{\text{mundo}}, \theta_{\text{mundo}})$ integrada con pasos de tiempo discretos ($\Delta t = 5\text{ ms}$).
2. **Inercia y arranque:**  
   Reproduce la aceleración no lineal inicial y el arrastre inercial al enviar `STOP`.
3. **Asimetría de tracción (Desbalance):**  
   Permite simular carros con desviaciones mecánicas (por ejemplo, que el motor izquierdo entregue un 10 % más de empuje que el derecho).
4. **Ruido estocástico parametrizado:**  
   Inyecta ruido blanco gaussiano en la velocidad angular del giroscopio virtual y deriva acumulada en los pivotes, permitiendo verificar si el navegador es capaz de completar rutas bajo condiciones adversas de tracción.

---

## 3. Módulo `sim_ruta.c`: Ejecución de escenarios Montecarlo

`sim_ruta.c` actúa como el árbitro de pruebas que orquesta la interacción entre el mundo virtual y la máquina de estados de `navegador.c`:

- **Definición del mundo:** Configura el punto de partida, los waypoints de la ruta y la posición métrica de las cajas de obstáculo (ej. caja de 20×20 cm sobre la trayectoria a 80 cm del inicio).
- **Emulación de sensores:**
  - Calcula la distancia real desde el frente del carro virtual hacia la caja.
  - Si la caja entra en el campo visual del corredor trapezoidal, genera una estructura `ResultadoObstaculos` sintética y se la entrega a `navegador_tick()`.
  - Genera mensajes de telemetría angular idénticos a los del firmware real (`SENS`).
- **Recolección estadística de resultados:**
  Al concluir cada corrida virtual, registra:
  - Si el vehículo logró alcanzar la celda de destino final sin colisionar (`toques == 0`).
  - La desviación lateral final respecto a la línea ideal (error lateral en milímetros).
  - El tiempo total empleado y la cantidad de maniobras de esquive ejecutadas.
  - Al completar un lote de $N = 400$ corridas, calcula la media, mediana (p50), percentil 95 (p95) y desviación máxima.

---

## 4. Evidencia generada en la jornada J15

Este simulador es el responsable de producir las estadísticas citadas en el reporte J15:
- **Escenario B6 con versión previa a J15:** Error lateral medio de 132 mm (13.2 cm) y p95 de 219 mm (21.9 cm).
- **Escenario B6 con J15 (Pose corregida con rumbo de giroscopio):** Error lateral medio de 19 mm (1.9 cm) y p95 de 46 mm (4.6 cm), con 400 de 400 rutas llegando a destino sin toques.
- **Rigor metodológico:** Las semillas del generador pseudoaleatorio son deterministas, garantizando que cualquier desarrollador pueda reproducir exactamente las mismas cifras ejecutando `make test` en `nucleo_c/`.

---

## 5. Pruebas unitarias asociadas

- `nucleo_c/pruebas_sim_ruta.c`: Ejecuta baterías de pruebas de ruta recta, en L y en circuito cerrado con y sin ruido.
- `nucleo_c/pruebas_reincorporacion_j15.c`: Evalúa formalmente la mejora estadística del escenario B6.

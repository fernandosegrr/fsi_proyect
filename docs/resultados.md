# Compendio de resultados y estado de validación

**Materia:** Fundamentos de los Sistemas de Información (FSI)  
**Proyecto:** Carro con navegación asistida  
**Fecha de consolidación:** 27 de octubre de 2026  
**Criterio editorial:** Separación estricta entre hechos medidos en hardware, resultados de simulación en PC y aspectos no medidos.

---

## 1. Resultados verificados físicamente en hardware

Las siguientes métricas fueron obtenidas mediante mediciones directas sobre el chasis físico, los microcontroladores y el teléfono de pruebas:

| Subsistema / Parámetro | Condición de prueba | Resultado medido | Evidencia documental |
|---|---|---|---|
| **Balance en línea recta (Perfil MANUAL)** | Avance de 150 cm sobre piso plano a potencia 255. | Desviación lateral reducida de **14.5 cm** (con 255/255) a **3.0 cm** aplicando balance 988 (252 izquierda / 255 derecha). | `firmware_carro/config.h:103-104`; `docs/reporte_J13_fisico.md`. |
| **Rendimiento de cámara GC0308 (HQVGA)** | Barrido de 100 cuadros, 20 MHz, RGB565, calidad JPEG 50. | **12.4 fps** promedio de salida; tiempo de compresión por software de **56 ms**; edad media del cuadro de **137 ms**. | `docs/camara_prueba.md` §«Lo medido»; `docs/referencia/camara.md:109`. |
| **Rendimiento de cámara GC0308 (QVGA)** | Barrido de 100 cuadros, 20 MHz, RGB565, calidad JPEG 50. | **8.8 fps** promedio; tiempo de compresión de **107 ms**; edad media del cuadro de **252 ms**. | `docs/camara_prueba.md` §«Lo medido». |
| **Tamaño del APK oficial** | Compilación release con bibliotecas nativas comprimidas. | **14,852,386 bytes** (cumple el límite de 15,000,000 bytes). SHA-256: `36CE38F2...`. | `dist/carro_fsi_0.4-074c9fd.apk`; `scripts/verificar_version.py`. |
| **Tamaño de la imagen de firmware** | Compilación Arduino-ESP32 3.3.12 con optimizaciones de espacio. | **1,031,712 bytes**. SHA-256: `9FD1BB51...`. | `dist/firmware_carro_453ed20-20261027.bin`. |
| **Tiempo de bucle del firmware (`LOOP?`)** | Ejecución continua con muestreo I2C a 400 kHz y WebSocket. | Ciclo de ejecución inferior a **5.0 ms** en régimen normal; I2C con timeout estricto de 5 ms. | `firmware_carro/config.h:163`; `firmware_carro.ino:535`. |
| **Pruebas de pista superadas (J14)** | Pruebas de integración en pasillo con cinta métrica. | Escenarios B0, B1, B3, B4, B5 y B6 ejecutados en hardware en la jornada J14. | `docs/reporte_J14.md`. |

---

## 2. Resultados obtenidos mediante simulación en PC

Las siguientes cifras corresponden a modelos matemáticos y simulaciones Montecarlo ejecutadas en computadora; **no representan mediciones del carro físico**:

| Escenario simulado | Modelo computacional | Resultado antes de J15 | Resultado con J15 | Interpretación técnica |
|---|---|---|---|---|
| **Escenario B6 (400 rutas con obstáculo frontal)** | Simulador cinemático en C (`nucleo_c/sim_ruta.c`) con ruido angular parametrizado. | Error lateral medio: **13.2 cm**.<br>Percentil 95 (p95): **21.9 cm**.<br>Llegadas exitosas: 344/400. | Error lateral medio: **1.9 cm**.<br>Percentil 95 (p95): **4.6 cm**.<br>Llegadas exitosas: **400/400**. | Integrar el rumbo real del giroscopio durante el avance posterior al esquive eliminó la acumulación de error lateral simulado. |
| **Escenario Recta sin obstáculo (400 rutas)** | Modelo cinemático virtual. | Error medio: 16.2 cm.<br>p95: 27.0 cm. | Error medio: **7.4 cm**.<br>p95: **12.4 cm**. | No degrada la trayectoria básica. |
| **Escenario Trayectoria en L (400 rutas)** | Modelo cinemático virtual con pivote a 90°. | Error medio: 8.8 cm.<br>p95: 15.5 cm. | Error medio: **2.6 cm**.<br>p95: **6.0 cm**. | Mejora la alineación posterior al pivote. |
| **Suite de pruebas unitarias del núcleo** | 21 ejecutables C99 independientes. | — | **1598 casos de prueba, 0 fallos**. | Verificación exhaustiva de algoritmos de trayectoria, celdas, corredor y navegador. |
| **Suite de pruebas de firmware** | Módulos C compilados con hardware falso. | — | **447 casos de prueba, 0 fallos**. | Verificación de watchdog a 1000/1001 ms, arranque escalonado y recuperación de cámara. |
| **Prueba de estrés de cola de comandos** | Inyección masiva de 100 000 comandos hacia `carro.c`. | — | **100 000 comandos procesados sin perder un solo `STOP`**. | Robustez comprobada de la política de descarte de la cola. |

---

## 3. Estado de validaciones físicas no realizadas (NO MEDIDO)

Con el fin de mantener la honestidad técnica más rigurosa, se señalan explícitamente aquellos puntos que **NO HAN SIDO MEDIDOS FÍSICAMENTE** en el vehículo:

1. **Validación física de la corrección de pose J15:**  
   La modificación introducida en J15 para integrar el rumbo del giroscopio durante los avances cuenta **únicamente con respaldo en simulación computacional**. En el carro físico se realizaron **cero corridas con esta versión**, y el criterio formal de aceptación de 5 de 6 llegadas consecutivas permanece como **NO MEDIDO**. El dueño del proyecto dejó esta validación como pendiente para continuarse con otro modelo de pruebas (`docs/reporte_J15.md`).
2. **Balance del perfil AUTO en pista real:**  
   El factor de balance de motores para el perfil AUTO (207 a la izquierda y 210 a la derecha) fue escalado matemáticamente a partir del perfil MANUAL. No ha sido medido con cinta métrica sobre el carro físico (**NO MEDIDO**).
3. **Latencia de video de extremo a extremo:**  
   El retraso total desde que la luz incide en el sensor GC0308 de la cámara hasta que los píxeles se iluminan en la pantalla OLED del teléfono Android solo cuenta con una medición aislada informal de ~530 ms. No existe una caracterización metrológica formal (**NO MEDIDO**).
4. **Pruebas físicas B2, B7, B8 y B9:**  
   Fueron suspendidas durante el cierre de J14 por decisión del dueño del proyecto; no se presentan como superadas.

---

## 4. Referencias visuales

- [`docs/figuras/fig_05_pose_j15.png`](file:///e:/Dev/fsi_proyecto/docs/figuras/fig_05_pose_j15.png): Comparación gráfica de pose proyectada antes y después de J15.
- [`docs/figuras/fig_06_esquive.png`](file:///e:/Dev/fsi_proyecto/docs/figuras/fig_06_esquive.png): Geometría de maniobra de esquive y reincorporación a la línea.

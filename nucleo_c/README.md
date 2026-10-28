# Núcleo de navegación en C (`nucleo_c`)

Este directorio contiene la biblioteca de algoritmos centrales de navegación, cinemática, calibración y percepción del carro, implementada íntegramente en **C estándar (C99)**.

---

## 1. Filosofía de diseño

- **Independiente de plataforma:** No depende de librerías del sistema operativo, marcos gráficos ni APIs de red.
- **Doble propósito:** Se compila directamente en la app Android mediante el NDK y JNI (`app/src/main/cpp/puente.c`) y en PC para ejecución de pruebas unitarias y simulaciones montecarlo sin necesidad de hardware.
- **Determinismo y seguridad:** Sin asignación dinámica descontrolada en tiempo de ejecución; estados acotados y predecibles.

---

## 2. Módulos principales

| Módulo | Archivos | Responsabilidad |
|---|---|---|
| **Navegador autónomo** | `navegador.c` / `navegador.h` | Máquina de estados finitos (`ESPERANDO`, `SIGUIENDO`, `DETENIDO_OBSTACULO`, `ESQUIVANDO`, `REINCORPORANDO`, `TERMINADO`, `ERROR`, `AYUDA`). En J15 integra la corrección de pose usando lecturas de rumbo del giroscopio. |
| **Calibración cinemática** | `calibracion.c` / `calibracion.h` | Factores de conversión física: milisegundos por centímetro de avance, compensación de inercia y frenado (`frenado_mm`), y milisegundos por grado en pivotes. |
| **Generación de trayectoria** | `trayectoria.c` / `trayectoria.h` | Convierte secuencias de celdas en cuadrícula (resolución de 20 cm) en listas de comandos temporizados (`FORWARD`, `LEFT`, `RIGHT`, `STOP`). |
| **Detección de obstáculos** | `obstaculos.c` / `obstaculos.h` | Análisis de matriz de piso y umbrales cromáticos para identificar bloqueos frontales o laterales. |
| **Corredor de seguridad** | `corredor.c` / `corredor.h` | Proyección de la franja trapezoidal de paso del vehículo considerando altura, inclinación y ángulo visual de la cámara. |
| **Fusión sensorial** | `fusion.c` / `fusion.h` | Combina la segmentación cromática del piso con detecciones de cajas/obstáculos generadas por redes neuronales. |
| **Protocolo de celdas** | `paquete_celdas.c` / `paquete_celdas.h` | Deserialización y procesamiento de paquetes UDP `CEL1` transmitidos por la cámara. |
| **Simulador de navegación** | `simulador_navegador.c` / `sim_ruta.c` | Modelo cinemático virtual con ruido aleatorio parametrizado para validar miles de escenarios de esquive y reincorporación en PC. |

---

## 3. Pruebas unitarias

El directorio cuenta con **21 programas de prueba** que evalúan exhaustivamente cada módulo:

```powershell
# Ejecución de la suite completa de pruebas del núcleo
cd nucleo_c
make test
```

Al concluir, el conjunto evalúa **1598 casos de prueba con 0 fallos**.

Para un recorrido detallado de las funciones y estructuras clave de cada módulo, véase [`docs/referencia/nucleo.md`](file:///e:/Dev/fsi_proyecto/docs/referencia/nucleo.md) y [`docs/codigo/`](file:///e:/Dev/fsi_proyecto/docs/codigo/).

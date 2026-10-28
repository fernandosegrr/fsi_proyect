# Figuras del manual

**Generado por** `docs/figuras/generar_figuras.py` a partir de `docs/figuras/catalogo.py`.
No se editan a mano: se cambia el dibujo y se vuelve a ejecutar el script.

Cada figura existe en PNG (170 dpi, para el PDF) y SVG (vectorial, para ampliar o editar).
Los pies y las notas viven en `catalogo.py`, para que el texto y la imagen no se separen.

| N.º | Título | Archivo | Capítulo | Qué muestra |
|---|---|---|---|---|
| 01 | El sistema completo | `fig_01_sistema.png` / `fig_01_sistema.svg` | Arquitectura del sistema | Teléfono, ESP32, cámara, sensores, driver y dos circuitos de alimentación. |
| 02 | Capas del firmware | `fig_02_capas_firmware.png` / `fig_02_capas_firmware.svg` | Firmware del carro | La lógica en C se separa del hardware por las funciones hw_..., y por eso se puede probar en el PC. |
| 03 | Qué pasa cuando llega un comando | `fig_03_flujo_comando.png` / `fig_03_flujo_comando.svg` | Firmware del carro | Recorrido de un FORWARD desde la app hasta los motores, y el PING que mantiene viva la conexión. |
| 04 | Estados del navegador | `fig_04_estados_navegador.png` / `fig_04_estados_navegador.svg` | Núcleo de navegación en C | El navegador siempre está en un estado conocido y solo cambia con reglas explícitas. |
| 05 | Cómo se estima la pose | `fig_05_pose_j15.png` / `fig_05_pose_j15.svg` | Núcleo de navegación en C | Antes todo el avance se proyectaba con un rumbo fijo; J15 integra cada tramo con el rumbo del giroscopio. |
| 06 | Esquivar y volver a la línea | `fig_06_esquive.png` / `fig_06_esquive.svg` | Núcleo de navegación en C | El carro se aparta 40 cm, pasa la caja y busca un punto 30 cm por delante para reincorporarse. |
| 07 | El corredor de trayectoria | `fig_07_corredor.png` / `fig_07_corredor.svg` | Núcleo de navegación en C | La franja de la imagen por la que el carro va a pasar se estrecha con la distancia; un objeto fuera de ella solo avisa. |
| 08 | Mapa de pantallas de la app | `fig_08_pantallas_app.png` / `fig_08_pantallas_app.svg` | Aplicación Android | Cuatro pestañas principales y cinco pantallas completas que comparten la conexión y el video. |
| 09 | Los tres modos de la cámara | `fig_09_camara_modos.png` / `fig_09_camara_modos.svg` | Cámara ESP32 | JPEG para ver, RAW para procesar sin pérdida y CELDAS para detectar obstáculos con muy pocos bytes. |
| 10 | Los dos circuitos de alimentación | `fig_10_alimentacion.png` / `fig_10_alimentacion.svg` | Hardware y cableado | La electrónica va por USB y los motores por cuatro pilas AA; las tierras van unidas y el positivo de las pilas no toca la ESP32. |
| 11 | Mensajes entre la app y el carro | `fig_11_protocolo.png` / `fig_11_protocolo.svg` | Protocolo de comunicación | Órdenes, ACK, PING/PONG, SENS y el watchdog que frena si no llega un comando real. |
| 12 | Navegar por tiempo | `fig_12_calibracion.png` / `fig_12_calibracion.svg` | Calibración | El carro convierte distancia y ángulo en tiempo con la calibración; el arranque y el frenado se miden aparte. |

## Cómo regenerar

```bash
python docs/figuras/generar_figuras.py
```

Requiere Python 3.9 o más nuevo y `matplotlib` 3.9.4 (el entorno usado en este repositorio).
El script no necesita conexión a internet ni el carro.

## Reglas

- Ninguna figura inventa una medición. Si un valor es simulado o no medido, se dice en la figura o en el pie.
- Las figuras son esquemas: no son planos de montaje ni sustituyen una foto.
- Las fotos reales van en `docs/imagenes/` y el manual las incluye aparte.

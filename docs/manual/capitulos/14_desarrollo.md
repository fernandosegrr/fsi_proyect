# 14. Proceso de desarrollo

Este capítulo resume la evolución técnica del proyecto desde el prototipo inicial de control por WiFi hasta la arquitectura distribuida con verificación automatizada y cierre de precisión de la **Versión final 1.1**. Más que enumerar fechas, explica qué problema concreto apareció en cada etapa y por qué se tomaron las decisiones de ingeniería que dieron forma al sistema actual.

## 14.1 Del control monolítico al desacoplamiento por interfaces en C99

En el punto de partida del proyecto (`original_backup`), un solo archivo de Arduino concentraba la configuración de la red `WiFi`, el servidor web, la recepción de mensajes y la escritura directa en los pines de los motores, mientras que la aplicación Android únicamente mostraba botones básicos. Ese enfoque inicial tenía dos limitaciones severas:

1. **Imposibilidad de probar la lógica sin el carro físico:** cualquier cambio en el manejo de comandos exigía grabar la placa `ESP32`, encender las baterías y observar manualmente si las ruedas giraban, sin forma de reproducir fallos de temporización o condiciones de frontera.
2. **Vulnerabilidad ante pérdida de conexión o saturación de mensajes:** si el teléfono se desconectaba mientras enviaba una orden de avance, o si los mensajes llegaban desde el hilo asíncrono de red al mismo tiempo que el bucle principal tocaba los pines, el vehículo podía quedar con los motores encendidos.

La primera decisión estructural consistió en extraer toda la lógica del firmware a módulos en estándar **`C99` puro** (`carro.c`) separados del hardware mediante funciones abstractas (`hw_*`), introducir la **cola determinista de 32 ranuras con prioridad absoluta para `STOP` y `EBRAKE`**, fijar el *watchdog* de comunicación en **`1,000 ms`** y crear la suite `pruebas_firmware` junto con el servidor falso de protocolo.

## 14.2 Creación del núcleo matemático compartido (`nucleo_c`) y puente JNI

Para dotar al vehículo de navegación por rutas sobre una cuadrícula de `20 cm` sin duplicar código entre la computadora de desarrollo y el teléfono Android, se diseñó la biblioteca **`nucleo_c`** en `C99` sin dependencias externas:

- Se implementaron el planificador geométrico (`trayectoria.c`), el editor de rutas (`ruta_editor.c`), el modelo lineal de conversión distancia-tiempo con compensación de arranque y frenado (`calibracion.c`) y la máquina de estados del navegador (`navegador.c`), enlazándolos con Kotlin mediante `puente.c` (`JNI`).
- Al mantener el núcleo en `C99`, se pudo construir un **simulador de vehículo y entorno (`simulador_navegador.c`)** capaz de ejecutar miles de recorridos completos en milisegundos dentro de `make -C nucleo_c test`.

[FIGURA:desarrollo|Evolución técnica del sistema en cinco etapas: desde el control WiFi inicial hasta el cierre de precisión J16 y la validación física del esquive B6.]

## 14.3 Integración de cámara independiente y percepción multimodal

La incorporación de visión artificial planteó un dilema de rendimiento: el sensor `GC0308` de la placa `Freenove ESP32-WROVER` carece de compresión `JPEG` por hardware y requiere codificación por software. Un barrido sistemático de resoluciones y frecuencias de reloj demostró que **`HQVGA` (`240×176` píxeles) a `20 MHz`** reducía la edad media de cuadro a **`137 ms`** (`12.4 fps`), casi la mitad del rezago de `QVGA` (`252 ms`), y el diseño de **buzón de último cuadro** más el modo `UDP` de celdas (`1,244` bytes) eliminaron la acumulación de imágenes viejas en cola.

En el lado de percepción, las pruebas con pisos reales mostraron que ni el color del suelo ni una red neuronal por separado bastaban para todos los objetos. Se construyó entonces la tubería multimodal: clasificador `YCbCr` de hasta tres clústeres con la regla de tolerancia en filas lejanas (`obstaculos.c`), máscara de artefactos (`mascara_vision.c`), red `EfficientDet-Lite0` en hilo dedicado con vigencia de `300 ms` (`ModeloEnHilo.kt`), proyección trapezoidal del ancho del carro (`corredor.c`) y módulo de fusión (`fusion.c`).

## 14.4 Sensores embarcados (`MPU6050` y `VL53L0X`) e integración de pose

Las primeras pruebas físicas en pista revelaron dos fenómenos mecánicos propios del chasis de cuatro motores `TT`:

1. Con ambos lados al máximo (`255/255`), la asimetría de los motores desviaba el carro `14.5 cm` en una recta de `150 cm` (corregido inicialmente a `3.0 cm` en modo `MANUAL` ajustando `252/255`), y los giros calculados solo por tiempo variaban con la carga de las pilas `AA` y la textura del piso.
2. En la maniobra de esquive, el navegador proyectaba los avances suponiendo que el carro mantenía siempre el rumbo ideal del tramo ($0^\circ, \pm 90^\circ$), por lo que cualquier error angular durante el rodeo desplazaba el punto de reincorporación.

Para resolver el primer punto se integraron en el bus `I2C` (`GPIO 21/22`) el giroscopio **`MPU6050`** (muestreado a `200 Hz` con lazo cerrado de giro `giro.c` a `±2.0°`) y el sensor láser **`VL53L0X`** (con paro reflejo local a `120 mm` en `tof.c`). Para resolver el segundo punto, se rediseñó el estimador de pose de `navegador.c` para que integrara cada incremento de avance usando el ángulo real reportado por el giroscopio en `SENS` y calculara trigonométricamente el vector de regreso hacia la línea original.

## 14.5 Cierre de precisión J16 y validación física del esquive en el carro

Finalmente, el análisis de precisión y concurrencia condujo al cierre **J16** que culmina esta **Versión final 1.1**:

- Se eliminó la pérdida de precisión por truncamiento integrando la pose en **décimas de grado (`rumbo_x10`)** y corrigiendo `override_largo_mm` y `descartar_dt`.
- Se implementó el **controlador `PI` de rumbo en rectas (`rumbo.c`)** con aprendizaje de sesgo entre tramos (`aprendido_mil`), guiado por punto de mira a `400 mm` y supresión de giros detenidos pequeños ($\le 8^\circ$).
- Se agregó el **factor de escala del giroscopio en partes por millón (`IMU_ESCALA` en `NVS`)**, la normalización de frenado en giros largos (`GIRO_LARGO_POR_GRADO`), el candado mutex `candado_vision` en `JNI`, el aviso `WD:<n>` y la vigilancia triple en `ModoSemiautonomo.kt`.
- Tras verificar el conjunto con **50,000 corridas Monte Carlo** y **2,267 casos de prueba automatizados en PC**, se validó físicamente en el carro el **esquive con reincorporación B6** (6 corridas el 10 de noviembre de 2026: `0/6` toques de caja y `5/6` corridas autónomas completas con error lateral aproximado entre `~0` y `~10 cm`).

Dónde está en el código: `bitacora_desarrollo.md`, `decisiones_diseno.md`, `reporte_J16.md`, `carro.c`, `rumbo.c`, `navegador.c`, `puente.c`.

# Guion de exposición y banco de preguntas frecuentes

**Materia:** Fundamentos de los Sistemas de Información (FSI)  
**Profesor evaluador:** Dr. Horacio Rostro González  
**Fecha de presentación:** 1 de diciembre de 2026  
**Proyecto:** Carro con navegación asistida por visión y sensores  

---

## 1. Estructura sugerida para la presentación oral (15 minutos)

```
 [0:00 - 2:00]   Introducción: El reto de la navegación autónoma en hardware accesible.
 [2:00 - 5:00]   Arquitectura en tres nodos: ESP32 DevKit, Freenove Cámara y App Android.
 [5:00 - 8:00]   Hardware y seguridad: Aislamiento eléctrico, giroscopio MPU6050 y láser ToF.
 [8:00 - 11:00]  Algoritmos centrales: Máquina de estados, corredor visual y corrección de pose.
 [11:00 - 13:00] Demostración en vivo: Enlace, video en tiempo real y ejecución de ruta.
 [13:00 - 15:00] Resultados y conclusiones: Separación transparente de lo medido vs simulado.
```

---

## 2. Guion paso a paso de la presentación

### Minuto 0:00 a 2:00 — Introducción y planteamiento del problema
- **Apertura:** Saludo formal al profesor y sínodo. Presentación del equipo y del objetivo del proyecto: *«Diseñar e implementar un vehículo terrestre autónomo capaz de ejecutar trayectorias planificadas y sortear obstáculos dinámicos empleando hardware embebido de bajo costo»*.
- **El reto técnico:** Navegar con precisión en plataformas accesibles sin recurrir a sensores LiDAR industriales costosos ni a encoders ópticos de alta resolución.
- **Tesis de ingeniería:** La combinación de una arquitectura desacoplada, algoritmos cinemáticos en C99 puro y fusión sensorial ligera permite navegación reactiva confiable sobre chasis educativos.

### Minuto 2:00 a 5:00 — Arquitectura distribuida del sistema
- **Los tres nodos:**
  1. *ESP32 DevKit:* Controlador de tiempo real, tracción PWM, punto de acceso WiFi y muestreo de sensores I2C a 100 Hz.
  2. *Freenove ESP32-WROVER:* Nodo de visión independiente con cámara GC0308 y 4 MB de PSRAM para compresión JPEG.
  3. *Teléfono Android:* Interfaz de usuario, planificación en cuadrícula de 20 cm e inferencia de modelos neuronales.
- **Por qué tres nodos:** Separar la visión de la tracción evita que la compresión de cuadros degrade los tiempos del PWM de los motores o el muestreo del giroscopio.

### Minuto 5:00 a 8:00 — Hardware, alimentación y seguridad
- **El problema del ruido eléctrico:** Los motores TT generan picos de corriente y ruido inductivo capaces de reiniciar microcontroladores (*brownout*).
- **Solución implementada:** Dos fuentes físicamente independientes:
  - Powerbank USB de 5 V para la electrónica sensible (ESP32 y cámara).
  - 4 pilas AA (~6 V) exclusivas para el driver de motores TC1508A.
  - Tierra común (GND) para mantener la referencia lógica de las señales PWM.
- **Sensores a bordo:** MPU6050 (eje Z para giros de precisión) y VL53L0X (láser ToF para parada de emergencia autónoma).

### Minuto 8:00 a 11:00 — Algoritmos y calidad de software
- **Núcleo C99 (`nucleo_c`):** Código portátil sin librerías externas que corre tanto en Android (vía JNI) como en PC para pruebas unitarias.
- **Máquina de estados finitos:** 8 estados predecibles (`ESPERANDO`, `SIGUIENDO`, `DETENIDO`, `ESQUIVANDO`, `REINCORPORANDO`, etc.).
- **Percepción:** Corredor trapezoidal de paso proyectado en la cámara; los objetos fuera del corredor no detienen la marcha.
- **Rigor de ingeniería:** 2045 casos de prueba en PC con 0 fallos, pruebas de estrés con 100 000 comandos continuos y watchdog de 1000 ms.

### Minuto 11:00 a 13:00 — Demostración práctica
- Encendido de placas, conexión a la red WiFi del carro (`192.168.4.1`).
- Muestra de telemetría viva en la app (ping de ~25 ms, ángulo de giroscopio en 0° y distancia milimétrica del láser).
- Demostración de teleoperación manual fluida y visualización de video MJPEG.
- Trazado de una ruta en la cuadrícula y ejecución asistida.

### Minuto 13:00 a 15:00 — Resultados y honestidad científica
- Presentación clara de los logros verificados en hardware: desvío recto reducido a 3 cm a 150 cm mediante balance 988; streaming de video a 12.4 fps en HQVGA.
- Diagnóstico de la jornada J15: en simulación computacional, la pose corregida redujo el desvío lateral post-esquive de 13.2 cm a 1.9 cm.
- Aclaración honesta de que la validación física de J15 queda catalogada como **NO MEDIDA**.

---

## 3. Banco de preguntas frecuentes (FAQ) y respuestas técnicas

### P1: ¿Por qué no instalaron encoders en las ruedas del carro?
**Respuesta:** Los chasis comerciales con motores TT amarillos tienen engranajes plásticos con holguras mecánicas considerables y las llantas patinan ligeramente sobre pisos pulidos. Los encoders miden el giro del eje del motor, pero no detectan el derrape de la rueda en el suelo, acumulando error de rumbo rápidamente. En su lugar, decidimos instalar un giroscopio digital MPU6050: al ir fijado rígidamente al chasis, mide la velocidad de rotación real del vehículo en el espacio con independencia total del patinaje de las ruedas.

### P2: ¿Por qué utilizar dos placas ESP32 distintas en lugar de conectar la cámara y los motores a una sola?
**Respuesta:** La placa con cámara (Freenove ESP32-WROVER) utiliza un sensor GC0308 que entrega imágenes crudas RGB565 sin comprimir. La compresión a JPEG la realiza el microcontrolador por software, lo cual consume entre 50 y 110 ms de tiempo de procesador por cuadro y hace uso intensivo de la memoria PSRAM. Si corriéramos la compresión JPEG en la misma placa que gestiona los motores y el bus I2C, la función `loop()` sufriría pausas inadmisibles de más de 100 ms, provocando retardos en la respuesta de frenado, desbordamientos de la cola WebSocket y pérdida de precisión en la integración del giroscopio.

### P3: ¿Qué ocurre si la red WiFi se cae o el teléfono se apaga mientras el carro avanza a máxima potencia?
**Respuesta:** El firmware implementa un **watchdog de comunicación estricto de 1000 ms** en `carro.c`. Mientras el usuario mantiene presionado un botón en la app, el teléfono envía el comando `FORWARD` repetidamente cada 250 ms. Si se pierde el enlace WiFi o la app se bloquea, el temporizador del microcontrolador expira y, exactamente en el milisegundo 1001, el firmware apaga el PWM de los cuatro motores y restablece el perfil al modo seguro `AUTO`. El comando `PING` no reinicia el watchdog, asegurando que solo órdenes explícitas mantengan el movimiento.

### P4: ¿Por qué separaron la alimentación en powerbank y pilas AA? ¿No era más fácil usar una sola batería grande con un convertidor reductor (buck)?
**Respuesta:** Los motores de corriente continua generan ruido electromagnético inductivo muy agresivo y caídas de tensión instantáneas de varios voltios al arrancar bajo carga. Si compartieran la misma batería a través de un convertidor reductor, las caídas de tensión bruscas inducirían fácilmente reinicios intempestivos (*brownouts*) en los microcontroladores y corrupciones en el bus I2C. La separación física garantiza que la electrónica reciba corriente continua pura de 5 V regulados, mientras que el portapilas suministra la corriente de fuerza al driver TC1508A, compartiendo únicamente la línea de tierra (GND) como referencia común.

### P5: ¿Por qué la cámara transmite a resolución HQVGA (240×176) en lugar de VGA o QVGA?
**Respuesta:** En la jornada experimental H3 realizamos un barrido metrológico de 100 cuadros por resolución. En QVGA (320×240), el tiempo de compresión por software era de 107 ms y la edad media del cuadro alcanzaba 252 ms, logrando apenas 8.8 fps. Al cambiar a HQVGA (240×176), la cantidad de píxeles se reduce un 45 %, lo que permitió recortar la compresión a 56 ms y la edad del cuadro a 137 ms, alcanzando 12.4 fps fluidos con calidad JPEG 50. Para el detector de obstáculos y la visualización humana en el móvil, 240×176 píxeles brinda suficiente resolución angular sin penalizar la latencia.

### P6: ¿Cómo esquiva obstáculos el carro si no construye un mapa completo de la habitación?
**Respuesta:** El carro utiliza una estrategia reactiva basada en un **corredor de seguridad trapezoidal**. Mediante calibración geométrica de la cámara (altura de montaje e inclinación angular), el núcleo calcula la región del plano de la imagen por la que el vehículo necesariamente transitará. Si la segmentación del piso o la red neuronal detectan un obstáculo dentro de esta franja, el vehículo se detiene, evalúa la distancia disponible a la izquierda y derecha mediante una cuadrícula local, se desvía 40 cm hacia el lado despejado, supera la caja y avanza 30 cm por delante antes de reincorporarse a la trayectoria original.

### P7: ¿Qué problema se detectó en la jornada J15 y cómo se resolvió? ¿Fue probado físicamente?
**Respuesta:** En las simulaciones del escenario B6 (esquivar una caja de 20 cm en una ruta), se observó que el carro llegaba al final con un error lateral promedio de 13.2 cm. El diagnóstico reveló que el estimador de pose proyectaba los avances asumiendo que el vehículo mantenía el rumbo inicial de la ruta, sin considerar que un giro intermedio introduce pequeñas perturbaciones angulares. En J15 se corrigió el algoritmo en `navegador.c` para integrar el rumbo instantáneo medido por el giroscopio durante el avance. En simulación computacional (400 rutas), el error se redujo drásticamente a 1.9 cm. Sin embargo, para ser rigurosos con la verdad científica, **aclaramos que esta corrección no se llegó a probar en el vehículo físico real (permanece como NO MEDIDA)**.

### P8: ¿Por qué escribieron el núcleo de navegación en C puro en vez de hacerlo en Kotlin dentro de Android?
**Respuesta:** Desarrollar los algoritmos en C99 estándar permitió desacoplar totalmente la lógica matemática del sistema operativo móvil. El mismo código fuente compilado en Android vía JNI se compila de manera nativa en computadoras con GCC, lo que nos permitió crear una batería de 21 suites de prueba con 1598 casos unitarios y ejecutar simulaciones Montecarlo de miles de rutas en pocos segundos sin necesidad de encender un emulador de Android ni agotar las baterías del carro.

### P9: Si la cámara y el láser ToF detectan cosas distintas, ¿cuál tiene la autoridad?
**Respuesta:** El **sensor ToF láser VL53L0X tiene la máxima autoridad de parada física**. La cámara puede fallar ante sombras, cambios de luz o retrasos de red, pero el láser emite fotones a la velocidad de la luz y mide distancias reales en milímetros con determinismo temporal. Si el láser detecta un objeto a menos del umbral de seguridad, el microcontrolador ESP32 frena los motores en el acto (`TOF_PARO`), independientemente de lo que la app o la cámara estén procesando.

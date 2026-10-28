# Historia y evolución cronológica del proyecto

**Materia:** Fundamentos de los Sistemas de Información (FSI)  
**Profesor:** Dr. Horacio Rostro González  
**Fecha:** 27 de octubre de 2026  

---

## 1. De «WALL-E» a «el carro»: origen y replanteamiento

El proyecto comenzó originalmente bajo el nombre informal de *WALL-E*, consistiendo en un prototipo elemental de carro robot basado en un sketch monolítico de Arduino para ESP32. Aquel sistema inicial presentaba limitaciones críticas:
- La lógica de movimiento, la gestión del servidor web y el acceso a los pines estaban mezclados en un único archivo.
- No existía retroalimentación sensorial de orientación ni de distancia: los giros y avances se realizaban a ciegas por tiempo estimado.
- Toda la teleoperación dependía de una página web simple propensa a desconexiones.

Con el fin de convertir el prototipo en un proyecto de ingeniería robusto y formal para la materia FSI, se rebautizó el vehículo oficialmente como **«el carro»** (o **carro FSI**), iniciando una profunda reestructuración arquitectónica.

---

## 2. Línea de tiempo por jornadas de trabajo

```
 [J1 - J10]                   [J11]                     [J12 - J13]               [J14]                    [J15]
 Modularización C99           Sensores I2C              Metrología física         Entrega v1.0             Diagnóstico B6
 Desacoplamiento hw_...       MPU6050 + VL53L0X         Balance recto (3 cm)      APK < 15 MB              Pose corregida
 Pruebas en PC sin placa      Giro angular cerrado      Cámara GC0308 y HQVGA     Estrés 100k cmds         400 rutas sim
 App Android y JNI            Perfiles AUTO/MANUAL      Keep-alive powerbank      Pruebas B0 a B6          Física NO MEDIDA
```

### Jornadas 1 a 10: Desacoplamiento arquitectónico y núcleo C
- **Separación de capas:** Se extrajo toda la lógica de control del sketch de Arduino hacia un módulo en C estándar (`carro.c`), interactuando con el microcontrolador únicamente mediante llamadas a funciones de abstracción de hardware (`hw_motor_pwm`, `hw_millis`).
- **Nacimiento del banco de pruebas en PC:** Al no depender de Arduino, se creó `pruebas_firmware/`, permitiendo probar la lógica del carro en computadoras con hardware falso y verificar el watchdog con precisión de un milisegundo.
- **Núcleo de navegación y JNI:** Se desarrolló la biblioteca matemática en C99 (`nucleo_c/`) para calcular trayectorias sobre cuadrícula de 20 cm, importada directamente en la app Android mediante el NDK sin duplicar código.
- **Percepción inicial:** Se implementó el detector de obstáculos por colorimetría del piso (`obstaculos.c`) y la proyección del corredor trapezoidal (`corredor.c`).

### Jornada 11: La revolución sensorial (Giroscopio y Láser)
- **Integración I2C:** Se incorporaron en un bus común de 400 kHz un giroscopio MPU6050 (`0x68`) y un sensor ToF láser VL53L0X (`0x29`).
- **Giro en bucle cerrado (`GIRAR:<grados>`):** El carro abandonó los giros ciegos por tiempo; ahora el giroscopio mide la velocidad angular a 100 Hz y corta el motor en el ángulo exacto.
- **Parada de emergencia por láser:** El sensor ToF adquirió autoridad para frenar el vehículo ante colisiones inminentes (`TOF_PARO`).
- **Dos perfiles de velocidad:** Se crearon los modos `MANUAL` (potencia plena 255) y `AUTO` (velocidad moderada 210 para facilitar frenados reactivos).

### Jornadas 12 y 13: Metrología y afinación de hardware
- **Calibración de balance en línea recta:** Se descubrió que el motor izquierdo empujaba más que el derecho, provocando un desvío de 14.5 cm en 1.5 metros. Se ajustó el factor de balance a 988 (252 izquierda / 255 derecha), logrando reducir la desviación a solo 3.0 cm.
- **Retos de la cámara Freenove:** Se constató que el sensor GC0308 carece de compresión JPEG por hardware. Se implementó compresión por software en PSRAM y se determinó que la resolución **HQVGA (240×176)** era la óptima (~12.4 fps frente a los 8.8 fps lentos de QVGA). Se diseñó además el protocolo UDP de celdas (`CEL1`).
- **Keep-alive de batería:** Se introdujo un pulso en GPIO 2 para evitar que la powerbank apague su salida por baja demanda de corriente.

### Jornada 14: Cierre formal de la versión 1.0 (Commit `eb88151`)
- **Compresión del APK oficial:** Se configuró el empaquetado de Gradle para comprimir bibliotecas compartidas del NDK, reduciendo el APK final a 14.85 MB para respetar el límite de 15 MB.
- **Validación masiva:** Se ejecutaron pruebas de estrés con 100 000 comandos continuos sin registrar pérdidas de comandos de frenado.
- **Pruebas físicas:** Se completaron las pruebas B0 a B6 en pista física. Por decisión del dueño del proyecto, las pruebas B7, B8, B9 y B2 se suspendieron.

### Jornada 15: Corrección de pose y diagnóstico del escenario B6
- **El problema del desvío lateral post-esquive:** En simulaciones del escenario B6 (esquivar una caja y continuar), se observó que el carro sufría un desvío lateral final acumulado de 13.2 cm en promedio (p95 de 21.9 cm).
- **Causa raíz identificada:** El navegador proyectaba los incrementos de avance usando el rumbo original fijado al inicio de la ruta; un giro intermedio introducía errores angulares no compensados al volver a avanzar.
- **Solución implementada:** Se integró en `navegador.c` la lectura del rumbo instantáneo reportado por el giroscopio (`SENS`) para proyectar la pose en tiempo real.
- **Resultado simulado:** En 400 rutas simuladas del escenario B6, el error lateral cayó a 1.9 cm promedio (p95 de 4.6 cm), logrando 400 de 400 llegadas exitosas.
- **Estado de cierre:** No se realizaron pruebas físicas en el vehículo real; la validación física quedó catalogada como **NO MEDIDA**.

---

## 3. Decisiones de diseño y alternativas descartadas

A lo largo del proyecto se evaluaron múltiples enfoques alternativos que fueron formalmente descartados por razones de ingeniería:

1. **Uso de encoders ópticos en las ruedas:**  
   *Descartado.* Los motores TT amarillos de bajo costo sufren holguras mecánicas y saltos de paso en los discos perforados. Instalar encoders agregaba cableado frágil y consumo de interrupciones sin resolver el problema del derrape de las llantas en el piso. Se optó por el giroscopio MPU6050, que mide la rotación real del chasis con independencia del patinaje de las ruedas.
2. **Procesamiento de visión en servidor en la nube / PC externa:**  
   *Descartado.* Introducía dependencias de conexión a internet externa y latencias de transmisión impredecibles (superiores a 300 ms). Se adoptó una arquitectura 100 % local donde la cámara sirve video dentro de la red del vehículo y el teléfono ejecuta las redes neuronales en su propio procesador.
3. **Parada de emergencia basada exclusivamente en visión:**  
   *Descartado.* Las variaciones de iluminación en aulas y la latencia del flujo de video impedían garantizar una distancia de frenado segura ante obstáculos inesperados. Se otorgó autoridad prioritaria al sensor ToF láser VL53L0X, cuya respuesta es determinista en milisegundos.
4. **Mapeo SLAM / Grilla persistente de ocupación:**  
   *Descartado.* Mantener un mapa métrico completo consumía demasiada memoria y requería odometría milimétrica. Para el alcance del curso, una política reactiva basada en un corredor trapezoidal proyectado y maniobras de evasión acotadas (desvío de 40 cm y avance de 30 cm) resultó infinitamente más simple, confiable y verificable.
5. **Rampa suave de aceleración (*soft start*):**  
   *Descartado.* Al intentar arrancar con valores PWM progresivos desde 0 o 100, la fricción estática del peso del carro hacía que los motores zumbasen sin girar. Se implementó en su lugar un «golpe de arranque» de potencia plena (255) durante exactamente 250 ms (`ARRANQUE_PLENO_MS`), conmutando después a la velocidad crucero.

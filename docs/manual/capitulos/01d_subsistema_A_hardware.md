# Subsistema A · Hardware del carro

## A.1 Qué es y para qué sirve

El hardware es lo físico del carro: una placa ESP32 DevKit (el «cerebro», con WiFi), un driver TC1508A que da corriente a cuatro motores TT con ruedas mecanum, un giroscopio MPU6050 que mide cuánto gira el carro, un sensor láser VL53L0X que mide la distancia al frente y tres fuentes de energía separadas. Sirve para mover el carro y darle dos «sentidos» (giro y distancia) que el firmware (subsistema B) usa para girar un ángulo, sostener el rumbo y frenar ante un obstáculo; la cámara es otra placa (subsistema C). Más detalle: capítulo 3 del manual (`docs/manual/capitulos/03_hardware.md`).

## A.2 Cómo funciona, paso a paso

1. **Energía.** Hay tres fuentes que solo comparten la tierra (GND). La powerbank #1 alimenta la ESP32 por USB y, con el regulador de 3.3 V de la placa, al MPU6050 y al VL53L0X. Un portapilas de 4 pilas AA (unos 6 V) alimenta solo el VDD del TC1508A. La powerbank #2 alimenta la cámara (`README.md`, `docs/manual/capitulos/02_arquitectura.md` §2.2).

```
  Powerbank #1 ──USB──▶ ┌──────────────┐ ──3V3──▶ MPU6050 + VL53L0X
                        │ ESP32 DevKit │          (SDA 21, SCL 22)
                        └──────┬───────┘
                               │ PWM: GPIO 27, 26, 25, 33
                               ▼
  4 pilas AA ──(+) a VDD──▶ ┌─────────┐ ──▶ 2 motores TT izquierdos (OUTA, OUTB)
  (~6 V)                    │ TC1508A │ ──▶ 2 motores TT derechos   (OUTC, OUTD)
                            └────┬────┘
  GND común: (-) de las pilas ───┴─── GND del driver ─── GND de la ESP32

  Powerbank #2 ──USB──▶ cámara Freenove (se une al resto solo por WiFi)
```

2. **Órdenes y driver.** El firmware escribe PWM (encender y apagar la señal muy rápido; el porcentaje encendido marca la potencia) en cuatro pines, de 0 a 255, a 1,500 Hz y 8 bits. El TC1508A es un puente H doble: dos canales con entradas A y B. PWM en una entrada y 0 en la otra gira el motor en un sentido; 0 y 0 lo deja suelto («rueda libre», según la hoja de la familia TC1508S; no se verificó en el módulo, `docs/investigacion_autonomo/04_motores_cinematica.md` §1.1). Cada canal alimenta dos motores TT en paralelo (`docs/conexiones.md` §5).
3. **Pines.** «A» y «B» son nombres de patitas, no sentidos de giro: el motor izquierdo avanza por GPIO 26 y el derecho por GPIO 25 (`config.h`).

| GPIO | Constante | Para qué |
|---|---|---|
| 27 | `PIN_MOTOR_IZQ_A` | izquierdo, retrocede |
| 26 | `PIN_MOTOR_IZQ_B` | izquierdo, avanza |
| 25 | `PIN_MOTOR_DER_A` | derecho, avanza |
| 33 | `PIN_MOTOR_DER_B` | derecho, retrocede |
| 21 y 22 | `PIN_I2C_SDA`, `PIN_I2C_SCL` | bus I2C de los sensores |
| 2 | `PIN_KEEPALIVE` | pulso de 50 ms cada 2,000 ms para que la powerbank no apague su salida |

| Orden | GPIO 27 | GPIO 26 | GPIO 25 | GPIO 33 | Resultado |
|---|---|---|---|---|---|
| `FORWARD` | 0 | 255 | 255 | 0 | los dos lados avanzan |
| `REVERSE` | 255 | 0 | 0 | 255 | los dos lados retroceden |
| `LEFT` | 0 | 0 | 255 | 0 | pivote: solo empuja el motor derecho |
| `RIGHT` | 0 | 255 | 0 | 0 | pivote: solo empuja el motor izquierdo |
| `STOP` y `EBRAKE` | 0 | 0 | 0 | 0 | ruedas libres |

En `FORWARD` y `REVERSE` el 255 dura 250 ms; después el lado izquierdo baja a 252 y el derecho queda en 255 (MANUAL), o 207 y 210 (AUTO) (`config.h`, `carro.c`).

4. **Ruedas mecanum.** Una rueda mecanum lleva rodillos a 45°. Con cada rueda controlada por separado, todas iguales dan avance, los lados opuestos en sentido contrario dan giro en el sitio y las diagonales opuestas dan movimiento lateral. Con solo dos canales, las dos ruedas de cada lado reciben la misma señal: el carro es una tracción diferencial de cuatro ruedas (como un tanque), sin desplazamiento lateral (`04_motores_cinematica.md` §1.3). Así lo hace el código, que solo conoce «izquierdo» y «derecho»; por eso el repo dice «tracción diferencial» aunque las ruedas sean mecanum.
5. **Giros.** `LEFT` y `RIGHT` empujan con un solo lado y el otro queda suelto (`girar_izquierda`, `girar_derecha` en `carro.c`). El carro gira alrededor de un punto fuera de su centro: en 90° a la derecha el centro avanza unos 15 cm y se mueve 16.5 cm a la derecha (radio de 15.8 cm); a la izquierda el radio medido fue de 8.7 a 9 cm (`04_motores_cinematica.md` §1.3, `docs/validacion_sensores.md`).
6. **Sensores.** El MPU6050 (GY-521, 0x68) y el VL53L0X (VL53LXX-V2, 0x29) comparten el bus I2C (GPIO 21 y 22, 400 kHz, 3.3 V; los módulos traen sus resistencias pull-up). El giroscopio mide la velocidad de giro del eje Z (±500 °/s, 100 muestras por segundo) y el firmware la suma para obtener el ángulo. El láser mide cuánto tarda un pulso infrarrojo de 940 nm en regresar: modo continuo, 33 ms por medida y un haz de unos 25° (`config.h`, `docs/investigacion_autonomo/01_vl53l0x.md` §1).

## A.3 Cómo lo hicimos

- **Tracción diferencial con ruedas mecanum.** Con dos canales no hay más: tres movimientos independientes necesitan al menos tres señales, y con el cableado por diagonales saldrían avance y movimiento lateral, pero no giro (análisis del repo, inferencia, `04_motores_cinematica.md` §1.3).
- CONFIRMAR CON EL EQUIPO: ¿Por qué se usaron ruedas mecanum si con este driver de dos canales el carro no puede moverse de lado?
- **Pivote con un solo lado.** Se probó girar con lados opuestos y «el carro no se mueve por falta de par»; la causa no se diagnosticó (`docs/calibracion.md` y `docs/bitacora.md`, citados en `04_motores_cinematica.md` §1.3). Costo: giro asimétrico.
- **Sin encoders, con giroscopio.** Los engranes plásticos de los TT tienen holgura y las llantas patinan; un encoder mide el eje del motor, no el patinaje, y el giroscopio mide el giro real del chasis (`docs/exposicion.md`, pregunta 1). La distancia se sigue estimando por tiempo.
- **Tres fuentes de energía.** Los motores dan picos de corriente y ruido inductivo, y la electrónica se reinicia (brownout) si su tensión baja demasiado (`docs/seguridad.md` §1.1); además el TC1508A no tiene alimentación lógica aparte (`docs/conexiones.md` §4). Reglas: el (+) de las pilas va solo a VDD, las tierras van unidas y no se agrega una quinta pila (cuatro AA dan unos 6 V y el driver admite 7.2 V como máximo). Esas corrientes no se midieron (**NO MEDIDO**). La ESP32 va a 160 MHz y el WiFi a 13 dBm para reducir picos que algunas powerbanks leen como cortocircuito (`config.h`).
- CONFIRMAR CON EL EQUIPO: ¿La cámara usa una segunda powerbank (`README.md`, capítulo 2) o comparte la del DevKit (`docs/seguridad.md`, `docs/referencia/hardware.md`)?
- CONFIRMAR CON EL EQUIPO: ¿Qué pilas AA se usan en la demostración, alcalinas (unos 6 V) o NiMH (unos 4.8 V), y por qué cuatro?
- **PWM de 1,500 Hz.** `config.h` dice que da más par bajo carga que 5,000 Hz, a costa de algo de zumbido; no encontramos la medición.
- CONFIRMAR CON EL EQUIPO: ¿Se comparó 1,500 Hz con 5,000 Hz en el carro?
- **Arranque pleno, perfiles y balance.** Con menos de 255 desde parado los motores solo zumbaban: cada arranque va a 255 por 250 ms. MANUAL usa 255 y AUTO 210 (menos recorrido mientras llega el STOP). El lado izquierdo es el más rápido y se le baja la potencia: con 255/255 el carro se desvió 14.5 cm a la derecha en 150 cm, con 248 se pasó 16 cm a la izquierda y con 252 quedó a 3 cm (`config.h`). El factor (988 en MANUAL, 986 en AUTO, en milésimas) lo cambia la app (subsistema B).
- **STOP deja las ruedas libres.** El driver permitiría freno activo, pero cambiarlo invalidaría las calibraciones sin medición (`docs/reporte_J16.md`, `docs/investigacion_autonomo/10_seguridad_funcional.md`). Consecuencia: inercia (A.5).
- **Montaje de los sensores.** El giroscopio va plano sobre la caja de pilas, flecha X al frente y eje Z arriba, para que girar a la izquierda dé ángulo positivo (`IMU_SIGNO` = 1). El láser va al frente, de 6 a 8 cm del piso y de 5 a 8° hacia arriba, para no ver el piso (`docs/sensores.md`, `docs/validacion_sensores.md`). Los empalmes van soldados o con regleta: un SCL flojo dejó al giroscopio invisible el 18-oct (`docs/conexiones.md` §8).
- CONFIRMAR CON EL EQUIPO: ¿A qué altura, con qué inclinación y a cuántos mm del frente del carro está montado el láser? (`docs/sensores.md` dice 6 a 8 cm y de 5 a 8° hacia arriba; el capítulo 3 dice horizontal; el simulador usa 55 mm.)
- CONFIRMAR CON EL EQUIPO: ¿Qué hay conectado al GPIO 2 (resistencia, LED o nada) y la powerbank realmente necesita el pulso?
- CONFIRMAR CON EL EQUIPO: ¿Qué marcado trae el driver (TC1508A o TC1508S) y cómo se rotulan sus entradas (IN1 a IN4, INT1 a INT4 o INA a IND)?

## A.4 Cómo lo probamos y con qué resultado

| Qué se probó | Cómo | Resultado | Etiqueta |
|---|---|---|---|
| Motores y cableado | App con el carro en el piso; `docs/bitacora.md`, 13-oct, anterior a v1.1 | Cada botón mueve el lado correcto; se detiene al soltar | **VALIDADO EN EL CARRO** |
| Recta de 150 cm en MANUAL | Varias potencias del lado izquierdo; `config.h`, 13-oct, anterior a v1.1 | 255/255: 14.5 cm a la derecha; 248: 16 cm a la izquierda; 252: 3 cm | **VALIDADO EN EL CARRO** |
| Recta de 150 cm en AUTO | 4 corridas sin RUMBO; `docs/validacion_sensores.md`, 27-oct, firmware `453ed20` (anterior a v1.1) | Desvío de 5.5 a 20 cm; el ajuste 207/210 sigue sin afinar (`config.h`) | **VALIDADO EN EL CARRO** y **PENDIENTE** (afinar) |
| Velocidad y frenado | Calibración con la app; `docs/latencia.md`, 13 y 14-oct | A 255: 38 cm/s y 0.9 cm de frenado; en AUTO: 19.3 cm/s y 1.4 cm; el 15-nov, unos 30 cm/s en AUTO (`docs/limites_conocidos.md`) | **VALIDADO EN EL CARRO** (caduca con las pilas) |
| Pivote de 90° | Giros con cinta; `docs/validacion_sensores.md`, 18-oct | A la derecha: 15 cm adelante y 16 cm al lado; a la izquierda: radio de 8.7 a 9 cm, poco medido | **VALIDADO EN EL CARRO** |
| Giroscopio: sentido y deriva | `GIRAR:+90` y 60 s en reposo; `docs/validacion_sensores.md`, 18 y 20-oct, anterior a v1.1 | Positivo a la izquierda sin tocar `IMU_SIGNO`; deriva de 0.0° en 60 s; sesgo de −0.44 a −0.48 °/s | **VALIDADO EN EL CARRO** |
| Giroscopio y láser en v1.1 | Consulta por USB al cargar el firmware; `docs/estado_20261110.md` §8, 10-nov | `I2C:29 68`, `IMU:0,-44,2,104,0` (lista) y `TOF:1200,1` (libre) | **VALIDADO EN EL CARRO** |
| Láser: exactitud y arranque | USB; `docs/validacion_sensores.md`, 20-oct, y `docs/sensores.md`, 17-oct, anteriores a v1.1 | Piso vacío: 52 lecturas de 1,200 mm (libre), 0 falsos obstáculos. Objeto a unos 37 cm (sin regla): 152 lecturas de 367 a 378 mm, promedio 371 mm. Con 5 reintentos iniciaron 8 de 8 arranques; sin reintentos fallaba 1 de cada 3 | **VALIDADO EN EL CARRO** |
| Giroscopio: ángulo físico y escala | — | Todos los «90°» los midió el propio giroscopio, sin cinta; `IMUS=0` | **NO MEDIDO** |
| Láser con patas, objetos negros mate o brillantes, sol | — | No se probó con superficies reales; el banco simula muebles con un modelo supuesto | **NO MEDIDO** y **SIMULADO** (banco) |
| Corriente, tensión bajo carga, autonomía, GPIO 2, ruido del PWM en I2C, sesgo con motores en marcha, pines del driver al reiniciar | — | Ninguno se midió (`docs/conexiones.md` §7). El arranque escalonado (`ARRANQUE_ESCALONADO_MS`, apagado) falta probarlo | **NO MEDIDO** y **PENDIENTE** |

## A.5 Límites conocidos

- **Sin movimiento lateral y con giro asimétrico.** Al girar 90° el centro se desplaza de 15 a 17 cm, distinto a cada lado (A.2). El navegador lo modela con un promedio que cambia con piso y pilas.
- **Sin encoders.** La distancia se calcula por tiempo. Con la misma orden PWM, la velocidad en AUTO pasó de 19.3 cm/s (14-oct) a unos 30 cm/s (15-nov), porque cambiaron pilas y contactos. Conviene recalibrar con las pilas de la demostración (`docs/limites_conocidos.md`).
- **STOP no frena: suelta las ruedas.** La inercia añade 1.4 cm al avanzar en AUTO y unos 15° en cada giro de 90° (`docs/latencia.md`, `docs/reporte_J16.md`).
- **Corriente sin medir y sin protección conocida.** Dos motores TT bloqueados en un canal podrían pedir más que los 1.8 A continuos del driver (estimación del repo de 2.2 a 3 A, no medida). El firmware no detecta atascos y la documentación consultada no menciona protección térmica ni por sobrecorriente. Si el módulo fuera un TC1508S, su máximo absoluto (5.5 V) quedaría bajo cuatro pilas alcalinas nuevas (`04_motores_cinematica.md` §2 y §6).
- **Láser: haz ancho y objetos difíciles.** Con un cono de unos 25°, una pata de 25 a 35 mm a 120 mm cubre más de la mitad del haz, pero a 400 mm cubre de 14 a 20 % y el fondo la oculta; un objeto más bajo que unos 4 cm queda bajo el haz (cálculos del repo, no medidos). Un objeto negro mate, brillante o transparente puede dar «sin eco», que el firmware toma como libre, y con luz exterior el alcance baja a 40 o 60 % del de interior (`01_vl53l0x.md` §2 y §3). Altura, inclinación y distancia del sensor al frente no están medidas; los 120 mm entre sensor y centro son supuestos (`docs/limites_conocidos.md`).
- **El ángulo del giroscopio es relativo y no se verificó con cinta.** Se obtiene sumando velocidades, así que deriva con el sesgo residual (no medido con motores en marcha) y su escala de fábrica (±3 %) no está calibrada. El acelerómetro no se lee ni daría rumbo (`02_mpu6050.md`).
- **Arranque del giroscopio.** El sesgo se mide en 2.5 s con el carro quieto; si se mueve, el giroscopio queda en «MAL» (pasó al encender con la powerbank el 20-oct) y el firmware reintenta cada 5 s solo con los motores detenidos (`docs/validacion_sensores.md`).
- **Alimentación sin vigilancia.** No se lee la tensión de las pilas y cuatro AA dejan poco margen contra los 7.2 V del driver; tampoco está confirmado que el pulso del GPIO 2 mantenga encendida la powerbank.
- **Un solo bus I2C y pines del driver al reiniciar.** No se usa el pin XSHUT del láser; el driver I2C del core limpia el bus tras un tiempo agotado, pero no se probó en el carro (`03_esp32_arduino.md` §1.5). Al reiniciarse la ESP32, los GPIO 25, 26, 27 y 33 arrancan sin salida habilitada; que el driver los lea como bajo depende de él o de un pull-down externo, y no se midió (`docs/seguridad.md` §3.2).

## A.6 Términos clave

- **ESP32 DevKit**: placa con el microcontrolador ESP32 (WiFi, 160 MHz en el carro) donde corre el firmware; un GPIO es un pin que el firmware sube o baja.
- **TC1508A**: driver (puente H doble) que convierte las señales de 3.3 V de la ESP32 en corriente para los motores; un puente H invierte la corriente.
- **PWM**: señal que se enciende y apaga muy rápido; el porcentaje encendido (0 a 255) fija la potencia. En el carro, 1,500 Hz y 8 bits.
- **Motor TT**: motorreductor de plástico (típico 1:48, de 3 a 6 V) sin encoder.
- **Rueda mecanum**: rueda con rodillos a 45°; controlada por separado permite moverse de lado, pero en este carro no.
- **Tracción diferencial**: avanzar y girar variando la potencia de cada lado.
- **Pivote**: giro apoyado en un solo lado (`LEFT`, `RIGHT`); el centro del carro se desplaza.
- **Rueda libre**: motor sin corriente (PWM 0 en las dos entradas); es lo que hacen `STOP` y `EBRAKE`.
- **MPU6050**: giroscopio y acelerómetro (módulo GY-521); el carro solo usa el eje Z del giroscopio.
- **Sesgo (bias)**: lectura del giroscopio con el carro quieto (cerca de −0.45 °/s); se mide al arrancar.
- **VL53L0X**: sensor láser de tiempo de vuelo (ToF) que mide la distancia al frente.
- **I2C**: bus de dos cables (SDA y SCL) que comparten el giroscopio y el láser.
- **GND común**: tierra compartida por pilas, driver y ESP32; sin ella el driver no entiende las señales PWM.

## A.7 Preguntas probables del profesor

1. **Si las ruedas son mecanum, ¿por qué el carro no se mueve de lado?** Porque el TC1508A tiene solo dos canales: las dos ruedas de cada lado reciben la misma señal y el carro se comporta como tracción diferencial de cuatro ruedas. Para moverse de lado haría falta controlar las diagonales por separado. El repo no dice por qué se eligieron ruedas mecanum.
2. **¿Por qué tres fuentes de energía y no una batería con un reductor?** Los motores TT dan picos de corriente y ruido inductivo, y la ESP32 puede reiniciarse si su tensión baja demasiado; separar las fuentes evita que los motores afecten a la electrónica (`docs/seguridad.md`, `docs/exposicion.md`). Las corrientes reales no las medimos: es una razón de diseño, no una medición.
3. **¿Por qué un giroscopio y no encoders?** Los engranes de los TT tienen holgura y las llantas patinan; un encoder no vería el patinaje y el giroscopio sí mide el giro del chasis. A cambio, el giroscopio solo da el ángulo: la distancia sigue siendo por tiempo y su escala no está calibrada (`IMUS=0`).
4. **¿Qué ve el láser y qué no?** Mide la distancia al frente con un haz de unos 25°. Midió 371 mm contra un objeto a unos 37 cm (sin regla) y no reaccionó a uno a 34° fuera del eje. No lo probamos con patas, objetos negros o brillantes ni con sol: pueden dar «sin eco», que el firmware toma como libre.
5. **¿Qué del hardware no han medido?** La corriente de arranque y de motor bloqueado, la tensión de las pilas bajo carga, la autonomía, qué hay en el GPIO 2, el ruido del PWM en I2C, el sesgo con motores en marcha y la geometría del láser: todo **NO MEDIDO**.

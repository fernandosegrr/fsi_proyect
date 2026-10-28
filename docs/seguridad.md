# Seguridad física, eléctrica y lógica del carro FSI

**Materia:** Fundamentos de los Sistemas de Información (FSI)  
**Propósito:** Análisis exhaustivo de los riesgos operativos y las capas de protección implementadas en hardware y software.  
**Fecha:** 27 de octubre de 2026  

---

## 1. Seguridad eléctrica: aislamiento y tierras compartidas

### 1.1 El principio de dos fuentes separadas
El sistema móvil requiere alimentar dos clases de cargas eléctricas con requerimientos radicalmente distintos:
- **Cargas digitales sensibles (microcontroladores y sensores):** Exigen una tensión constante y libre de fluctuaciones (5 V USB regulados internamente a 3.3 V). Son vulnerables a reinicios espurios (*brownouts*) si el voltaje cae por debajo de 2.7 V.
- **Cargas inductivas ruidosas (motores eléctricos TT):** Generan picos de corriente elevados al arrancar (hasta 1.5 A combinados) y ruido inductivo de retorno al frenar o conmutar el sentido de giro.

Por esta razón, **está estrictamente prohibido alimentar los motores desde la misma powerbank o regulador de la placa ESP32**.

```
  Powerbank 5 V ──────────► ESP32 DevKit (5 V) ──► Sensores I2C (3.3 V)
  (Electrónica) ──────────► Freenove Cámara (5 V)
                                 │
                            GND COMÚN (0 V)
                                 │
  4 Pilas AA (~6 V) ──────► TC1508A (VDD) ──────► 4 Motores TT
  (Potencia)
```

### 1.2 Regla de oro del cableado
> **ADVERTENCIA CRÍTICA:** El terminal positivo (+ ~6 V) proveniente del portapilas de 4 pilas AA debe conectarse **ÚNICA Y EXCLUSIVAMENTE** al pin `VDD` del driver TC1508A. **Bajo ninguna circunstancia debe tocar ningún pin de la placa ESP32 ni de los módulos sensores**. Conectar 6 V a los pines de 3.3 V o 5 V del ESP32 destruirá irreversiblemente el microcontrolador.

### 1.3 Necesidad del bus de tierra común (GND)
Las señales PWM que controlan la velocidad y dirección de los motores se transmiten como niveles de voltaje (0 V = bajo, 3.3 V = alto) desde los GPIOs del ESP32 hacia las entradas lógicas `INA`, `INB`, `INC` e `IND` del driver TC1508A.

Para que el driver reconozca estos niveles, **la tierra de la powerbank y la tierra de las pilas AA deben estar rígidamente unidas**. Si la conexión de tierra común se interrumpe:
- Los voltajes de control flotan sin referencia fija.
- El driver puede entrar en conducción descontrolada, dejando los motores encendidos a plena potencia de manera impredecible.

---

## 2. Seguridad térmica y mecánica

### 2.1 Límites de corriente del driver TC1508A
El circuito integrado TC1508A soporta una corriente continua de 1.8 A y picos máximos de hasta 2.5 A por canal:
- Con las cuatro ruedas girando libremente, el consumo típico ronda los 300–500 mA.
- Si el vehículo queda atrapado contra una pared o las ruedas se bloquean mecánicamente, la corriente de bloqueo puede elevarse rápidamente a más de 2 A, provocando el calentamiento acelerado del encapsulado del driver.
- **Protección operativa:** Nunca forzar el avance del carro si se encuentra atascado físicamente. En caso de colisión, presionar de inmediato `EBRAKE`.

### 2.2 Temporizador de corte en giros (`GIRO_TIMEOUT`)
Al ejecutar maniobras de giro angular (`GIRAR:<grados>`), si el carro no logra alcanzar el ángulo objetivo antes de un tiempo límite preestablecido (por ejemplo, si una rueda resbala sobre una superficie jabonosa o queda trabada), el firmware aborta el giro, apaga los motores y emite la alarma `GIRO_TIMEOUT` hacia la app.

---

## 3. Seguridad lógica y vigilancia por software (Watchdogs)

El sistema integra tres niveles independientes de supervisión automática:

### 3.1 Watchdog de comunicación (1000 ms)
- **Función:** Supervisa la continuidad del enlace inalámbrico entre la app y el carro.
- **Comportamiento:** Si transcurren **1000 ms** sin que el ESP32 reciba un comando válido de movimiento (`FORWARD`, `REVERSE`, `LEFT`, `RIGHT`), el firmware asume que el teléfono se apagó, la app crasheó o se perdió la cobertura WiFi. En el milisegundo **1001**, corta inmediatamente la señal PWM a los cuatro motores, establece estado `STOP` y restablece el perfil de velocidad a `AUTO` (`carro.c:245-265`).
- **Inmunidad del PING:** Los mensajes de sondeo de latencia `PING` no reinician el temporizador del watchdog, forzando a la app a enviar comandos explícitos de avance.

### 3.2 Watchdog de hardware del microcontrolador (5000 ms)
- El temporizador de vigilancia por hardware del ESP32 (`esp_task_wdt`) está configurado a 5000 ms (`firmware_carro.ino`).
- En cada ciclo de la función `loop()`, se ejecuta `esp_task_wdt_reset()`.
- Si el hilo principal sufriera un bloqueo indefinido por alguna transacción I2C colgada o fallo de memoria, el microcontrolador se reinicia automáticamente en frío, llevando de inmediato los pines de motor a nivel bajo por hardware.

### 3.3 Parada de emergencia por proximidad láser (Autoridad ToF)
- En perfil `AUTO`, el sensor láser VL53L0X mide continuamente la distancia hacia el frente.
- Si detecta un objeto dentro de la zona de seguridad mínima predefinida, el firmware interrumpe la tracción de forma autónoma con prioridad sobre las órdenes de la app, deteniendo el vehículo e informando `TOF_PARO:<mm>`.

---

## 4. Referencias visuales

- [`docs/figuras/fig_10_alimentacion.png`](file:///e:/Dev/fsi_proyecto/docs/figuras/fig_10_alimentacion.png): Esquema de distribución eléctrica y advertencia de voltajes.
- [`docs/figuras/fig_03_flujo_comando.png`](file:///e:/Dev/fsi_proyecto/docs/figuras/fig_03_flujo_comando.png): Arquitectura del watchdog y cola prioritaria de frenado.

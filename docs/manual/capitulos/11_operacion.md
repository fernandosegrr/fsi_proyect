# 11. Operación y seguridad física

**Revisión del 11-nov-2026:** rama `fix/navegacion-esquive-confirmado`, basada en v1.1. El arreglo y la prueba con banquito de 35×45 cm están **NO MEDIDOS**: primero debe repetirse con caja de 20×20 cm, previa autorización. Los resultados físicos J16 de abajo son históricos y no validan esta rama. No se instala APK, graba firmware ni declara v1.2.

## 11.1 Lista de verificación previa al encendido

Antes de conectar cualquier cable USB o accionar el interruptor del portapilas, el operador debe comprobar cinco puntos físicos sobre el chasis:

1. **Ruedas libres y chasis elevado para la prueba inicial:** verificar que ningún cable roce las cuatro llantas omnidireccionales ni las cajas reductoras `TT`.
2. **Integridad de la tierra común (`GND`):** confirmar que el cable negro de referencia une el negativo del portapilas `4×AA`, el pin `GND` del puente H `TC1508A` y el pin `GND` de la placa `ESP32 DevKit`.
3. **Aislamiento del positivo de tracción (`VDD`):** comprobar que el cable rojo del portapilas `4×AA` entra exclusivamente al borne `VDD` del driver `TC1508A` y no toca ningún pin de `5 V` o `3.3 V` de la `ESP32 DevKit`.
4. **Carga de las tres baterías:** asegurar que las dos *powerbanks* USB tengan carga suficiente y que las cuatro pilas `AA` estén recién cargadas o dentro de su ventana operativa (**máximo 20 corridas o 90 minutos de uso acumulado**).
5. **Pista interior despejada y uniforme:** retirar objetos pequeños (< `5 cm` de altura, que pueden quedar por debajo del haz del láser `VL53L0X`), secar cualquier líquido del piso y disponer de al menos `2.0 m × 1.5 m` libres con iluminación uniforme.

## 11.2 Secuencia de encendido y calibración en reposo

El orden de encendido es importante porque el firmware de la `ESP32 DevKit` calibra el sesgo en reposo (`bias`) del giroscopio `MPU6050` durante el primer medio segundo tras recibir energía (`100` muestras a `200 Hz`), y porque la cámara `Freenove` necesita que el punto de acceso `CarroFSI` ya exista para asociarse a él:

| Paso | Acción del operador | Resultado esperado y verificación |
|---|---|---|
| **1. Mantener motores apagados** | Colocar el interruptor del portapilas `4×AA` en **`OFF`** y apoyar el carro inmóvil sobre el piso plano. | Los motores no pueden moverse mientras el microcontrolador arranca y calibra el giroscopio. |
| **2. Energizar `ESP32 DevKit`** | Conectar la **Powerbank USB #1** a la placa `ESP32 DevKit` sin tocar ni mover el chasis durante `2 segundos`. | La placa crea la red `CarroFSI` (`192.168.4.1`) y completa la calibración inicial de `MPU6050` (`imu_calibrada = 1`). |
| **3. Energizar cámara `Freenove`** | Conectar la **Powerbank USB #2** (propia de la cámara) a la placa `Freenove ESP32-WROVER`. | La cámara se une como cliente a `CarroFSI` con la IP `192.168.4.2` y activa `/stream` (`:81`) y `UDP :4210`. |
| **4. Conectar el teléfono Android** | Unir el teléfono a la red WiFi `CarroFSI` (desactivando el cambio automático a datos móviles) y abrir la app **`LIDIA`**. | La pestaña `Inicio` muestra estado `Conectado`, imagen en vivo de la cámara y versión `FW:6d1b213-20261110`. |
| **5. Verificar `Sensores`** | Abrir `Más → Sensores` y revisar ángulo, distancia láser y banderas (`flags = 7`). Si el carro se movió al conectarlo, pulsar `Calibrar IMU` (`CAL_IMU`). | El ángulo marca `0.0°` estable en reposo, el láser mide la distancia real en milímetros y `RUMBO` aparece habilitado (`ON`). |
| **6. Energizar tracción (`4×AA`)** | Pasar el interruptor del portapilas `4×AA` a **`ON`**. | El puente H `TC1508A` recibe potencia y el vehículo queda listo para moverse. |

## 11.3 Operación en modo manual y ejecución supervisada de rutas

### Conducción manual (`Pestaña Manual`)

- Seleccionar el perfil de potencia deseado: **`AUTO` (`207/210`)** para movimientos suaves y controlados o **`MANUAL` (`252/255`)** para velocidad plena.
- Mientras se mantiene presionado un botón direccional (`Adelante`, `Atrás`, `Izquierda`, `Derecha`), la aplicación envía la orden cada `250 ms`. Al soltar el botón, envía inmediatamente `STOP`.
- Si durante un avance en perfil `AUTO` el sensor láser detecta un objeto a menos de `120 mm`, el carro frena por sí mismo, emite `TOF_PARO` y la aplicación corta el reenvío de `FORWARD` hasta que el usuario suelte el botón y retroceda o gire.

### Ejecución de una ruta semiautónoma (`Pestaña Rutas`)

1. **Aprendizaje previo del color del piso:** antes de cada nueva ruta con cámara y siempre que cambie la iluminación, abrir `Más → Visión`, apuntar el carro hacia el piso despejado y pulsar **`Aprender piso`**. Debe terminar correctamente; un intento fallido invalida el anterior y un segundo toque en Iniciar no lo sustituye. Comprobar en la superposición gráfica que el trapecio del corredor aparece libre (verde) sobre el suelo y se marca como ocupado (rojo/naranja) al colocar la caja de prueba de `20×20 cm` a `60 cm` del frente.
2. **Revisión del estado de calibración (J16):** al abrir la pestaña `Rutas`, verificar que no aparezca el cintillo de advertencia de calibración inválida o sin medir.
3. **Selección y simulación previa:** elegir una ruta predefinida (por ejemplo, `Recta 150 cm` o una ruta de esquive) o trazarla en la cuadrícula de `20 cm`, y pulsar primero **`Simular`** para inspeccionar en pantalla la secuencia de pasos que calculará el núcleo en `C99`.
4. **Inicio desde `0.0°`:** alinear físicamente el carro con el eje de salida, pulsar **`Ejecutar`** (lo que envía `MODO_AUTO`, consulta/activa `RUMBO` y pone a cero la referencia inicial) y acompañar visualmente el recorrido a menos de un metro de distancia, listo para pulsar el botón rojo **`PARO`** si fuera necesario.

## 11.4 Jerarquía de paro de emergencia y mantenimiento de baterías

Ante cualquier comportamiento inesperado, el sistema dispone de cuatro mecanismos de detención ordenados por inmediatez:

1. **Paro por liberación o botón `PARO` en pantalla (`< 50 ms`):** soltar el control manual o tocar el botón rojo **`PARO`** envía `STOP` con prioridad máxima en la cola del firmware.
2. **Paro automático por láser frontal (`< 20 ms` locales):** cualquier objeto dentro del cono del `VL53L0X` a menos de `120 mm` detiene los motores en el propio microcontrolador.
3. **Paro por *watchdog* o silencio de sensores (`1.0 s` a `1.5 s`):** apagar la pantalla del teléfono, cerrar la aplicación o desconectar el WiFi detiene el carro a lo sumo en `1,000 ms` (`WD:<n>`).
4. **Corte físico de energía (`instantáneo`):** apagar el interruptor del portapilas `4×AA` corta directamente la alimentación `VDD` del puente H `TC1508A` sin apagar el microcontrolador ni perder la sesión de diagnóstico.

### Regla operativa de baterías (`20 corridas` o `90 minutos`)

Dado que la distancia longitudinal en `nucleo_c` se estima a partir del tiempo de activación (`ms_por_cm`), cuando el voltaje de las cuatro pilas `AA` cae por descarga el vehículo avanza menos centímetros por cada segundo de comando. Para mantener la validez de la calibración longitudinal, **las cuatro pilas `AA` deben recargarse o sustituirse cada 20 corridas de ruta o cada 90 minutos de operación acumulada**, y nunca deben mezclarse celdas nuevas con celdas usadas.

Dónde está en el código: `ManualFragment.kt`, `EjecutarRutaFragment.kt`, `ControlLogica.kt`, `ModoSemiautonomo.kt`, `SensoresActivity.kt`, `VisionActivity.kt`, `carro.c`, `tof.c`.

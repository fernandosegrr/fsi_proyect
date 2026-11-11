# 15. Solución de problemas

Este capítulo proporciona una guía sistemática de diagnóstico y resolución de fallas para los problemas que pueden presentarse durante la preparación, conexión u operación del vehículo **LIDIA**. Sigue el principio de aislar una sola variable a la vez, comenzando siempre por la alimentación eléctrica y las conexiones físicas con los motores apagados.

## 15.1 Método general de aislamiento de fallas

Cuando ocurre un comportamiento inesperado en el sistema, modificar simultáneamente cables, baterías y ajustes de la aplicación impide identificar la causa raíz. El procedimiento recomendado consta de cuatro pasos:

1. **Inmovilizar el vehículo:** pulsar `PARO` en la aplicación o pasar el interruptor del portapilas `4×AA` a `OFF` y elevar el chasis antes de revisar el cableado.
2. **Separar los tres enlaces:** comprobar primero si existe la red `CarroFSI` (`192.168.4.1`), después si responde el control por `WebSocket` (`ESTADO` / `SENS?`) y por separado si llega video desde `192.168.4.2`.
3. **Consultar la telemetría interna:** abrir `Más → Sensores` (o el monitor serie a `115,200` baudios) para leer las banderas de `SENS:<ang_x10>,<mm>,<flags>` y `RUMBO?`.
4. **Cambiar una variable por vez:** tras ajustar un cable, recargar una batería o recalibrar el piso, repetir una prueba corta y registrar el resultado.

## 15.2 Tabla maestra de diagnóstico por síntoma

| Síntoma observado | Causa probable | Verificación inmediata | Solución paso a paso |
|---|---|---|---|
| **1. No aparece la red WiFi `CarroFSI` en el teléfono** | Powerbank #1 descargada, cable micro-USB de solo carga flojo o `ESP32 DevKit` sin arrancar. | Revisar LED de encendido en la `ESP32 DevKit` y carga de la Powerbank #1. | Conectar la Powerbank #1 con un cable firme, presionar el botón `EN`/`RST` de la `ESP32 DevKit` y esperar `3 s`. |
| **2. Conectado a `CarroFSI`, pero la app dice `Reconectando` o sin control** | El teléfono redirige el tráfico por datos móviles al ver que `CarroFSI` no tiene internet, o hay otro cliente conectado. | Abrir en el navegador `http://192.168.4.1/` o revisar si está activo el cambio inteligente de red en Android. | Desactivar datos móviles o aceptar `"Mantener conexión WiFi sin internet"` en Android; verificar que la IP en `Ajustes` sea `192.168.4.1`. |
| **3. Hay control del carro, pero no se ve video de la cámara** | Powerbank #2 (propia de la cámara) apagada, cámara aún no asociada a `CarroFSI` o segundo cliente bloqueando `/stream`. | Verificar LED en la placa `Freenove` y abrir `http://192.168.4.2/stats` desde el teléfono. | Encender la Powerbank #2 **después** de que la `ESP32 DevKit` ya creó `CarroFSI`; cerrar cualquier pestaña de navegador que tenga abierto `:81/stream`. |
| **4. La app envía órdenes (`OK`), pero ninguna rueda gira** | Interruptor del portapilas `4×AA` en `OFF`, pilas agotadas o falta el cable de tierra común (`GND`). | Medir o revisar el portapilas `4×AA` y la unión `GND` entre `DevKit`, `TC1508A` y pilas. | Pasar el interruptor a `ON`, comprobar que el cable `GND` común esté firme en la protoboard y usar 4 pilas `AA` con carga completa. |
| **5. En modo `AUTO`, el botón `Adelante` no avanza y muestra `TOF_PARO`** | El sensor láser frontal `VL53L0X` detecta un objeto o la mano del operador a `< 120 mm` (bandera `flags & 16`). | Abrir `Más → Sensores` y leer la distancia en milímetros (`< 120 mm`). | Retirar el obstáculo frontal hasta que la lectura supere `150 mm` (umbral de histéresis `TOF_LIBRE_MM`) o usar `Atrás`/`Izquierda`/`Derecha`. |
| **6. El ángulo en `Sensores` deriva rápido con el carro quieto o devuelve `CAL_ERR_MOV`** | El vehículo fue movido durante el primer segundo tras encenderse o la mesa/piso vibra al pulsar `Calibrar IMU`. | Observar si la bandera `imu_calibrada` (`bit 1`, valor `2`) está en `0` dentro de `flags`. | Apoyar el carro en el piso firme sin tocarlo, abrir `Más → Sensores`, pulsar `Calibrar IMU` (`CAL_IMU`) y esperar `1 s` hasta ver `CAL_OK`. |
| **7. El carro se curva sistemáticamente hacia un lado en rectas de ruta** | Control `RUMBO` desactivado (`OFF`), pilas `AA` descargadas o rueda con roce mecánico. | En `Más → Sensores`, pulsar `RUMBO?` y comprobar que devuelva `RUMBO:1,...` (`on=1`). | Activar el interruptor `Control de rumbo en rectas` (`RUMBO:ON`), verificar que ninguna llanta roce con cables y recalibrar `CAL_IMU` antes de salir. |
| **8. En giros acumulados (`90°` o `360°`) el carro gira sistemáticamente de más o de menos** | Tolerancia de sensibilidad del giroscopio MEMS (`±1 %` a `±3 %`) con `escala_ppm = 0`. | Consultar `IMU_ESCALA?` en `Sensores` y comparar un giro físico de `360°` contra el ángulo reportado. | Ejecutar en `Más → Sensores` el asistente de 3 pasos (`1. Poner a 0°` → girar `360°` físicos → `3. Calcular y guardar`) para persistir `IMU_ESCALA` en `NVS`. |
| **9. La cámara frena en falso en piso despejado o ignora la caja en ruta** | El modelo de color de suelo corresponde a otra habitación o cambió la iluminación ambiental. | Abrir `Más → Visión` y observar si el trapecio del corredor marca celdas rojas sobre el piso limpio. | Apuntar el carro hacia un tramo despejado del piso actual con la iluminación de trabajo y pulsar **`Aprender piso`**. |
| **10. La ruta se detiene sola a los `1.0` o `1.5 s` con aviso `Watchdog` o `Sin datos de sensores`** | El teléfono entró en suspensión de pantalla, se alejó fuera de cobertura WiFi o sufrió interferencia severa. | Revisar el mensaje en pantalla (`WD:<n>` o silencio de `SENS > 1.5 s`). | Mantener la pantalla del teléfono encendida en primer plano y caminar a menos de `2`–`3 m` del vehículo durante toda la ruta. |

## 15.3 Verificación rápida por puerto serie (cuando no hay WiFi disponible)

Si se desea diagnosticar la placa `ESP32 DevKit` directamente desde una computadora por cable USB sin usar el teléfono, basta abrir cualquier terminal serie a **`115,200` baudios** (terminador de línea `\n`) y enviar los cuatro comandos de inspección:

1. `VERSION?` → debe responder `FW:6d1b213-20261110`.
2. `ESTADO` → debe responder `AUTO` (o `MANUAL`) seguido de `CAPS:IMU=1,CAL=1,TOF=1,GIRO=1,RUMBO=1,IMUS=...,FW:6d1b213-20261110`.
3. `SENS?` → debe responder `SENS:<ang_x10>,<mm>,7` (donde `flags = 7` confirma que tanto el `MPU6050` en `0x68` como el `VL53L0X` en `0x29` están respondiendo en el bus `I2C`).
4. `RUMBO?` → debe responder `RUMBO:1,0,0,0,0,0` (confirmando que el controlador `PI` de rumbo está habilitado y listo).

Dónde está en el código: `carro.c`, `sensores.c`, `recuperacion.c`, `Enlace.kt`, `ModoSemiautonomo.kt`, `SensoresActivity.kt`.

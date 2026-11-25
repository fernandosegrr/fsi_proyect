# 15. Solución de problemas

Este capítulo proporciona una guía sistemática de diagnóstico y resolución de fallas para los problemas que pueden presentarse durante la preparación, conexión u operación del vehículo **LIDIA**. Sigue el principio de aislar una sola variable a la vez, comenzando siempre por la alimentación eléctrica y las conexiones físicas con los motores apagados.

## 15.1 Método general de aislamiento de fallas

Cuando ocurre un comportamiento inesperado en el sistema, modificar simultáneamente cables, baterías y ajustes de la aplicación impide identificar la causa raíz. El procedimiento recomendado consta de cuatro pasos:

1. **Inmovilizar el vehículo:** pulsar `PARO` en la aplicación o pasar el interruptor del portapilas `4×AA` a `OFF` y elevar el chasis antes de revisar el cableado.
2. **Separar los tres enlaces:** comprobar primero si existe la red `CarroFSI` (`192.168.4.1`), después si responde el control por `WebSocket` (`PING` y `PONG`, y las tramas `SENS` cada `100 ms`) y por separado si llega video desde `192.168.4.2`.
3. **Consultar la telemetría interna:** abrir `Más → Calibración → Abrir sensores` (o el monitor serie a `115,200` baudios) para leer las banderas de `SENS:<ang_x10>,<mm>,<flags>` y `RUMBO?`.
4. **Cambiar una variable por vez:** tras ajustar un cable, recargar una batería o recalibrar el piso, repetir una prueba corta y registrar el resultado.

## 15.2 Tabla maestra de diagnóstico por síntoma

| Síntoma observado | Causa probable | Verificación inmediata | Solución paso a paso |
|---|---|---|---|
| **1. No aparece la red WiFi `CarroFSI` en el teléfono** | Powerbank #1 descargada, cable micro-USB de solo carga flojo o `ESP32 DevKit` sin arrancar. | Revisar LED de encendido en la `ESP32 DevKit` y carga de la Powerbank #1. | Conectar la Powerbank #1 con un cable firme, presionar el botón `EN`/`RST` de la `ESP32 DevKit` y esperar `3 s`. |
| **2. Conectado a `CarroFSI`, pero la app dice `Reconectando` o sin control** | El teléfono redirige el tráfico por datos móviles al ver que `CarroFSI` no tiene internet, o hay otro cliente conectado. | Abrir en el navegador `http://192.168.4.1/` o revisar si está activo el cambio inteligente de red en Android. | Desactivar datos móviles o aceptar `"Mantener conexión WiFi sin internet"` en Android; verificar que la IP en `Ajustes` sea `192.168.4.1`. |
| **3. Hay control del carro, pero no se ve video de la cámara** | Powerbank #2 (propia de la cámara) apagada, cámara aún no asociada a `CarroFSI` o segundo cliente bloqueando `/stream`. | Verificar LED en la placa `Freenove` y abrir `http://192.168.4.2/stats` desde el teléfono. | Encender la Powerbank #2 **después** de que la `ESP32 DevKit` ya creó `CarroFSI`; cerrar cualquier pestaña de navegador que tenga abierto `:81/stream`. |
| **4. La app envía órdenes (`OK`), pero ninguna rueda gira** | Interruptor del portapilas `4×AA` en `OFF`, pilas agotadas o falta el cable de tierra común (`GND`). | Medir o revisar el portapilas `4×AA` y la unión `GND` entre `DevKit`, `TC1508A` y pilas. | Pasar el interruptor a `ON`, comprobar que el cable `GND` común esté firme en la protoboard y usar 4 pilas `AA` con carga completa. |
| **5. En modo `AUTO`, el botón `Adelante` no avanza y muestra `TOF_PARO`** | El sensor láser frontal `VL53L0X` detecta un objeto o la mano del operador a `< 120 mm` (bandera `flags & 16`). | Abrir `Más → Calibración → Abrir sensores` y leer la distancia en milímetros (`< 120 mm`). | Retirar el obstáculo frontal hasta que la lectura supere `150 mm` (umbral de histéresis `TOF_LIBRE_MM`) o usar `Atrás`/`Izquierda`/`Derecha`. |
| **6. El ángulo en `Sensores` deriva rápido con el carro quieto o devuelve `NAK_IMU_CAL`** | El vehículo fue movido durante el primer segundo tras encenderse o la mesa/piso vibra al pulsar `Calibrar IMU`. | Observar si la bandera de giroscopio listo (`bit 0`, valor `1`) está en `0` dentro de `flags`. | Apoyar el carro en el piso firme sin tocarlo, abrir `Más → Calibración → Abrir sensores`, pulsar `Calibrar IMU` (`IMU_CAL`) y esperar unos `3 s` hasta ver `IMU_CAL_OK`. |
| **7. El carro se curva sistemáticamente hacia un lado en rectas de ruta** | Control `RUMBO` desactivado (`OFF`), pilas `AA` descargadas o rueda con roce mecánico. | En `Más → Calibración → Abrir sensores`, pulsar `RUMBO?` y comprobar que devuelva `RUMBO:1,...` (`on=1`). | Activar el interruptor `Control de rumbo en rectas` (`RUMBO:ON`), verificar que ninguna llanta roce con cables y recalibrar `IMU_CAL` antes de salir. |
| **8. En giros acumulados (`90°` o `360°`) el carro gira sistemáticamente de más o de menos** | Tolerancia de sensibilidad del giroscopio MEMS (`±1 %` a `±3 %`) con `escala_ppm = 0`. | Consultar `IMU_ESCALA?` en `Sensores` y comparar un giro físico de `360°` contra el ángulo reportado. | Ejecutar en `Más → Calibración → Abrir sensores` el asistente de 3 pasos (`1. Poner a 0°` → girar `360°` físicos → `3. Calcular y guardar`) para persistir `IMU_ESCALA` en `NVS`. |
| **9. La cámara frena en falso en piso despejado o ignora la caja en ruta** | El modelo de color de suelo corresponde a otra habitación o cambió la iluminación ambiental. | Abrir `Más → Visión` y observar si el trapecio del corredor marca celdas rojas sobre el piso limpio. | Apuntar el carro hacia un tramo despejado del piso actual con la iluminación de trabajo y pulsar **`Aprender piso`**. |
| **10. La ruta se detiene sola a los `1.0` o `1.5 s` con aviso `Watchdog` o `Sin datos de sensores`** | El teléfono entró en suspensión de pantalla, se alejó fuera de cobertura WiFi o sufrió interferencia severa. | Revisar el mensaje en pantalla (`WD:<n>` o silencio de `SENS > 1.5 s`). | Mantener la pantalla del teléfono encendida en primer plano y caminar a menos de `2`–`3 m` del vehículo durante toda la ruta. |

## 15.3 Verificación rápida por puerto serie (cuando no hay WiFi disponible)

Si se desea diagnosticar la placa `ESP32 DevKit` directamente desde una computadora por cable USB sin usar el teléfono, basta abrir cualquier terminal serie a **`115,200` baudios** (terminador de línea `\n`) y revisar estos cuatro puntos:

1. `CAPS?` → debe responder una línea `CAPS:...` que termina en `FW:6d1b213-20261110`.
2. Esa misma línea debe traer `IMU=1`, `TOF=1` y `RUMBO=1` (giroscopio listo, láser vivo y control de rumbo disponible).
3. Con el perfil `AUTO` y un cliente conectado, el carro manda solo `SENS:<ang_x10>,<mm>,7` cada `100 ms` (donde `flags = 7` confirma que tanto el `MPU6050` en `0x68` como el `VL53L0X` en `0x29` están respondiendo en el bus `I2C`).
4. `RUMBO?` → debe responder `RUMBO:1,0,0,0,0,0` (confirmando que el controlador `PI` de rumbo está habilitado y listo).

Dónde está en el código: `carro.c`, `sensores.c`, `recuperacion.c`, `Enlace.kt`, `ModoSemiautonomo.kt`, `SensoresActivity.kt`.

## 15.4 Autónomo v2: causas nuevas y caja negra

Rama `feat/autonomo-v2` (`SIMULADO`; la app solo se compiló).

| Síntoma | Qué significa | Qué hacer |
|---|---|---|
| «Causa 16: pared u objeto muy ancho…» | El láser midió los lados al girar y no encontró por dónde rodear, o medir el otro lado no era seguro | Quitar el objeto o dibujar otra ruta. El carro no se acercó |
| «Causa 17: el detector de objetos dejó de dar resultados…» | La red neuronal lleva 2.5 s sin un resultado vigente | Revisar en Visión que el modelo esté cargado; reiniciar la app |
| «Causa 18: el láser vio algo dentro del arco del giro…» | Al girar para rodear había una pared o un objeto pegado al costado; el carro dejó de girar para no barrerlo | Tomar el control, alejar el carro de eso y continuar, o dibujar la ruta más lejos de la pared |
| «Aviso: Percepción incompleta: …» al iniciar | La ruta sale sin láser, sin detector de objetos o sin cámara | Decidir si conviene salir así; sin láser nada frena por distancia |
| El carro tuerce unos grados y vuelve sin frenar | Es el vistazo: el láser mira algo que solo vio el color | Nada. Si confirma, se detiene y rodea |
| El carro se queda medio segundo quieto antes de volver a la línea | Es la mirada antes de entrar | Nada |
| El carro avanza mucho más allá del obstáculo antes de volver | Es la pista del paso en paralelo (la cámara no ve lo que tiene al lado) | Dejar al menos 1.3 m de ruta después del obstáculo |

**Caja negra.** Cada ruta deja un archivo de texto en `Android/data/com.fsi.carro/files/cajas_negras/`. Su nombre aparece en Ejecutar ruta → Avanzado. Para sacarlos: `adb pull /sdcard/Android/data/com.fsi.carro/files/cajas_negras`. El formato de las líneas está en `docs/autonomo_v2_diseno.md`.

## 15.5 Autónomo v3: causa 19 y el freno de 150 mm

Rama `feat/autonomo-v3` (`SIMULADO`; la app solo se compiló).

| Síntoma | Qué significa | Qué hacer |
|---|---|---|
| «Causa 19: el láser dejó de responder a mitad de la ruta…» | Con la ruta en marcha, el láser vivía al empezar y lleva 600 ms sin vivir | Revisar la pantalla Sensores (láser caído, cable I²C). El carro se detuvo a propósito: no avanza sin el láser. Con el láser aún muerto, continuar vuelve a pedir ayuda |
| Con firmware `SEG=3` el carro se detiene a ~15 cm de una pared en vez de ~12 cm | `TOF_PARO_MM` pasó de 120 a 150 mm | Normal. `CAPS` trae `TOFP=150` |
| Con firmware `SEG=3` un avance contra algo a menos de 18 cm no arranca | `TOF_BLOQUEO_MM` es 180 mm | Alejar el carro o pasar a MANUAL |

# Anexos

Los anexos reúnen términos y datos de consulta para usar junto con los capítulos anteriores. Los valores configurados no sustituyen las mediciones físicas pendientes.

## Glosario

**ESP32:** familia de microcontroladores con conectividad inalámbrica. **I2C:** bus compartido para comunicar circuitos mediante líneas de datos y reloj. **PWM:** señal cuyo ciclo de encendido regula la potencia aplicada. **Pose:** posición y orientación estimadas del vehículo. **WebSocket:** canal de comunicación bidireccional persistente. **Watchdog:** mecanismo que vigila un plazo y ejecuta una acción si no se cumple la condición esperada. **ToF:** medición de distancia basada en el tiempo de vuelo de la luz.

## Comandos principales

`FORWARD`, `REVERSE`, `LEFT` y `RIGHT` solicitan movimiento. `STOP` y `EBRAKE` detienen. `PING` recibe `PONG` y no mantiene el movimiento. `MODO_AUTO` y `MODO_MANUAL` seleccionan perfiles. `GIRAR:<grados>` solicita un giro; `CAPS?`, `IMU?` y `TOF?` consultan sensores. `IMU_CAL` solicita calibración y `IMU_CERO` establece la referencia angular. La activación de la autoridad del láser se controla con `TOF_USO`.

## Pines de la DevKit

GPIO 27 y 26: canal izquierdo del driver. GPIO 25 y 33: canal derecho. GPIO 21: SDA del bus I2C. GPIO 22: SCL del bus I2C. GPIO 2: pulso configurado para mantenimiento de la powerbank, con conexión y eficacia físicas **NO MEDIDAS**.

## Fuentes por tema

Arquitectura y hardware: `conexiones.md`, `config.h`. Firmware y protocolo: `carro.c`, `sensores.c`, `protocolo.md`. Cámara: `camara_prueba.ino`, `camara_prueba.md`. Navegación: `navegador.c`, `calibracion.c`, `reporte_J15.md`. Percepción: `obstaculos.c`, `fusion.c`, `corredor.c`. Aplicación: `MainActivity.kt`, `ModoSemiautonomo.kt`. Resultados físicos: `reporte_J13_fisico.md`, `reporte_J14.md`.

## Pendientes

Validar físicamente el esquive y la reincorporación J15; medir el balance AUTO, el consumo eléctrico y el comportamiento de GPIO 2; comprobar la versión de firmware en la placa cuando se disponga del equipo.

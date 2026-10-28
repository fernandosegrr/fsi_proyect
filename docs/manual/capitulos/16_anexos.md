# Anexos

Los anexos reúnen términos y datos de consulta para usar junto con los capítulos anteriores. Los valores configurados no sustituyen las mediciones físicas pendientes.

## Glosario

**ESP32:** familia de microcontroladores con conectividad inalámbrica. **I2C:** bus compartido para comunicar circuitos mediante líneas de datos y reloj. **PWM:** señal cuyo ciclo de encendido regula la potencia aplicada. **Pose:** posición y orientación estimadas del vehículo. **WebSocket:** canal de comunicación bidireccional persistente. **Watchdog:** mecanismo que vigila un plazo y ejecuta una acción si no se cumple la condición esperada. **ToF:** medición de distancia basada en el tiempo de vuelo de la luz.

## Comandos principales

La Tabla 1 resume las órdenes principales. La activación de la autoridad del láser se controla con `TOF_USO`.

## Pines de la DevKit

La Tabla 2 reúne la asignación de los pines empleados por el controlador.

## Fuentes por tema

Arquitectura y hardware: `conexiones.md`, `config.h`. Firmware y protocolo: `carro.c`, `sensores.c`, `protocolo.md`. Cámara: `camara_prueba.ino`, `camara_prueba.md`. Navegación: `navegador.c`, `calibracion.c`, `reporte_J15.md`. Percepción: `obstaculos.c`, `fusion.c`, `corredor.c`. Aplicación: `MainActivity.kt`, `ModoSemiautonomo.kt`. Resultados físicos: `reporte_J13_fisico.md`, `reporte_J14.md`.

## Pendientes

Validar físicamente el esquive y la reincorporación J15; medir el balance AUTO, el consumo eléctrico y el comportamiento de GPIO 2; comprobar la versión de firmware en la placa cuando se disponga del equipo.

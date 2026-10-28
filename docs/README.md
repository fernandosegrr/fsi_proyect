# Documentación técnica del carro FSI

Bienvenido al centro de documentación del proyecto del carro con navegación asistida de la materia **Fundamentos de los Sistemas de Información (FSI)**, a cargo del profesor **Dr. Horacio Rostro González**.

---

## 1. Convenciones y honestidad técnica

Toda afirmación técnica en esta documentación distingue explícitamente su grado de certeza mediante las siguientes etiquetas normalizadas:

| Etiqueta | Significado y rigor |
|---|---|
| **VERIFICADO** | Comprobado directamente en el código fuente versionado o medido físicamente en el carro y registrado con evidencia en reportes de pruebas. |
| **SIMULADO** | Resultado obtenido mediante modelos matemáticos y simulaciones computacionales en PC; no constituye una medición en el vehículo físico real. |
| **HIPÓTESIS** | Explicación técnica plausible que carece de instrumentación o pruebas definitivas para confirmarse. |
| **NO MEDIDO** | Parámetro, tiempo o magnitud que no ha sido medido o cuyos registros físicos no se conservan. |
| **PENDIENTE** | Elemento pendiente de realización, fotografía o resolución por parte del equipo. |

---

## 2. Organización de la documentación

### 2.1 Documentos temáticos generales (vigentes y consolidados)

- [`index.md`](file:///e:/Dev/fsi_proyecto/docs/index.md): Índice temático de consulta rápida.
- [`arquitectura.md`](file:///e:/Dev/fsi_proyecto/docs/arquitectura.md): Visión global del sistema, diagramas de interacción y flujos de datos.
- [`protocolo.md`](file:///e:/Dev/fsi_proyecto/docs/protocolo.md): Especificación exhaustiva de comandos WebSocket, respuestas, mensajes de telemetría y códigos de error.
- [`operacion.md`](file:///e:/Dev/fsi_proyecto/docs/operacion.md): Guía operativa paso a paso: encendido, emparejamiento WiFi, modos de conducción y rutinas de prueba.
- [`calibracion_guia.md`](file:///e:/Dev/fsi_proyecto/docs/calibracion_guia.md): Procedimiento metrológico para calibrar avance en recta, balance de tracción y pivote angular con giroscopio.
- [`seguridad.md`](file:///e:/Dev/fsi_proyecto/docs/seguridad.md): Análisis de riesgos eléctricos (separación de tierras y potencia), mecánicos, térmicos y mitigaciones de software (watchdogs).
- [`resultados.md`](file:///e:/Dev/fsi_proyecto/docs/resultados.md): Compendio transparente de lo validado en hardware, lo comprobado en simulación y lo que permanece no medido.
- [`historia.md`](file:///e:/Dev/fsi_proyecto/docs/historia.md): Evolución cronológica del proyecto a lo largo de las jornadas de trabajo (J1 a J15).
- [`glosario.md`](file:///e:/Dev/fsi_proyecto/docs/glosario.md): Definición detallada de términos, siglas y unidades utilizadas.
- [`exposicion.md`](file:///e:/Dev/fsi_proyecto/docs/exposicion.md): Guion estructurado para la presentación oral y banco de preguntas frecuentes con respuestas técnicas sólidas.
- [`solucion_problemas.md`](file:///e:/Dev/fsi_proyecto/docs/solucion_problemas.md): Matriz de diagnóstico: síntomas, causas probables y acciones correctivas.
- [`mantenimiento.md`](file:///e:/Dev/fsi_proyecto/docs/mantenimiento.md): Cuidado de celdas de alimentación, motores TT, limpieza de óptica y conservación.

### 2.2 Referencias técnicas por área (`docs/referencia/`)

Contienen el desglose técnico en profundidad con referencias cruzadas a archivos y líneas de código:
1. [`hardware.md`](file:///e:/Dev/fsi_proyecto/docs/referencia/hardware.md): Chasis, microcontroladores, driver TC1508A, cableado y buses.
2. [`firmware.md`](file:///e:/Dev/fsi_proyecto/docs/referencia/firmware.md): Arquitectura del firmware del ESP32 DevKit, comandos y watchdog.
3. [`nucleo.md`](file:///e:/Dev/fsi_proyecto/docs/referencia/nucleo.md): Algoritmos en C del navegador, estimación de pose e integración angular.
4. [`app.md`](file:///e:/Dev/fsi_proyecto/docs/referencia/app.md): Estructura de la aplicación Android, enlace JNI y sesiones compartidas.
5. [`camara.md`](file:///e:/Dev/fsi_proyecto/docs/referencia/camara.md): Placa Freenove ESP32-WROVER, modos de streaming y compresión por software.
6. [`compilacion.md`](file:///e:/Dev/fsi_proyecto/docs/referencia/compilacion.md): Cadenas de compilación para ESP32, C nativo y Gradle Android.
7. [`pruebas.md`](file:///e:/Dev/fsi_proyecto/docs/referencia/pruebas.md): Resumen de los 2045 casos de prueba automatizados en PC.
8. [`simulador.md`](file:///e:/Dev/fsi_proyecto/docs/referencia/simulador.md): Modelado virtual cinemático, escenarios de ruido y validación montecarlo.

### 2.3 Recorridos de código fuente (`docs/codigo/`)

Análisis minucioso archivo por archivo con explicación pedagógica, estructuras y fragmentos comentados.

### 2.4 Figuras, imágenes y manual
- [`docs/figuras/`](file:///e:/Dev/fsi_proyecto/docs/figuras/README.md): Catálogo y scripts reproducibles de las 12 figuras técnicas en PNG y SVG.
- [`docs/imagenes/`](file:///e:/Dev/fsi_proyecto/docs/imagenes/README.md): Fotografías físicas y capturas reales del sistema.
- [`docs/manual/`](file:///e:/Dev/fsi_proyecto/docs/manual/): Script generador y documento PDF consolidado para entrega y exposición.

---

## 3. Documentos históricos del proyecto

Los archivos de registro previo existentes en `docs/` (como `bitacora.md`, `reporte_J13.md`, `reporte_J14.md`, `reporte_J15.md`, `conexiones.md`, `sensores.md`, `calibracion.md`, etc.) son **documentos históricos de trabajo**. Se mantienen intactos en el repositorio como memoria evolutiva. Para cualquier discrepancia entre un texto histórico y la implementación vigente, prevalece el código fuente y las anotaciones en [`pendientes_documentacion.md`](file:///e:/Dev/fsi_proyecto/docs/pendientes_documentacion.md).

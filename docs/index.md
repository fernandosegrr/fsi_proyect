# Índice general de documentación — Carro FSI

**Revisión pendiente del 11-nov-2026:** [esquive confirmado y resultados](diagnostico_esquive_20261111.md). Basada en v1.1; NO MEDIDA en el carro. Banquito: NO MEDIDA hasta repetir primero con caja de 20×20 cm. Esta nota es histórica: la v1.2 (base v3) la incluye y sigue pendiente de validar en el carro.

Este índice organiza de manera temático-secuencial todos los recursos documentales del proyecto para facilitar su lectura por estudiantes, evaluadores y desarrolladores.

---

## 0. Para exponer (v1.2)

- [Manual técnico en PDF (`manual/Manual_Proyecto_Carro_FSI.pdf`)](manual/Manual_Proyecto_Carro_FSI.pdf): el manual vigente (v1.2, base v3), con mapa de una página, glosario, índice «si te preguntan X, ve a la página Y» y los subsistemas A a I.
- [Guía de exposición (`guia_exposicion.md`)](guia_exposicion.md): guion, preguntas probables, quién explica qué (sin nombres), qué decir si la demostración falla y qué está medido y qué simulado.
- [Preguntas pendientes para el equipo (`pendientes_confirmar.md`)](pendientes_confirmar.md): todo lo marcado `CONFIRMAR CON EL EQUIPO` en el manual y la guía.

---
## 1. Visión general y fundamentos

- [Arquitectura del sistema (`arquitectura.md`)](arquitectura.md): Estructura en tres nodos (ESP32 DevKit, Freenove ESP32-WROVER y Teléfono Android), enlaces de comunicación y flujo de control.
- [Protocolo de comunicación (`protocolo.md`)](protocolo.md): Mensajes de texto WebSocket, respuestas, formato de telemetría `SENS` y parámetros de balance.
- [Historia y evolución del proyecto (`historia.md`)](historia.md): Crónica de las jornadas de trabajo (J1 a J15), decisiones técnicas clave y descarte de alternativas.
- [Glosario de términos y unidades (`glosario.md`)](glosario.md): Definición accesible de acrónimos, conceptos de robótica y magnitudes físicas del vehículo.

---

## 2. Puesta en marcha y operación práctica

- [Guía de operación y rutinas (`operacion.md`)](operacion.md): Procedimiento paso a paso para encender, enlazar WiFi, conducir en modo manual y ejecutar rutas asistidas.
- [Guía de calibración física (`calibracion_guia.md`)](calibracion_guia.md): Métodos para calibrar avance recto, balance de tracción izquierda/derecha y pivote angular con MPU6050.
- [Seguridad física y electrónica (`seguridad.md`)](seguridad.md): Manejo de alimentación separada (powerbank vs. pilas AA), protección de pines, watchdogs y parada ToF.
- [Solución de problemas (`solucion_problemas.md`)](solucion_problemas.md): Tabla de síntomas frecuentes, comprobaciones rápidas y soluciones de hardware/software.
- [Mantenimiento preventivo (`mantenimiento.md`)](mantenimiento.md): Conservación mecánica del chasis TT, cuidado de baterías y limpieza de sensores ópticos.

---

## 3. Validación, resultados y evaluación

- [Compendio de resultados (`resultados.md`)](resultados.md): Síntesis honesta clasificando lo medido en pista física, lo validado en simulación y lo que permanece no medido.
- [Guion de exposición y preguntas frecuentes (`exposicion.md`)](exposicion.md): Estructura sugerida para la presentación final del 1 de diciembre de 2026 y respuestas técnicas ante el sínodo.
- [Lista de fotos requeridas (`lista_de_fotos.md`)](lista_de_fotos.md): Catálogo de las 17 imágenes pendientes por capturar.
- [Pendientes y observaciones (`pendientes_documentacion.md`)](pendientes_documentacion.md): Registro de contradicciones históricas, dudas técnicas y puntos de gobernanza por definir.

---

## 4. Referencias técnicas por subsistema (`docs/referencia/`)

- [Hardware y conexiones (`hardware.md`)](referencia/hardware.md)
- [Firmware del microcontrolador (`firmware.md`)](referencia/firmware.md)
- [Núcleo de navegación C99 (`nucleo.md`)](referencia/nucleo.md)
- [Aplicación Android (`app.md`)](referencia/app.md)
- [Cámara y transmisión de video (`camara.md`)](referencia/camara.md)
- [Cadenas de compilación (`compilacion.md`)](referencia/compilacion.md)
- [Banco de pruebas automatizadas (`pruebas.md`)](referencia/pruebas.md)
- [Simulador y modelos virtuales (`simulador.md`)](referencia/simulador.md)

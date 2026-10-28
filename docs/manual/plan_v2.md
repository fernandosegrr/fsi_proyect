# Plan editorial del manual v2

## Ampliación solicitada

El lector espera un manual mucho más detallado, con una extensión cercana a 200 páginas. La ampliación debe aportar procedimientos, ejemplos explicados, decisiones, diagnóstico, interpretación de telemetría, calibración, pruebas y trazabilidad; no debe obtenerse repitiendo texto ni copiando documentos completos. La cifra es una meta editorial y no una autorización para inventar datos. Cada bloque ampliado debe contrastarse con el código o los reportes y distinguir configuración, resultado histórico, simulación y medición pendiente. El borrador previo de 22 páginas es un resumen insuficiente para esta nueva expectativa y no debe publicarse como entrega final.

Base: `origin/main` en c6ee161. Se redactará contenido propio por capítulo; las fuentes citadas son puntos de partida y sus afirmaciones se contrastarán con el código y los reportes antes de incorporarlas. Las cifras de simulación no se presentarán como mediciones físicas.

## Estructura y fuentes

1. **Introducción.** Presentar el propósito, alcance, lector y leyenda única de evidencia. Fuentes: `README.md`, `docs/entrega.md`, `docs/reporte_J15.md`.
2. **Arquitectura general.** Explicar las responsabilidades de los tres nodos, red y prioridad de parada. Fuentes: `docs/arquitectura.md`, `docs/referencia/firmware.md`, `firmware_carro/config.h`, `camara_prueba/camara_prueba.ino`.
3. **Hardware y conexiones.** Describir componentes, pines, sensores, montaje y alimentación. Fuentes: `docs/conexiones.md`, `docs/referencia/hardware.md`, `docs/sensores.md`, `firmware_carro/config.h`, fotos del repositorio y confirmación del dueño sobre dos powerbanks independientes.
4. **Firmware del carro.** Mostrar capas, control de motores, cola, perfiles, sensores y vigilancia. Fuentes: `docs/referencia/firmware.md`, `firmware_carro/carro.c`, `firmware_carro/firmware_carro.ino`, `firmware_carro/config.h`.
5. **Cámara Freenove.** Describir placa, red, modos de transmisión, recuperación y límites medidos. Fuentes: `docs/camara_prueba.md`, `docs/referencia/camara.md`, `camara_prueba/camara_prueba.ino`, `camara_prueba/recuperacion.c`.
6. **Protocolo de comunicación.** Explicar mensajes, respuestas, temporización y acciones de parada. Fuentes: `docs/protocolo.md`, `firmware_carro/carro.c`, `firmware_carro/sensores.c`, `app_android`.
7. **Núcleo de navegación en C.** Explicar rutas, calibración, giros, pose, esquive, reincorporación y simulación. Fuentes: `docs/navegador.md`, `docs/calibracion.md`, `docs/reporte_J15.md`, `nucleo_c/navegador.c`, `nucleo_c/trayectoria.c`, `nucleo_c/simulador_navegador.c`.
8. **Visión y percepción.** Explicar color, modelo, láser, fusión y corredor con sus límites. Fuentes: `docs/obstaculos.md`, `docs/fusion.md`, `docs/modelo_objetos.md`, `docs/vision_huawei.md`, `nucleo_c/obstaculos.c`, `nucleo_c/fusion.c`, `nucleo_c/corredor.c`.
9. **Aplicación Android.** Describir pantallas, conexión, video, control y ejecución de rutas. Fuentes: `docs/referencia/app.md`, `docs/operacion.md`, `app_android`.
10. **Compilación, instalación y versión.** Explicar requisitos, comandos y verificación con distinción entre versiones históricas y estado actual. Fuentes: `docs/referencia/compilacion.md`, `docs/entrega.md`, `scripts/cargar.ps1`, `scripts/verificar_version.py`, `app_android/app/build.gradle.kts`.
11. **Operación y seguridad física.** Presentar comprobaciones, secuencia de encendido, uso y parada. Fuentes: `docs/operacion.md`, `docs/seguridad.md`, `docs/conexiones.md`, `docs/guia_conexion_J15.md`.
12. **Pruebas y resultados.** Separar evidencia en carro, pruebas de software y simulación, indicando condiciones y ausencia de validación J15. Fuentes: `docs/reporte_J13_fisico.md`, `docs/reporte_J14.md`, `docs/reporte_J15.md`, `docs/camara_prueba.md`.
13. **Límites, riesgos y trabajo futuro.** Precisar restricciones y pendientes sin convertir hipótesis en mediciones. Fuentes: `docs/limites_conocidos.md`, `docs/pendientes_firmware.md`, `docs/reporte_J15.md`.
14. **Proceso de desarrollo.** Resumir etapas y decisiones descartadas con respaldo documental, en dos o tres páginas. Fuentes: `docs/bitacora.md`, `docs/progreso.md`, `docs/reporte_J14.md`, `docs/reporte_J15.md`.
15. **Solución de problemas.** Asociar síntomas con comprobaciones concretas y límites de diagnóstico. Fuentes: `docs/conexiones.md`, `docs/sensores.md`, `docs/operacion.md`, `docs/instalar_app_huawei.md`.
16. **Anexos.** Reunir glosario, comandos y pines, correspondencia entre capítulos y archivos, y pendientes. Fuentes: `docs/protocolo.md`, `firmware_carro/config.h`, `docs/pendientes_documentacion.md` y las fuentes citadas por capítulo.

## Exclusiones y transformaciones

- El guion y banco para el sínodo permanecen en `docs/exposicion.md`: son material personal de estudio.
- No incorporar portadas, metadatos de rama, historial de agentes, instrucciones del encargo ni notas de cierre de documentos fuente.
- Mover la trazabilidad detallada a los anexos y la auditoría; conservar solo nombres de archivo al final de cada capítulo.
- Reemplazar ASCII, diagramas con fuente incrustada y fórmulas sin interpretar. Numerar por orden de aparición, no por el nombre de archivos anteriores.
- Agrupar las cuatro fotografías del carro y las capturas útiles; explicar sus estados de conexión y no alterar las imágenes salvo escala.
- Verificar institución y cifras antes de publicarlas; omitir lo no respaldado o marcarlo pendiente. El dueño confirmó una powerbank para la cámara y otra para la DevKit; no inferir una conexión eléctrica entre sus tierras.

## Figuras previstas

1. Tres nodos: teléfono, DevKit y cámara, enlaces y alimentación de cada placa.
2. Alimentación: dos powerbanks y portapilas de motores; tierra común únicamente donde la documentación la respalda.
3. Capas de seguridad: operador, app, vigilancia y parada por láser, con condiciones de aplicación.
4. Flujo de comandos: app, WebSocket, cola, lógica y motores; retorno de telemetría.
5. Lámina de cuatro fotografías del carro con identificación de vistas.
6. Ciclo de navegación: ruta, avance por tiempo, giro con giroscopio y actualización de pose estimada.
7. Esquive y reincorporación: trayectoria conceptual, obstáculo y retorno; validación física J15 no medida.
8. Percepción: color y modelo, fusión, corredor, láser y decisión; distinguir las rutas reales de datos al verificar el código.
9. Mapa de pantallas Android y recursos compartidos.
10. Lámina de capturas de la app con los estados de conexión explicados.
11. Línea de tiempo de etapas y decisiones respaldadas por la bitácora.

La numeración definitiva se fijará al ordenar el contenido. Se utilizará ReportLab si el entorno local permite reproducir el diseño y la tabla de contenido en dos pasadas; la elección final se comprobará durante la implementación.

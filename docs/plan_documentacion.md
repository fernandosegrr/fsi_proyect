# Plan de documentación del carro FSI

**Versión:** 1.0 · **Fecha:** 27 de octubre de 2026 · **Rama:** `docs/documentacion-completa`

Este plan ordena el trabajo documental del proyecto. No cambia firmware, núcleo, app,
pruebas ni scripts: todo lo nuevo vive en `README.md`, `docs/` y los README de carpeta.
La meta es que una persona que nunca vio el carro pueda entenderlo, armarlo, operarlo,
diagnosticarlo y exponerlo el 1 de diciembre de 2026 sin depender de la memoria del equipo.

## 1. Alcance y reglas

### 1.1 Qué se documenta

- El carro físico: ESP32 DevKit, driver TC1508A, cuatro motores TT, MPU6050,
  VL53L0X, powerbank y alimentación.
- El firmware del carro (`firmware_carro/`) y el de la cámara (`camara_prueba/`).
- El núcleo portátil en C (`nucleo_c/`), la app Android (`app_android/`) y el puente JNI.
- Las pruebas de PC, el servidor falso, la cámara falsa y los scripts de apoyo.
- El estado real de validación: qué se midió, qué solo se simuló y qué sigue pendiente.

### 1.2 Qué no se hace

- No se corrige código, firmware, pruebas ni scripts. Los defectos y comentarios
  faltantes se anotan en `docs/pendientes_documentacion.md` con archivo y línea.
- No se opera el carro, el teléfono, ADB, WiFi ni el APK, salvo capturas inertes
  autorizadas. No se ejecutan rutas ni movimientos.
- No se lee, copia ni publica `secretos.h`, contraseñas, PIN, tokens, correos ni
  nombres de red reales. El repositorio es público.
- No se publica el binario del firmware ni nada mayor de 15,000,000 bytes.
- No se hace merge a `main` ni se toca `original_backup/` o la copia de OneDrive.

### 1.3 Cómo se distingue lo que sabemos

Cada afirmación técnica lleva una de estas etiquetas cuando no es obvia:

| Etiqueta | Significado |
|---|---|
| **VERIFICADO** | Leído en el código o medido y registrado en un documento del repositorio. |
| **SIMULADO** | Resultado de un modelo en PC; no es una medición del carro físico. |
| **HIPÓTESIS** | Explicación plausible sin evidencia suficiente; se dice qué falta para comprobarla. |
| **NO MEDIDO** | Nadie lo ha medido o la medición no se conserva. |
| **PENDIENTE** | Falta hacerlo, medirlo, fotografiarlo o decidirlo. |

Toda referencia técnica debe poder rastrearse a un archivo, una función o un commit.
Las afirmaciones históricas se apoyan en `docs/reporte_J13*.md`, `docs/reporte_J14.md`
y `docs/reporte_J15.md`; no se presentan como mediciones nuevas.

## 2. Público y tono

- **Público principal:** el equipo que expone y el dueño que seguirá probando el carro.
- **Público secundario:** un compañero o profesor que quiera entender o replicar el sistema.
- **Tono:** claro, directo, en español; sin siglas sin explicar; sin promesas que el
  repositorio no respalde.
- **Tablas:** solo cuando comparan datos, pines, comandos o estados. Para explicar,
  se prefiere prosa y listas.
- **Nombre:** el proyecto se llama **carro FSI** o **el carro**. No se usa WALL-E.

## 3. Estructura final

```
README.md                      Presentación, estado, arranque rápido e índice
firmware_carro/README.md       Firmware del carro
camara_prueba/README.md        Firmware de la cámara (el encargo lo llama firmware_camara)
nucleo_c/README.md             Núcleo de navegación en C
app_android/README.md          Aplicación Android
pruebas_firmware/README.md     Pruebas de PC y servidor falso
dist/README.md                 Paquete de entrega y verificación
docs/README.md                 Índice y convenciones de la documentación
docs/plan_documentacion.md     Este plan
docs/arquitectura.md           Vista general y flujos
docs/protocolo.md              Protocolo completo, comandos y respuestas
docs/operacion.md              Puesta en marcha, modos y rutinas
docs/calibracion_guia.md       Calibración paso a paso (guía nueva, no el histórico)
docs/seguridad.md              Riesgos eléctricos, mecánicos y de operación
docs/resultados.md             Resultados verificados, simulados y NO MEDIDOS
docs/historia.md               Línea de tiempo, nombres y versiones
docs/glosario.md               Términos, siglas y unidades
docs/exposicion.md             Guion de la exposición y preguntas frecuentes
docs/solucion_problemas.md     Síntomas, causas y comprobaciones
docs/mantenimiento.md          Batería, limpieza, almacenamiento y vida útil
docs/lista_de_fotos.md         Fotos y capturas necesarias, con estado
docs/pendientes_documentacion.md  Defectos y huecos detectados, con archivo y línea
docs/referencia/               Detalle técnico por área
docs/codigo/                   Recorridos de código por módulo
docs/figuras/                  Fuentes y PNG/SVG de las figuras
docs/imagenes/                 Fotos reales (hoy vacío; se incluyen solas en el PDF)
docs/manual/                   Script reproducible y PDF del manual
```

Los documentos existentes (`docs/navegador.md`, `docs/sensores.md`,
`docs/calibracion.md`, `docs/reporte_*.md`, etc.) se conservan como **documentos de
trabajo o históricos**. No se reescriben: la documentación nueva los resume, los
verifica y remite a ellos. Cuando un documento histórico contradice al código, se
anota en `docs/pendientes_documentacion.md`.

## 4. Fases

### Fase 0 — Plan y estructura (este documento)

- Definir estructura, públicos, Convenciones y índice del manual.
- Crear las carpetas `docs/referencia`, `docs/codigo`, `docs/figuras`, `docs/imagenes`,
  `docs/manual`.
- Criterio de cierre: árbol aprobado y sin cambios de código.

### Fase 1 — Referencia técnica

Ocho documentos, cada uno con fuentes y límites:

1. `hardware.md` — pines, alimentación, I2C, cableado, riesgos.
2. `firmware.md` — capas, comandos, watchdog, perfiles, sensores, versión.
3. `nucleo.md` — módulos, API, estados, pose, calibración, parámetros.
4. `app.md` — pantallas, sesión, JNI, detector, cámara, builds.
5. `camara.md` — modos, HTTP, UDP, recuperación, rendimiento.
6. `compilacion.md` — toolchains, comandos, variables, verificación.
7. `pruebas.md` — programas, casos, qué cubren y qué no.
8. `simulador.md` — modelo, escenarios, límites y reproducibilidad.

Criterio de cierre: cada afirmación tiene archivo/función; los números dudosos quedan
como NO MEDIDOS.

### Fase 2 — Figuras e imágenes

- Generar 10–12 figuras reproducibles con `docs/figuras/generar_figuras.py`
  (PNG + SVG) y guardar la fuente.
- Crear `docs/figuras/README.md` con número, título, fuente, imagen y uso.
- Crear `docs/imagenes/README.md` y `docs/lista_de_fotos.md`.
- Criterio de cierre: cada figura se abre, se lee y coincide con el texto.

### Fase 3 — README y documentos temáticos

- Reescribir el README principal y crear los README de carpeta.
- Escribir arquitectura, protocolo, operación, calibración, seguridad, resultados,
  historia, glosario, exposición, solución de problemas y mantenimiento.
- Criterio de cierre: un lector nuevo puede pasar de “no sé qué es” a “puedo
  conectarlo y probarlo” sin abrir el código.

### Fase 4 — Recorridos de código

- `docs/codigo/firmware_carro.md`, `nucleo_c.md`, `app_android.md`,
  `camara_prueba.md`, `pruebas_firmware.md`, `scripts.md`.
- Cada uno: mapa de archivos, funciones clave, flujo, puntos delicados y pruebas.
- Criterio de cierre: los nombres y líneas citados existen en el commit documentado.

### Fase 5 — Manual PDF

- `docs/manual/generar_manual.py`: lee el manifiesto de capítulos, numera figuras,
  crea portada, índice paginado, encabezados, pies, enlaces internos y galería
  automática de `docs/imagenes/`.
- Salida: `docs/manual/Manual_Proyecto_Carro_FSI.pdf`.
- Revisión visual página por página con renders PNG y corrección de viudas,
  huérfanas, tablas cortadas, imágenes gigantes y enlaces rotos.
- Criterio de cierre: PDF < 15 MB, índice con páginas correctas, todas las figuras
  referenciadas y ninguna página ilegible.

### Fase 6 — Auditoría y cierre

- Auditar al menos 40 afirmaciones contra código o ejecución.
- Revisar contradicciones con J15, enlaces, ortografía, secretos y `git diff --stat`.
- Confirmar que `main` no cambió.
- Commits pequeños con identidad noreply, push solo a `docs/documentacion-completa`.
- Criterio de cierre: informe final con rama, último commit, cómo hacer merge,
  índice del manual y contenido de `docs/lista_de_fotos.md`.

## 5. Índice previsto del manual

1. Portada
2. Cómo leer este manual
3. El proyecto en una página
4. Arquitectura del sistema
5. Hardware y cableado
6. Firmware del carro
7. Protocolo de comunicación
8. Núcleo de navegación en C
9. Aplicación Android
10. Cámara ESP32-CAM
11. Puesta en marcha
12. Operación y modos
13. Calibración
14. Pruebas y calidad
15. Resultados y estado de validación
16. Límites, riesgos y seguridad
17. Solución de problemas
18. Mantenimiento
19. Glosario
20. Anexo A: mapa de archivos
21. Anexo B: comandos y tablas
22. Anexo C: pendientes y fotos

El índice del PDF se genera solo, con números de página reales; este listado es la
intención editorial, no el índice final.

## 6. Convenciones de figuras

- Nombre: `fig_NN_slug.png` y `fig_NN_slug.svg`.
- Fuente: `docs/figuras/generar_figuras.py` y una ficha en `docs/figuras/README.md`.
- En el texto: `Figura N. Título.` y referencia `(figura N)`.
- Las fotos reales van en `docs/imagenes/` con nombre `foto_NN_slug.jpg`; el script
  del manual las detecta e incluye sin tocar la lista de capítulos.
- Ninguna figura se inventa: si un dato no está verificado, se dibuja como esquema
  y se marca “esquema, no a escala” o “valor NO MEDIDO”.

## 7. Convenciones de código citado

- Ruta relativa a la raíz y, cuando importa, número de línea: `nucleo_c/navegador.c:348`.
- Los extractos se copian del commit documentado y se marcan con lenguaje.
- No se corrigen defectos en los extractos; si el código tiene un problema, se cita
  tal cual y se remite a `docs/pendientes_documentacion.md`.
- Los comandos se prueban solo si son de lectura o pruebas de PC; se anota fecha,
  comando y resultado.

## 8. Riesgos del trabajo documental

| Riesgo | Mitigación |
|---|---|
| Convertir hipótesis en hechos | Etiquetas y revisión cruzada con código. |
| Repetir cifras históricas sin contexto | Citar el reporte y la fecha; decir si fue simulación. |
| Publicar un secreto | Escaneo previo a cada commit; no leer `secretos.h`. |
| PDF gigante | Imágenes a 150–200 dpi, JPEG optimizado, límite 15 MB. |
| Manual desactualizado | El manual se genera de los mismos Markdown; no se edita a mano. |
| Perder el hilo por documentos históricos | `docs/README.md` marca cuáles son históricos y cuál es la fuente vigente. |

## 9. Criterios de aceptación

- [ ] README principal y un README corto en cada carpeta entregable.
- [x] Ocho documentos de referencia con fuentes.
- [ ] Seis recorridos de código con archivos y funciones reales.
- [ ] 10–12 figuras con fuente e imagen, revisadas visualmente.
- [ ] `docs/lista_de_fotos.md` con estado de cada foto.
- [ ] `docs/pendientes_documentacion.md` con archivo y línea, sin corregir nada.
- [ ] Manual PDF con portada, índice paginado, figuras numeradas, código y enlaces.
- [ ] Script reproducible que incluye solo las fotos nuevas de `docs/imagenes/`.
- [ ] ≥ 40 afirmaciones auditadas y contradicciones con J15 resueltas o anotadas.
- [ ] `main` sin cambios; push solo a `docs/documentacion-completa`.
- [ ] Informe final con rama, commit, merge, índice y lista de fotos.

## 10. Registro de avance

| Fase | Estado | Evidencia |
|---|---|---|
| 0 Plan | Completada | Este documento |
| 1 Referencia | Completada | Ocho documentos en `docs/referencia/` |
| 2 Figuras | Pendiente | `docs/figuras/` |
| 3 README y temas | Pendiente | README y `docs/` |
| 4 Código | Pendiente | `docs/codigo/` |
| 5 Manual | Pendiente | PDF y script |
| 6 Auditoría | Pendiente | Informe final |

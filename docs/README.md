# Centro de Documentación Técnica — Proyecto Carro FSI (v1.2.5 — autónomo v3 con correcciones del 26-nov; validación física parcial; arranque con sombras por láser (probado en el carro, 3 corridas); demostración con pilas nuevas sin frenado validado a esa velocidad)

**Nota histórica (11-nov-2026):** [esquive confirmado y resultados](diagnostico_esquive_20261111.md) (basada en v1.1; banquito sin medir en el carro). La versión vigente **v1.2.5** incluye las pruebas físicas del 25–26 nov-2026 (validación física **PARCIAL**; ver [correcciones_fisicas_20261126.md](correcciones_fisicas_20261126.md)), el arreglo de arranque con sombras por láser (`aprenderPiso`, umbral `600 mm`, probado en el carro el 27-nov en 3 corridas de C2 más 2 intentos de C1) y la decisión de exposición **Opción 2** (pilas AA nuevas y calibración nueva a 46–50 cm/s, sin frenado aislado validado a esa velocidad).

Bienvenido al centro de documentación del proyecto **LIDIA (*Carro FSI*)** de la materia **Fundamentos de los Sistemas de Información (FSI)** (Universidad de Guanajuato · DICIS, profesor **Dr. Horacio Rostro González**).

---

## 1. Manual Técnico en PDF (v1.2.5)

- **[Manual_Proyecto_Carro_FSI.pdf](manual/Manual_Proyecto_Carro_FSI.pdf)** (`142` páginas, `12` figuras, `65` tablas, índice general interactivo, mapa de una página, glosario y «si te preguntan X, ve a la página Y»): v1.2.5 — autónomo v3 con correcciones del 26-nov; validación física parcial; arranque con sombras por láser (probado en el carro, 3 corridas); demostración con pilas nuevas sin frenado validado a esa velocidad.
- **[guia_exposicion.md](guia_exposicion.md):** guion de 3 a 5 minutos, reparto de 10 diapositivas, preguntas probables agrupadas por integrante, qué decir si la demostración falla y qué está medido y qué simulado.
- **[pendientes_confirmar.md](pendientes_confirmar.md):** preguntas `CONFIRMAR CON EL EQUIPO` que hay que contestar antes de exponer.
- **[docs/manual/README.md](manual/README.md):** instrucciones para regenerar el PDF con `python docs/manual/generar_manual.py`, diagnóstico de la versión previa ([manual/diagnostico_v1.md](manual/diagnostico_v1.md)), plan editorial ([manual/plan_final.md](manual/plan_final.md)) y auditoría de afirmaciones ([manual/auditoria_afirmaciones.md](manual/auditoria_afirmaciones.md)).
- **[docs/figuras/README.md](figuras/README.md):** catálogo y generador reproducible (`docs/figuras/generar_figuras_v2.py`) de los diagramas y láminas del manual.

---

## 2. Convención de evidencia y documentos de cierre J16

Toda afirmación técnica distingue explícitamente su grado de evidencia:

| Etiqueta | Significado técnico |
|---|---|
| **`VALIDADO EN EL CARRO` / `VERIFICADO`** | Comprobado físicamente en el carro (en rutas del cierre J16, **únicamente el esquive con reincorporación B6**: 6 corridas el 10-nov-2026, `0/6` toques, `5/6` autónomas completas con error lateral aproximado de `~0` a `~10 cm` y `1/6` en `AYUDA` sin causa identificada) o verificado en las suites automatizadas de PC (`2,267` casos con `0` fallos). |
| **`SIMULADO`** | Resultado obtenido en el simulador físico-matemático (`simulador_navegador.c`, `50,000` corridas Monte Carlo con semillas fijas `160000 + índice del escenario × 10007 + corrida`, ver `nucleo_c/pruebas_precision_j16.c`). No sustituye mediciones en el carro físico. |
| **`NO MEDIDO`** | Implementado y probado en software/simulación, pero sin corridas físicas en el carro (rectas con y sin `RUMBO`, ruta en L, cuadrado, corte de WiFi en marcha, freno láser aislado, caja ancha, calibración en piso de `IMU_ESCALA`, `GPIO 2` y consumo en amperios). |
| **`PENDIENTE`** | Trabajo futuro identificado (odometría por codificadores, control de 4 canales independientes, bitácora `.jsonl` por corrida). |

### Registros maestros de estado y pruebas (Versión final 1.1)
- [estado_20261110.md](estado_20261110.md): estado integral de validación al 10 de noviembre de 2026 (`FW:6d1b213-20261110` y `APP:0.4-6d1b213`).
- [pruebas_fisicas_20261110.csv](pruebas_fisicas_20261110.csv): registro de las 6 corridas físicas del escenario B6 en el carro.
- [limites_conocidos.md](limites_conocidos.md): límites físicos, geométricos, visuales y de protocolo vigentes.
- [entrega.md](entrega.md): guía de verificación de artefactos de `dist/`, hashes SHA-256 e instalación.
- [reporte_J16.md](reporte_J16.md) y [precision/monte_carlo_1000.txt](precision/monte_carlo_1000.txt): detalle matemático del cierre J16 y tabla de `50,000` corridas Monte Carlo.

---

## 3. Documentos temáticos y referencias por área

### 3.1 Guías temáticas generales
- [index.md](index.md): índice temático general.
- [arquitectura.md](arquitectura.md): topología de tres nodos (Android, `ESP32 DevKit`, `Freenove`) y tres fuentes de energía independientes.
- [protocolo.md](protocolo.md): catálogo de comandos sobre `WebSocket /ws`, puerto serie y tramas `SENS` / `CAPS`.
- [operacion.md](operacion.md) y [seguridad.md](seguridad.md): secuencia segura de encendido, conducción y cuatro capas de protección.
- [calibracion_guia.md](calibracion_guia.md): calibración longitudinal, angular y factor de escala del giroscopio en ppm (`IMU_ESCALA`).
- [resultados.md](resultados.md): compendio de resultados físicos, suites en PC y simulaciones.
- [historia.md](historia.md): evolución técnica y justificación de diseño.
- [solucion_problemas.md](solucion_problemas.md) y [mantenimiento.md](mantenimiento.md): diagnóstico por síntoma y cuidado de baterías/componentes.
- [glosario.md](glosario.md): definiciones técnicas.
- [exposicion.md](exposicion.md): material de apoyo para presentación oral (separado del manual técnico entregable).

### 3.2 Monografías de referencia (`docs/referencia/`) y recorridos de código (`docs/codigo/`)
- [referencia/hardware.md](referencia/hardware.md), [referencia/firmware.md](referencia/firmware.md), [referencia/nucleo.md](referencia/nucleo.md), [referencia/app.md](referencia/app.md), [referencia/camara.md](referencia/camara.md), [referencia/compilacion.md](referencia/compilacion.md), [referencia/pruebas.md](referencia/pruebas.md) y [referencia/simulador.md](referencia/simulador.md).
- Recorridos archivo por archivo en `docs/codigo/`.

# Centro de Documentación Técnica — Proyecto Carro FSI (LIDIA · Versión final 1.1)

Bienvenido al centro de documentación del proyecto **LIDIA (*Carro FSI*)** de la materia **Fundamentos de los Sistemas de Información (FSI)** (Universidad de Guanajuato · DICIS, profesor **Dr. Horacio Rostro González**).

---

## 1. Entregable principal: Manual Técnico en PDF (Versión final 1.1)

- **[Manual_Proyecto_Carro_FSI.pdf](manual/Manual_Proyecto_Carro_FSI.pdf)** (`51` páginas, `12` figuras, `23` tablas, índice general interactivo): documento principal autocontenido y reescrito para la **Versión final 1.1** (cierre J16).
- **[docs/manual/README.md](manual/README.md):** instrucciones para regenerar el PDF con `python docs/manual/generar_manual.py`, diagnóstico de la versión previa ([manual/diagnostico_v1.md](manual/diagnostico_v1.md)), plan editorial ([manual/plan_final.md](manual/plan_final.md)) y auditoría de afirmaciones ([manual/auditoria_afirmaciones.md](manual/auditoria_afirmaciones.md)).
- **[docs/figuras/README.md](figuras/README.md):** catálogo y generador reproducible (`docs/figuras/generar_figuras_v2.py`) de los diagramas y láminas del manual.

---

## 2. Convención de evidencia y documentos de cierre J16

Toda afirmación técnica distingue explícitamente su grado de evidencia:

| Etiqueta | Significado técnico |
|---|---|
| **`VALIDADO EN EL CARRO` / `VERIFICADO`** | Comprobado físicamente en el carro (en rutas del cierre J16, **únicamente el esquive con reincorporación B6**: 6 corridas el 10-nov-2026, `0/6` toques, `5/6` autónomas completas con error lateral aproximado de `~0` a `~10 cm` y `1/6` en `AYUDA` sin causa identificada) o verificado en las suites automatizadas de PC (`2,267` casos con `0` fallos). |
| **`SIMULADO`** | Resultado obtenido en el simulador físico-matemático (`simulador_navegador.c`, `50,000` corridas Monte Carlo con semilla fija `0xF51A2026`). No sustituye mediciones en el carro físico. |
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

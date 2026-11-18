# Construcción del Manual Técnico (base v1.1; revisión pendiente)

**Revisión pendiente del 12-nov-2026:** [esquive confirmado y resultados](../diagnostico_esquive_20261111.md). Basada en v1.1; NO MEDIDA en el carro. Banquito: NO MEDIDA hasta repetir primero con caja de 20×20 cm. No constituye una publicación v1.2.

El manual técnico del proyecto **LIDIA (*Carro FSI*)** se redacta de forma modular en `docs/manual/capitulos/` (`01_introduccion.md` a `16_anexos.md`) y se compila de manera reproducible en un solo comando mediante `docs/manual/generar_manual.py`.

## Cadena de herramientas local

- **ReportLab (`SimpleDocTemplate` + `TableOfContents` en dos pasadas):** maquetación tipográfica con fuentes `DejaVu Sans` y `DejaVu Sans Mono` incrustadas, encabezados jerárquicos, tablas con repetición de cabecera, bloques de fórmulas y marcadores interactivos del PDF (*bookmarks*).
- **Matplotlib (`docs/figuras/generar_figuras_v2.py`):** generación programática de los 10 diagramas vectoriales y las 2 láminas fotográficas compuestas en `docs/figuras/v2/` a `220 dpi`.
- **PyMuPDF (`pymupdf`):** limpieza de recursos tipográficos no utilizados (`/F1 Helvetica`) y verificación de metadatos y fuentes incrustadas.

## Cómo regenerar el PDF completo

Con el entorno de Python del repositorio, ejecutar desde la raíz:

```powershell
python docs/manual/generar_manual.py
```

El script ejecuta automáticamente `docs/figuras/generar_figuras.py` antes de compilar el documento y produce `docs/manual/Manual_Proyecto_Carro_FSI.pdf`.

## Documentos de soporte en esta carpeta

- [Manual_Proyecto_Carro_FSI.pdf](Manual_Proyecto_Carro_FSI.pdf): PDF con la revisión de autonomía del 12-nov sobre la base v1.1.
- [diagnostico_v1.md](diagnostico_v1.md): diagnóstico visual página por página del borrador previo a la reescritura.
- [plan_final.md](plan_final.md): estructura de 16 capítulos, fuentes verificadas y catálogo de figuras y tablas de la versión 1.1.
- [auditoria_afirmaciones.md](auditoria_afirmaciones.md): trazabilidad de las afirmaciones técnicas, cifras y etiquetas de evidencia (`VALIDADO EN EL CARRO`, `SIMULADO`, `NO MEDIDO`, `PENDIENTE`).

Para una revisión solo de texto, `python docs/manual/generar_manual.py --sin-figuras` reutiliza las figuras existentes. La portada identifica la revisión pendiente sobre v1.1; los datos físicos del 10-nov son históricos.

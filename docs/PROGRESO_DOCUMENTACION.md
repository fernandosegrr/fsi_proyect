# Progreso de la documentación completa del carro FSI

**Materia:** Fundamentos de los Sistemas de Información (FSI)  
**Profesor:** Dr. Horacio Rostro González  
**Fecha de entrega:** 1 de diciembre de 2026  
**Rama de trabajo:** `docs/documentacion-completa`  
**Última actualización:** 27 de octubre de 2026  

---

## 1. Estado por fases

| Fase | Descripción | Estado | Detalle y evidencia |
|---|---|---|---|
| **Paso 0** | Retomar con verificación | **Completado** | Verificadas >15 afirmaciones contra el código. Pruebas de núcleo (21 suites, 1598 casos) y firmware (447 casos) ejecutadas con 0 fallos. |
| **Fase 0** | Plan y estructura | **Completado** | Documento `docs/plan_documentacion.md` versionado en commit `3f3cb02`. |
| **Fase 1** | Ocho referencias técnicas | **Completado** | En `docs/referencia/` (`hardware.md`, `firmware.md`, `nucleo.md`, `app.md`, `camara.md`, `compilacion.md`, `pruebas.md`, `simulador.md`), commit `3f3cb02`. |
| **Fase 2** | Figuras del manual | **Completado** | 12 figuras generadas en PNG (170 dpi) y SVG con `docs/figuras/generar_figuras.py`. Catálogo en `catalogo.py` y ficha en `docs/figuras/README.md`. |
| **Fase 3** | Imágenes y lista de fotos | **Completado** | Creados `docs/imagenes/README.md`, `docs/lista_de_fotos.md` (17 fotos/capturas catalogadas) y `docs/pendientes_documentacion.md`. |
| **Fase 4** | Documentación en el repo | **Completado** | Creados README principal, READMEs por carpeta (7), documentos temáticos (11) y recorridos de código en `docs/codigo/` (10). Script de validación de enlaces en `docs/verificar_enlaces.py`. |
| **Fase 5** | Manual en PDF | **Completado** | Generado `docs/manual/Manual_Proyecto_Carro_FSI.pdf` (76 páginas, 1.77 MB < 15 MB) mediante script reproducible `generar_manual.py` con dos pasadas, índice paginado, 12 figuras vectoriales y recuadros de fotos pendientes. |
| **Fase 6** | Auditoría y cierre | **Completado** | 42 afirmaciones técnicas auditadas (100% de éxito), cero enlaces rotos, escaneo de secretos sin incidencias y verificación de diff estricta de solo documentación. |

---

## 2. Historial de commits en esta rama

1. `deae8b6` — (base) *J15 fase 6: entregar reporte y dejar validacion fisica pendiente*
2. `3f3cb02` — *docs: agregar referencia tecnica de las ocho areas*
3. *Siguiente:* commit de Fase 2 (figuras y catálogo).

---

## 3. Próximos pasos inmediatos

1. Fase 2: Hacer commit de `docs/figuras/` y hacer push inicial a `origin docs/documentacion-completa`.
2. Fase 3: Crear `docs/imagenes/README.md`, `docs/lista_de_fotos.md` y `docs/pendientes_documentacion.md`.
3. Fase 4: Redactar los READMEs de cada módulo, README principal, documentos temáticos en `docs/` y recorridos de código en `docs/codigo/`.

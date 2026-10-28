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
| **Fase 3** | Imágenes y lista de fotos | **En progreso** | `docs/imagenes/README.md` y `docs/lista_de_fotos.md`. |
| **Fase 4** | Documentación en el repo | **Pendiente** | README principal, READMEs por carpeta, docs temáticos y recorridos de código en `docs/codigo/`. |
| **Fase 5** | Manual en PDF | **Pendiente** | Script generador `docs/manual/generar_manual.py` y PDF final verificado visualmente (<15 MB). |
| **Fase 6** | Auditoría y cierre | **Pendiente** | Verificación de ≥40 afirmaciones técnicas, escaneo de secretos y preparación para entrega. |

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

# Progreso del Manual Final v1.1 y Cierre J16

## Estado actual
- **Fase 1 (Estado y verificación):** COMPLETADA.
  - Worktree: `E:\Dev\fsi_proyecto`.
  - `v1.0` confirmado en `eb88151` (`eb88151f438048190c26c9ed9ba26fbb6ad9424e`).
  - `main` local (`c6ee161`) estaba 9 commits detrás de `origin/main` (`5d5c628`); actualizado por fast-forward a `5d5c628`.
  - `j16-precision` en `6d1b213` (sincronizada con `origin/j16-precision`).
  - Pruebas en `j16-precision` todas en verde:
    - `make -C nucleo_c test`: 22 programas, 1,655 casos, 0 fallos.
    - `make -C pruebas_firmware test`: 5 programas, 540 casos, 0 fallos + `verificar_pagina.py` (3 comprobaciones OK).
    - `python scripts/probar_version.py`: 18 pruebas, 0 fallos.
    - `python pruebas_firmware/probar_servidor_falso.py`: 54 casos, 0 fallos.
    - `python docs/verificar_enlaces.py`: 133 archivos Markdown, 73 enlaces, 0 rotos.
- **Fase 2 (APK y registros en `j16-precision`):** COMPLETADA (commit `63bf0f9`).
  - `dist/carro_fsi_0.4-6d1b213.apk` copiada (`14,881,402` bytes, SHA-256 `C5B36FEC74BFEDE9798B4BE7C992D7FEA8369F456DA4D4A10D61A4BBB25ED02F`); `dist/carro_fsi_0.4-873bfb9.apk` retirada con `git rm`.
  - `dist/SHA256SUMS.txt`, `dist/LEEME.txt`, `dist/FIRMWARE_LOCAL.txt` (`FW:6d1b213-20261110`, SHA-256 `71D671A4E64E0F234015E8685D4E741B87471534E3E50869EE023C5A657B79FC`), `dist/README.md`, `docs/limites_conocidos.md`, `docs/estado_20261110.md` y `docs/pruebas_fisicas_20261110.csv` actualizados y verificados con `verificar_version.py --solo-archivo --apk dist\carro_fsi_0.4-6d1b213.apk --revision 6d1b213` (`PASA`).
- **Fase 3 (Integrar J16 en `main`):** COMPLETADA (commit de fusión `cd90001`).
  - `git switch main; git merge --no-ff j16-precision` completado sin conflictos (`cd900019647a51d5df27f5986aea72c0d55c55f2`).
  - Pruebas repetidas en `main`: 100 % en verde (`nucleo_c`: 22 programas, 1,655 casos; `pruebas_firmware`: 5 programas, 540 casos; `probar_version.py`: 18 pruebas; `probar_servidor_falso.py`: 54 casos; `verificar_enlaces.py`: 134 archivos, 73 enlaces, 0 rotos).
  - Publicadas en `origin`: `main` (`cd90001`) y `j16-precision` (`63bf0f9`).
- **Fase 4 (Manual en PDF v1.1):** COMPLETADA.
  - Diagnóstico visual de las 67 páginas previas en `docs/manual/diagnostico_v1.md`.
  - Plan editorial en `docs/manual/plan_final.md` y auditoría de trazabilidad en `docs/manual/auditoria_afirmaciones.md`.
  - Reescritura completa de los 16 capítulos (`docs/manual/capitulos/01_introduccion.md` .. `16_anexos.md`), del generador de figuras (`docs/figuras/generar_figuras_v2.py` y `generar_figuras.py`) y del maquetador en ReportLab (`docs/manual/generar_manual.py`).
  - Generado `docs/manual/Manual_Proyecto_Carro_FSI.pdf` (`51` páginas, `12` figuras, `23` tablas, `3,650.2 KB`, fuentes `DejaVu*` incrustadas) y revisado visualmente en 3 iteraciones hasta corregir todos los defectos gráficos y tipográficos.
  - Copia sin versionar `Manual_Proyecto_Carro_FSI_v1.1.pdf` lista en la raíz para el release de GitHub.
- **Fase 5 (Publicar y comprobar acceso):** EN CURSO.
- **Fase 6 (Reportes):** PENDIENTE.

## Decisiones tomadas
- Se mantiene la identidad del repositorio (`fernandosegrr <223138626+fernandosegrr@users.noreply.github.com>`).
- La APK validada en el carro (`app-debug.apk`, `APP:0.4-6d1b213`, 14,881,402 bytes, SHA-256 `C5B36FEC74BFEDE9798B4BE7C992D7FEA8369F456DA4D4A10D61A4BBB25ED02F`) se publica en `dist/carro_fsi_0.4-6d1b213.apk` reemplazando `dist/carro_fsi_0.4-873bfb9.apk`.
- El binario del firmware (`FW:6d1b213-20261110`, 1,034,112 bytes, SHA-256 `71D671A4E64E0F234015E8685D4E741B87471534E3E50869EE023C5A657B79FC`) NO se publica en Git; solo se documenta su versión y hash en `dist/FIRMWARE_LOCAL.txt` y `dist/LEEME.txt`.
- El manual técnico se reescribió íntegramente en 16 capítulos (`51` páginas) distinguiendo siempre `VALIDADO EN EL CARRO` (en rutas J16 solo el esquive B6: 6 corridas, 0 toques, 5 completas autónomas con error lateral aproximado de `~0` a `~10 cm` y 1 en `AYUDA` sin causa identificada), `SIMULADO` y `NO MEDIDO`.

## Comandos para reanudar
```powershell
Set-Location E:\Dev\fsi_proyecto
git status -sb
git branch -a -vv
```

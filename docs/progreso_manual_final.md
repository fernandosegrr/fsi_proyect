# Progreso del Manual Final v1.1 y Cierre J16

## Estado actual: TODAS LAS FASES COMPLETADAS
- **Fase 1 (Estado y verificación):** COMPLETADA.
  - Worktree confirmado en `E:\Dev\fsi_proyecto`.
  - Etiqueta `v1.0` intacta en `eb88151` (`eb88151f438048190c26c9ed9ba26fbb6ad9424e`).
  - `main` local actualizado desde `origin/main` (`5d5c628`).
  - Todas las suites en `j16-precision` (`6d1b213`) en verde (`22` programas / `1,655` casos en `nucleo_c`, `5` programas / `540` casos en `pruebas_firmware`, `18` en `probar_version.py`, `54` en `probar_servidor_falso.py`, `0` enlaces rotos).
- **Fase 2 (APK y registros en `j16-precision`):** COMPLETADA (commit `63bf0f9`).
  - Publicada `dist/carro_fsi_0.4-6d1b213.apk` (`14,881,402` bytes, SHA-256 `C5B36FEC74BFEDE9798B4BE7C992D7FEA8369F456DA4D4A10D61A4BBB25ED02F`) y retirada `dist/carro_fsi_0.4-873bfb9.apk`.
  - Actualizados `dist/SHA256SUMS.txt`, `dist/LEEME.txt`, `dist/FIRMWARE_LOCAL.txt` (`FW:6d1b213-20261110`, SHA-256 `71D671A4E64E0F234015E8685D4E741B87471534E3E50869EE023C5A657B79FC`), `dist/README.md`, `docs/limites_conocidos.md`, `docs/estado_20261110.md` y `docs/pruebas_fisicas_20261110.csv`.
- **Fase 3 (Integrar J16 en `main`):** COMPLETADA (commit de fusión `cd90001`).
  - Merge `--no-ff` de `j16-precision` en `main`, pruebas repetidas en `main` con `0` fallos y push a `origin`.
- **Fase 4 (Manual en PDF, Versión final 1.1):** COMPLETADA (commit `adfbda1` en `docs/manual-final`, fusionado en `main` en `c32e650`).
  - Diagnóstico visual (`docs/manual/diagnostico_v1.md`), plan (`docs/manual/plan_final.md`), auditoría (`docs/manual/auditoria_afirmaciones.md`), 12 figuras/láminas (`docs/figuras/generar_figuras_v2.py`) y 16 capítulos reescritos (`docs/manual/capitulos/01_introduccion.md`..`16_anexos.md`).
  - Generado `docs/manual/Manual_Proyecto_Carro_FSI.pdf` (`51` páginas, `12` figuras, `23` tablas, `3,650.2 KB`, fuentes `DejaVu*` incrustadas) y verificado visualmente en 3 iteraciones.
- **Fase 5 (Publicar y comprobar acceso):** COMPLETADA.
  - Actualizados `README.md`, `docs/README.md`, `docs/manual/README.md`, `docs/figuras/README.md` y `docs/entrega.md`.
  - Verificador de enlaces y suites de código en verde; etiqueta anotada `v1.1` creada y publicada en `origin` junto con `main`, `j16-precision` y `docs/manual-final`.
- **Fase 6 (Reportes finales):** COMPLETADA.

## Decisiones tomadas
- Identidad de Git: `fernandosegrr <223138626+fernandosegrr@users.noreply.github.com>`.
- El binario `.bin` del firmware (`FW:6d1b213-20261110`) y `secretos.h` no se publican en Git ni en releases; solo se documenta el hash SHA-256 del firmware.
- Se mantiene distinción estricta entre `VALIDADO EN EL CARRO` (en rutas J16 únicamente el esquive B6: 6 corridas, 0 toques, 5 autónomas completas con error lateral aproximado de `~0` a `~10 cm` y 1 en `AYUDA` sin causa identificada), `SIMULADO` y `NO MEDIDO`.

## Comandos de verificación rápida
```powershell
Set-Location E:\Dev\fsi_proyecto
git status -sb
git tag -l -n1
python docs\verificar_enlaces.py
```

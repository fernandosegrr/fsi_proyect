# Entrega y estado de validación del Proyecto Carro FSI (LIDIA — Versión final 1.1)

**Fecha de corte:** 10 de noviembre de 2026  
**Etiqueta de versión:** `v1.1` (cierre de precisión e integración **J16**; la etiqueta histórica `v1.0` se conserva intacta en `eb88151`).  
**Manual técnico principal (PDF, Versión final 1.1):** [docs/manual/Manual_Proyecto_Carro_FSI.pdf](manual/Manual_Proyecto_Carro_FSI.pdf)

---

## 1. Contenido publicado en `dist/`

- **[`dist/carro_fsi_0.4-6d1b213.apk`](../dist/carro_fsi_0.4-6d1b213.apk)**
  - **Versión interna:** `APP:0.4-6d1b213` (`versionName = "0.4"`, `versionCode = 3`).
  - **Tamaño exacto:** `14,881,402` bytes.
  - **SHA-256:** `C5B36FEC74BFEDE9798B4BE7C992D7FEA8369F456DA4D4A10D61A4BBB25ED02F`.
- **[`dist/SHA256SUMS.txt`](../dist/SHA256SUMS.txt):** suma criptográfica SHA-256 de la APK oficial.
- **[`dist/LEEME.txt`](../dist/LEEME.txt):** guía rápida de instalación y resumen honesto de validación.
- **[`dist/FIRMWARE_LOCAL.txt`](../dist/FIRMWARE_LOCAL.txt):** registro del firmware validado en el carro (`FW:6d1b213-20261110`, tamaño `1,034,112` bytes, SHA-256 `71D671A4E64E0F234015E8685D4E741B87471534E3E50869EE023C5A657B79FC`). Por seguridad, el binario `.bin` del firmware y `secretos.h` **nunca** se publican en Git porque contienen las credenciales WiFi.

---

## 2. Verificación e instalación de la APK

Desde la raíz del repositorio en PowerShell:

```powershell
# 1. Comprobar el hash SHA-256 del archivo APK
Get-FileHash dist\carro_fsi_0.4-6d1b213.apk -Algorithm SHA256

# 2. Verificar integridad y metadatos internos de versión sin requerir hardware conectado
python scripts\verificar_version.py --solo-archivo --apk dist\carro_fsi_0.4-6d1b213.apk --revision 6d1b213

# 3. Instalar en el teléfono Android por ADB conservando calibración y preferencias (-r)
adb install -r dist\carro_fsi_0.4-6d1b213.apk
```

---

## 3. Resumen del estado de validación (Cierre J16)

1. **`VALIDADO EN EL CARRO` (pruebas físicas del 10 de noviembre de 2026):**
   - Con `FW:6d1b213-20261110` y `APP:0.4-6d1b213` se validó físicamente en el carro **únicamente el escenario de esquive con reincorporación B6** ([`docs/pruebas_fisicas_20261110.csv`](pruebas_fisicas_20261110.csv)):
   - **6 corridas físicas:** **`0/6` toques de caja**, **`5/6` corridas autónomas completas (`TERMINADO`)** con desviación lateral final aproximada entre **`~0 cm` y `~10 cm` a la derecha** (`~10, ~10, ~5, ~0, ~0 cm`), y **`1/6` corrida (`J16_B6_r5`) en `AYUDA`** (paso `4/10`) sin tocar la caja y sin causa identificada.
2. **`VERIFICADO` en software (PC):**
   - `nucleo_c`: `22` programas y `1,655` casos (`0` fallos).
   - `pruebas_firmware`: `5` programas y `540` casos (`0` fallos) + `verificar_pagina.py` (`3/3` OK).
   - `probar_servidor_falso.py`: `54` casos (`0` fallos).
   - `probar_version.py`: `18` casos (`0` fallos).
   - `verificar_enlaces.py`: `0` enlaces rotos.
3. **`SIMULADO` en computadora ([`docs/precision/monte_carlo_1000.txt`](precision/monte_carlo_1000.txt)):**
   - `50,000` corridas Monte Carlo (`1,000` por escenario en 5 configuraciones con semilla fija `0xF51A2026`): recta de 150 cm (`94 mm -> 8 mm`), ruta en L (`64 mm -> 36 mm`), cuadrado de 60 cm (`142 mm -> 42 mm`) y esquive (`128 mm -> 38 mm`, `0 %` colisiones).
4. **`NO MEDIDO` en el carro físico:**
   - Rectas físicas con y sin `RUMBO` (`B5`), ruta en L (`B7`), cuadrado de 60 cm (`B8`), calibración en piso de `360°` para `IMU_ESCALA`, corte de WiFi en marcha (`B9`), freno autónomo aislado por láser (`TOF_PARO` a `120 mm`), caja ancha (`40×20 cm`), calibración automática (`B2`), circuito `GPIO 2` y consumo eléctrico en amperios.

---

## 4. Documentos relacionados

- Estado detallado: [estado_20261110.md](estado_20261110.md) y [limites_conocidos.md](limites_conocidos.md).
- Reporte técnico J16: [reporte_J16.md](reporte_J16.md).
- Manual técnico completo en PDF: [manual/Manual_Proyecto_Carro_FSI.pdf](manual/Manual_Proyecto_Carro_FSI.pdf).

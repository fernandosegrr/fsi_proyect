# Paquete de entrega y distribución (`dist`)

Artefactos compilados y su manifiesto de verificación correspondientes al cierre de J16 (`6d1b213`, validado en el carro en el esquive B6 el 10 de noviembre de 2026). La entrega previa se conserva en la etiqueta `v1.0` (`eb88151`).

---

## 1. Contenido del directorio

| Archivo | Tamaño | SHA-256 | Estado en git | Propósito |
|---|---|---|---|---|
| `carro_fsi_0.4-6d1b213.apk` | 14,881,402 bytes | `C5B36FEC74BFEDE9798B4BE7C992D7FEA8369F456DA4D4A10D61A4BBB25ED02F` | Versionado | APK `APP:0.4-6d1b213` compilada desde `6d1b213`, instalada en el teléfono y validada en el carro en las 6 corridas del esquive B6. Menos de 15 MB. |
| `FW:6d1b213-20261110` (binario local) | 1,034,112 bytes | `71D671A4E64E0F234015E8685D4E741B87471534E3E50869EE023C5A657B79FC` | **No versionado** | Firmware J16 cargado en la ESP32 y probado en el esquive B6 (`RUMBO`, aviso `WD` y escala del giroscopio). Contiene la contraseña del WiFi: no se publica. |
| `firmware_carro_453ed20-20261027.bin` | 1,031,712 bytes | `9FD1BB5103D1AD66652ADEA68B009D54D30B5C5B6F07F8CA7488A4FBF274048E` | **No versionado** | Firmware anterior (J14/J15), conservado localmente para respaldo. |
| `SHA256SUMS.txt` | — | — | Versionado | Manifiesto de sumas de los artefactos publicados (solo la APK). |
| `FIRMWARE_LOCAL.txt` | — | — | Versionado | Versión, tamaño, hash SHA-256 y pasos para reconstruir el firmware sin publicar el binario. |
| `LEEME.txt` | — | — | Versionado | Resumen rápido de versiones, hashes y estado de validación (qué está validado en el carro y qué es simulado o no medido). |

Las APK de candidatos anteriores (`carro_fsi_0.4-873bfb9.apk`, `carro_fsi_0.4-097008d.apk`, `carro_fsi_0.4-d823a63.apk`, `carro_fsi_0.4-074c9fd.apk`) se conservan en el historial de Git; la de la entrega `v1.0`, en esa etiqueta (`eb88151`).

---

## 2. Cómo verificar la integridad

Sin teléfono ni puerto serie:

```powershell
python scripts/verificar_version.py --solo-archivo --apk dist/carro_fsi_0.4-6d1b213.apk --revision 6d1b213
```

Comprueba la versión incrustada contra la revisión indicada, que las fuentes estén limpias, el nombre del archivo y `SHA256SUMS.txt`.

Directamente:

```powershell
Get-FileHash dist/carro_fsi_0.4-6d1b213.apk -Algorithm SHA256
```

El resultado debe ser `C5B36FEC74BFEDE9798B4BE7C992D7FEA8369F456DA4D4A10D61A4BBB25ED02F`.

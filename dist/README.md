# Paquete de entrega y distribución (`dist`)

Artefactos compilados y su manifiesto de verificación. En la rama `j16-precision` es el **candidato J16**, todavía sin validación física; la entrega validada sigue en la etiqueta `v1.0` (eb88151).

---

## 1. Contenido del directorio

| Archivo | Tamaño | SHA-256 | Estado en git | Propósito |
|---|---|---|---|---|
| `carro_fsi_0.4-097008d.apk` | 14,511,016 bytes | `F0A037CB37C203A8F44279B91DD8D2061665ECC03DAC45FD347AD523AE78842A` | Versionado | APK del candidato J16 (segunda ronda), compilada desde el commit limpio 097008d. Menos de 15 MB. NO instalada en J16. |
| `firmware_carro_097008d-20261031.bin` | 1,033,440 bytes | `D7CEC2A9A07F3597798C7AF8603A007D4555D3A28B3DB12756D3D88A9AA64F2C` | **No versionado** (excluido localmente) | Firmware J16 compilado en la PC (RUMBO y aviso WD del watchdog). Contiene la contraseña del WiFi: no se publica. NO cargado. |
| `firmware_carro_453ed20-20261027.bin` | 1,031,712 bytes | `9FD1BB5103D1AD66652ADEA68B009D54D30B5C5B6F07F8CA7488A4FBF274048E` | **No versionado** | Firmware anterior (el del carro en J14/J15), para volver atrás. |
| `SHA256SUMS.txt` | — | — | Versionado | Manifiesto de sumas de los artefactos publicados (solo el APK). |
| `FIRMWARE_LOCAL.txt` | — | — | Versionado | Cómo reconstruir y, cuando se decida, cargar el firmware. |
| `LEEME.txt` | — | — | Versionado | Resumen rápido del candidato y su estado. |

La APK de J15 (`carro_fsi_0.4-074c9fd.apk`) y la de la primera ronda de J16 (`carro_fsi_0.4-d823a63.apk`) se conservan en el historial de Git; la de la entrega v1.0, en esa etiqueta.

---

## 2. Cómo verificar la integridad

Sin teléfono ni puerto serie (J16):

```powershell
python scripts/verificar_version.py --solo-archivo --apk dist/carro_fsi_0.4-097008d.apk
```

Comprueba la versión incrustada contra Git, que las fuentes estén limpias, el nombre del archivo y `SHA256SUMS.txt`. No sustituye la comprobación del APK instalado (`--solo-apk`, con el teléfono) ni la del firmware (`--puerto`).

Directamente:

```powershell
Get-FileHash dist/carro_fsi_0.4-097008d.apk -Algorithm SHA256
```

El resultado debe ser `F0A037CB37C203A8F44279B91DD8D2061665ECC03DAC45FD347AD523AE78842A`.

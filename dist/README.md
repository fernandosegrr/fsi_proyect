# Paquete de entrega y distribución (`dist`)

Artefactos compilados y su manifiesto de verificación. En la rama `j16-precision` es el **candidato J16**, todavía sin validación física; la entrega validada sigue en la etiqueta `v1.0` (eb88151).

---

## 1. Contenido del directorio

| Archivo | Tamaño | SHA-256 | Estado en git | Propósito |
|---|---|---|---|---|
| `carro_fsi_0.4-d823a63.apk` | 14,508,012 bytes | `76DB84867344808E140EDA7328600B5F9F55CE93DDDAFD23004880C7AC80484D` | Versionado | APK del candidato J16, compilada desde el commit limpio d823a63. Menos de 15 MB. NO instalada en J16. |
| `firmware_carro_d823a63-20261031.bin` | 1,033,312 bytes | `D955B69D0ACE616B0E591789250B8B0054ECD705E3EA3DEAB4898FBE98849FA8` | **No versionado** (excluido localmente) | Firmware J16 compilado en la PC (RUMBO). Contiene la contraseña del WiFi: no se publica. NO cargado. |
| `firmware_carro_453ed20-20261027.bin` | 1,031,712 bytes | `9FD1BB5103D1AD66652ADEA68B009D54D30B5C5B6F07F8CA7488A4FBF274048E` | **No versionado** | Firmware anterior (el del carro en J14/J15), para volver atrás. |
| `SHA256SUMS.txt` | — | — | Versionado | Manifiesto de sumas de los artefactos publicados (solo el APK). |
| `FIRMWARE_LOCAL.txt` | — | — | Versionado | Cómo reconstruir y, cuando se decida, cargar el firmware. |
| `LEEME.txt` | — | — | Versionado | Resumen rápido del candidato y su estado. |

La APK de J15 (`carro_fsi_0.4-074c9fd.apk`) se conserva en el historial de Git; la de la entrega v1.0, en esa etiqueta.

---

## 2. Cómo verificar la integridad

Sin teléfono ni puerto serie (J16):

```powershell
python scripts/verificar_version.py --solo-archivo --apk dist/carro_fsi_0.4-d823a63.apk
```

Comprueba la versión incrustada contra Git, que las fuentes estén limpias, el nombre del archivo y `SHA256SUMS.txt`. No sustituye la comprobación del APK instalado (`--solo-apk`, con el teléfono) ni la del firmware (`--puerto`).

Directamente:

```powershell
Get-FileHash dist/carro_fsi_0.4-d823a63.apk -Algorithm SHA256
```

El resultado debe ser `76DB84867344808E140EDA7328600B5F9F55CE93DDDAFD23004880C7AC80484D`.

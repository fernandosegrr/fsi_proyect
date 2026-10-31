# Paquete de entrega y distribución (`dist`)

Artefactos compilados y su manifiesto de verificación. En la rama `j16-precision` es el **candidato J16**, todavía sin validación física; la entrega validada sigue en la etiqueta `v1.0` (eb88151).

---

## 1. Contenido del directorio

| Archivo | Tamaño | SHA-256 | Estado en git | Propósito |
|---|---|---|---|---|
| `carro_fsi_0.4-873bfb9.apk` | 14,520,520 bytes | `50179013BCB18029F6C1BCBFE737F59A9B14B49B7BE848E002D67C119433C15B` | Versionado | APK del candidato J16 (tercera ronda: escala del giroscopio), compilada desde el commit limpio 873bfb9. Menos de 15 MB. NO instalada en J16. |
| `firmware_carro_873bfb9-20261031.bin` | 1,034,112 bytes | `5B00529D21999F1BB8441D265D673D12CD163D3A3F59F5E1EFBB468C88AA6D6A` | **No versionado** (excluido localmente) | Firmware J16 compilado en la PC (RUMBO, aviso WD y escala del giroscopio). Contiene la contraseña del WiFi: no se publica. NO cargado. |
| `firmware_carro_453ed20-20261027.bin` | 1,031,712 bytes | `9FD1BB5103D1AD66652ADEA68B009D54D30B5C5B6F07F8CA7488A4FBF274048E` | **No versionado** | Firmware anterior (el del carro en J14/J15), para volver atrás. |
| `SHA256SUMS.txt` | — | — | Versionado | Manifiesto de sumas de los artefactos publicados (solo el APK). |
| `FIRMWARE_LOCAL.txt` | — | — | Versionado | Cómo reconstruir y, cuando se decida, cargar el firmware. |
| `LEEME.txt` | — | — | Versionado | Resumen rápido del candidato y su estado. |

La APK de J15 (`carro_fsi_0.4-074c9fd.apk`) y las de las rondas anteriores de J16 (`carro_fsi_0.4-d823a63.apk`, `carro_fsi_0.4-097008d.apk`) se conservan en el historial de Git; la de la entrega v1.0, en esa etiqueta.

---

## 2. Cómo verificar la integridad

Sin teléfono ni puerto serie (J16):

```powershell
python scripts/verificar_version.py --solo-archivo --apk dist/carro_fsi_0.4-873bfb9.apk
```

Comprueba la versión incrustada contra Git, que las fuentes estén limpias, el nombre del archivo y `SHA256SUMS.txt`. No sustituye la comprobación del APK instalado (`--solo-apk`, con el teléfono) ni la del firmware (`--puerto`).

Directamente:

```powershell
Get-FileHash dist/carro_fsi_0.4-873bfb9.apk -Algorithm SHA256
```

El resultado debe ser `50179013BCB18029F6C1BCBFE737F59A9B14B49B7BE848E002D67C119433C15B`.

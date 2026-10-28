# Paquete de entrega y distribución (`dist`)

Este directorio contiene los artefactos compilados de entrega final y sus manifiestos criptográficos de verificación para el proyecto del carro FSI.

---

## 1. Contenido del directorio

| Archivo | Tamaño | SHA-256 | Estado en git | Propósito |
|---|---|---|---|---|
| `carro_fsi_0.4-074c9fd.apk` | 14,852,386 bytes | `36CE38F2ABD11C1030A7DE7B8A23009A865A01365900192E69CED9CDDE9513BA` | Versionado en git | Paquete ejecutable para teléfonos Android. Cumple el límite estricto de menos de 15 MB para entregas del curso. |
| `firmware_carro_453ed20-20261027.bin` | 1,031,712 bytes | `9FD1BB5103D1AD66652ADEA68B009D54D30B5C5B6F07F8CA7488A4FBF274048E` | **No versionado** (ignorado en git) | Imagen binaria del firmware compilada localmente para flasheo directo en ESP32 DevKit. Al contener credenciales de red compiladas, no se publica en el repositorio público. |
| `SHA256SUMS.txt` | 93 bytes | `59E3430B668C938EDB7467D0939A018BF9EE2E7298F1A1884A1FB159E0ADF8D9` | Versionado en git | Manifiesto de sumas de verificación de los artefactos oficiales. |
| `FIRMWARE_LOCAL.txt` | 726 bytes | `8CD5CC83A2F26C0CB7DE8D9F59D9B9E80DC634AD2D4F9B6A22E662E6BA3246B6` | Versionado en git | Instrucciones para reconstruir el binario local del firmware a partir del código fuente. |
| `LEEME.txt` | 1,324 bytes | `B1D7E7BB97099C6A467399D2955F24AE11FC4D3FC231C56759D43C4211721BB0` | Versionado en git | Guía rápida de comprobación de integridad en terminal. |

---

## 2. Cómo verificar la integridad de la entrega

Para comprobar que el APK y los archivos entregados no han sufrido alteraciones, ejecute en PowerShell:

```powershell
# Comprobación automática con el script verificador del proyecto
python scripts/verificar_version.py --solo-apk
```

O bien directamente con utilidades del sistema:

```powershell
# En Windows PowerShell
Get-FileHash dist/carro_fsi_0.4-074c9fd.apk -Algorithm SHA256
```

El hash resultante debe coincidir exactamente con `36CE38F2ABD11C1030A7DE7B8A23009A865A01365900192E69CED9CDDE9513BA`.

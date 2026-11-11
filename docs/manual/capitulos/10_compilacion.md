# 10. Compilación, instalación y verificación de versión

Este capítulo documenta los requisitos de herramientas, los comandos exactos para compilar cada subsistema desde el código fuente, el procedimiento seguro de carga en las placas `ESP32` y en el teléfono Android, y el sistema de trazabilidad criptográfica que vincula la aplicación `APP:0.4-6d1b213` y el firmware `FW:6d1b213-20261110` con el commit de Git.

## 10.1 Herramientas de desarrollo requeridas

El repositorio está preparado para compilarse y probarse en Windows (con `PowerShell`) o entornos POSIX sin depender de servicios en la nube:

1. **Compilador C99 y Make (`gcc` / `MinGW-w64` y `GNU Make`):** compila los 22 ejecutables de prueba del núcleo (`nucleo_c`) y los 5 ejecutables de lógica del firmware (`pruebas_firmware`).
2. **Arduino CLI (`arduino-cli`) con núcleo `esp32:esp32` (`3.3.7`):** compila y carga tanto `firmware_carro/firmware_carro.ino` (`FQBN esp32:esp32:esp32`) como `camara_prueba/camara_prueba.ino` (`FQBN esp32:esp32:esp32wrover`).
3. **Android SDK, NDK (`27.0.12077973`), CMake (`3.22.1`) y JDK 17 (`Gradle`):** compila la biblioteca nativa `libcarro_nucleo.so` desde `nucleo_c/*.c` más `puente.c` y empaqueta la aplicación Android (`app-debug.apk`).
4. **Python 3 (`>= 3.9`):** ejecuta los verificadores de versión (`verificar_version.py`, `probar_version.py`), el servidor falso (`probar_servidor_falso.py`), el verificador de enlaces (`verificar_enlaces.py`) y el generador reproducible de este manual (`generar_manual.py` con `reportlab` y `pymupdf`).

## 10.2 Compilación y carga del firmware del carro y de la cámara

### Configuración de credenciales WiFi sin exponer secretos en Git

Por seguridad, el repositorio **nunca almacena contraseñas reales**. En `firmware_carro/` y en `camara_prueba/` se distribuyen únicamente las plantillas `secretos.ejemplo.h`. Antes de compilar por primera vez en una máquina nueva, el desarrollador copia `secretos.ejemplo.h` a `secretos.h` (archivo ignorado por `.gitignore`) y define allí la clave de al menos 8 caracteres de la red `CarroFSI`.

### Uso de `scripts/cargar.ps1` y precaución crítica sobre la sonda de placas

El script `scripts/cargar.ps1` automatiza la inyección de la versión de Git (`version_generada.h` con la cadena `FW:<hash>-<AAAAMMDD>`) y la compilación con `arduino-cli`:

- **Solo verificar compilación (sin tocar hardware ni puertos USB):**
  ```powershell
  powershell -ExecutionPolicy Bypass -File scripts\cargar.ps1 -Proyecto carro -SoloCompilar
  powershell -ExecutionPolicy Bypass -File scripts\cargar.ps1 -Proyecto camara -SoloCompilar
  ```
- **Cargar en una placa cuyo puerto COM ya se conoce (recomendado):**
  ```powershell
  powershell -ExecutionPolicy Bypass -File scripts\cargar.ps1 -Proyecto carro -Puerto COM5
  ```

> **Advertencia operativa sobre la detección automática de puertos:** si se invoca `scripts/cargar.ps1` **sin** `-SoloCompilar` y **sin** especificar `-Puerto COMx`, cuando hay varios puertos serie presentes el script puede cargar temporalmente el programa `scripts/sonda_placa/sonda_placa.ino` para identificar la placa, lo cual **sobrescribe el firmware previamente instalado**. Por ello, siempre debe indicarse `-SoloCompilar` cuando se trabaja sin hardware o `-Puerto COMx` cuando se desea grabar una placa específica.

## 10.3 Compilación e instalación de la aplicación Android

El archivo `app_android/app/build.gradle.kts` consulta automáticamente `git rev-parse --short=7 HEAD` durante la construcción e inyecta el hash corto en `versionName` (`0.4-<hash>`) y en `BuildConfig.GIT_REVISION`.

1. **Compilar la APK de depuración desde consola:**
   ```powershell
   powershell -ExecutionPolicy Bypass -File scripts\compilar_app.ps1
   ```
   El ejecutable generado se deposita en `app_android\app\build\outputs\apk\debug\app-debug.apk` y se copia para distribución en `dist\carro_fsi_0.4-6d1b213.apk`.
2. **Instalación en el teléfono:** puede copiarse directamente el archivo `dist\carro_fsi_0.4-6d1b213.apk` al teléfono Android y abrirlo desde el gestor de archivos (autorizando la instalación de orígenes locales), o bien instalarse por cable USB mediante `adb install -r dist\carro_fsi_0.4-6d1b213.apk`.

## 10.4 Artefactos validados de la Versión final 1.1 y verificación criptográfica

Para impedir que se ejecuten pruebas en el carro con una APK desactualizada o con un firmware de otra revisión, el proyecto incluye la herramienta `scripts/verificar_version.py` (respaldada por `18` pruebas unitarias en `scripts/probar_version.py`). La siguiente tabla documenta los artefactos exactos de la **Versión final 1.1**:

| Artefacto | Identificador de versión | Ubicación en el repositorio | Tamaño exacto | Hash SHA-256 verificado |
|---|---|---|---|---|
| **Aplicación Android (APK)** | `APP:0.4-6d1b213` (`versionCode = 5`) | `dist/carro_fsi_0.4-6d1b213.apk` | `14,881,402 B` (`14.88 MB`) | `C5B36FEC74BFEDE9798B4BE7C992D7FEA8369F456DA4D4A10D61A4BBB25ED02F` |
| **Firmware del carro (`ESP32 DevKit`)** | `FW:6d1b213-20261110` | Código fuente en `firmware_carro/`; registro en `dist/FIRMWARE_LOCAL.txt` | `1,034,112 B` *(binario local no publicado)* | `71D671A4E64E0F234015E8685D4E741B87471534E3E50869EE023C5A657B79FC` |

### Por qué no se publica el binario `.bin` del firmware

Al compilar `firmware_carro.ino`, el preprocesador de C incrusta en la sección de datos constantes (`.rodata`) del archivo `.bin` el SSID y la contraseña WiFi definidos en `secretos.h`. Publicar el archivo `.bin` en GitHub o en un *release* permitiría extraer la contraseña en texto plano con una simple búsqueda de cadenas (`strings`). Por esta razón, **`dist/` solo publica la APK (`carro_fsi_0.4-6d1b213.apk`), el manifiesto `SHA256SUMS.txt` y el registro `FIRMWARE_LOCAL.txt` con el tamaño y el hash SHA-256 del firmware**, sin incluir jamás el binario `.bin`.

### Comandos de verificación (`verificar_version.py`)

- **Verificar la APK empaquetada contra una revisión específica (sin hardware ni ADB):**
  ```powershell
  python scripts\verificar_version.py --solo-archivo --apk dist\carro_fsi_0.4-6d1b213.apk --revision 6d1b213
  ```
- **Verificar la integridad criptográfica de `dist/SHA256SUMS.txt` en PowerShell:**
  ```powershell
  (Get-FileHash dist\carro_fsi_0.4-6d1b213.apk -Algorithm SHA256).Hash
  ```

Dónde está en el código: `cargar.ps1`, `compilar_app.ps1`, `verificar_version.py`, `probar_version.py`, `build.gradle.kts`, `SHA256SUMS.txt`, `FIRMWARE_LOCAL.txt`.

# Referencia de compilación

**Estado:** documento de referencia · **Fecha:** 27 de octubre de 2026 · **Rama:** `docs/documentacion-completa`
**Fuentes:** `README.md`, `nucleo_c/Makefile`, `pruebas_firmware/Makefile`, `app_android/app/build.gradle.kts`, `app_android/app/src/main/cpp/CMakeLists.txt`, `scripts/cargar.ps1`, `scripts/verificar_version.py`, `docs/hardware_cargar.md` y `docs/entrega.md`.

## 1. Qué se compila y con qué

**Compilar** es traducir el código que escribe una persona a un archivo que la placa o el teléfono pueden ejecutar. El conjunto de programas que hacen esa traducción se llama **toolchain**. El proyecto tiene cuatro productos y cada uno usa una herramienta distinta:

| Producto | Herramienta | Resultado |
|---|---|---|
| Firmware del carro | `arduino-cli` + core `esp32:esp32` 3.3.12 | `.bin` para la DevKit |
| Firmware de la cámara | `arduino-cli` + core `esp32:esp32` 3.3.12 | `.bin` para la Freenove |
| Núcleo y pruebas de PC | `gcc` + `make` | 21 ejecutables de prueba |
| App Android | Gradle + Android SDK/NDK + CMake | `.apk` |

En palabras llanas: `arduino-cli` compila para las placas; `gcc` y `make` compilan para el PC; **Gradle** empaqueta la app de Android; el **SDK** trae las herramientas y bibliotecas de Android; el **NDK** permite compilar C para el teléfono; y **CMake** organiza esa compilación nativa.

La regla del proyecto es que **el código C se compile sin copiarlo**: la app toma los `.c` de `nucleo_c/` desde su carpeta original, y las pruebas de firmware toman `carro.c` desde `firmware_carro/`. Así no hay dos versiones de la misma lógica.

## 2. Firmware del carro

### Requisitos

- `arduino-cli` en el `PATH`.
- Core `esp32:esp32` versión **3.3.12**.
- Librerías: **Async TCP 3.5.0**, **ESP Async WebServer 3.12.0** y **VL53L0X de Pololu 1.3.1** (`docs/pendientes_firmware.md:39`; `firmware_carro.ino:17-24`).
- `firmware_carro/secretos.h` con el nombre y la contraseña de la red. Si no existe, se usa `secretos.ejemplo.h` y el proyecto compila con valores de ejemplo (`config.h:20-36`).

### Compilar

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32 firmware_carro
```

### Cargar

```powershell
.\scripts\cargar.ps1 nuevo -Puerto COM6
```

`cargar.ps1` compila en una carpeta temporal, obtiene el hash corto del commit, escribe `version_compilada.h` con `FW:<hash>-<aaaammdd>` y sube el binario (`scripts/cargar.ps1:163-173`). También reintenta a 921600, 512000 y 115200 baudios si la primera subida falla (`docs/hardware_cargar.md` §«Uso»).

### Verificar sin cargar

```powershell
.\scripts\cargar.ps1 nuevo -SoloCompilar
```

En J14/J15 el firmware compiló al **73-74 % de la flash** (`docs/hardware_cargar.md` §«Qué se probó»; `docs/camara_prueba.md` §«Cargar»). El binario conservado en J15 es `FW:453ed20-20261027`, 1 031 712 bytes, SHA-256 `9FD1BB51...274048E` (`docs/entrega.md` §4). **No se publica** porque contiene la contraseña del WiFi.

## 3. Firmware de la cámara

### Requisitos

- Core `esp32:esp32` 3.3.12.
- Placa **ESP32 Wrover Module** (`esp32:esp32:esp32wrover`, con PSRAM).
- `camara_prueba/secretos.h` (ignorado por git).

### Compilar y cargar

```powershell
.\scripts\cargar.ps1 camara -SoloCompilar
.\scripts\cargar.ps1 camara -Puerto COM8
```

`cargar.ps1` copia el sketch a una carpeta temporal y, si existen, los archivos `obstaculos.c/h` y `paquete_celdas.c/h` de `nucleo_c/` para el modo CELDAS (`scripts/cargar.ps1:176-190`). Sin ellos, el sketch compila solo con JPEG y RAW.

La cámara ocupa ~**83 % de la flash** (`docs/camara_prueba.md` §«Cargar»). El `COMPRESOR_JPEGENC` está activo para comparar compresores, pero el compresor por defecto sigue siendo `frame2jpg` (`camara_prueba.ino:51-54`).

## 4. Núcleo y pruebas de PC

### Requisitos

- `gcc` con soporte C99.
- `make`.
- En Windows, un entorno que tenga ambos (MSYS2/MinGW o WSL). El proyecto se desarrolló y probó en este equipo con esas herramientas; la versión exacta no está registrada en el repositorio (**PENDIENTE** de anotar).

### Compilar y ejecutar todo

```bash
cd nucleo_c
make test
```

El `Makefile` compila con `-std=c99 -Wall -Wextra -Wpedantic -Werror` (`nucleo_c/Makefile:4-5`). Cualquier advertencia es error: el código debe compilar limpio. La lista de programas está en `nucleo_c/Makefile:7` y la de ejecución en `72-93`.

```bash
cd pruebas_firmware
make test
```

Compila `carro.c`, los sensores y `recuperacion.c` con hardware falso, ejecuta las pruebas y verifica el hash de la página web (`pruebas_firmware/Makefile:28-33`). El flag `-DARRANQUE_ESCALONADO_MS=40` compila una segunda variante para probar el arranque escalonado (`Makefile:19-20`).

### Limpiar

```bash
make clean
```

Borra los ejecutables, no las fuentes.

### Estado de las pruebas

El último registro es **21 programas / 1598 casos** en el núcleo, **447** en firmware, **46** en el servidor falso y **13** en el verificador, con cero fallos (`README.md`; `docs/entrega.md` §5). No se repitieron durante el cierre documental por indicación del dueño. Los números no se inventan: son los del reporte.

## 5. App Android

### Requisitos

- Android Studio con SDK **compileSdk 37**, build-tools 36.0.0.
- NDK **30.0.16248370**.
- CMake **4.1.2**.
- JDK 17 (`app_android/app/build.gradle.kts:35-37`, `73-76`).

### Compilar

```powershell
cd app_android
$env:JAVA_HOME = "C:\Program Files\Android\Android Studio\jbr"
.\gradlew.bat assembleDebug
```

El APK queda en `app_android/app/build/outputs/apk/debug/`. La versión se genera con el hash de git y el nombre `0.4` (`build.gradle.kts:15-31`).

### Qué compila el NDK

`CMakeLists.txt` genera `libcarro_nativo.so` con `puente.c` y estos módulos de `nucleo_c/`: trayectoria, obstáculos, máscara, navegador, calibración, calgiro, fusión, movimiento visual, video stats, prueba de obstáculo, corredor, cualquiera, movimiento de piso, paquete de celdas, sesión compartida, latencia de video, editor de rutas, simulación y simulador (`CMakeLists.txt:25-46`). Solo se compila `arm64-v8a` (`build.gradle.kts:50-52`).

### Instalar

```powershell
adb install -r dist\carro_fsi_0.4-074c9fd.apk
```

La app instalada en J15 se verificó por hash y se conservó sin borrar datos (`docs/entrega.md` §3). **No se reinstaló durante el cierre documental.**

## 6. Verificación de versiones

`scripts/verificar_version.py` compara el hash del firmware y de la APK con Git. Es de solo lectura: no instala, no mueve y no escribe en la placa.

```powershell
pruebas_firmware\.venv\Scripts\python.exe scripts\verificar_version.py --solo-apk --apk dist\carro_fsi_0.4-074c9fd.apk --revision HEAD
```

El verificador tolera commits posteriores de documentación, entrega y del propio verificador, pero no cambios de producto (`scripts/verificar_version.py:42-60`). El resultado de J15 fue **PASA** para la APK y **pendiente** para el firmware (`docs/entrega.md` §3).

`pruebas_firmware\.venv` existe para no instalar paquetes en el Python del sistema. Si no existe, se crea con:

```powershell
python -m venv pruebas_firmware\.venv
pruebas_firmware\.venv\Scripts\pip install -r pruebas_firmware\requirements.txt
```

(El archivo de requisitos se consulta en el repositorio; si no existe, el verificador solo usa la biblioteca estándar.)

## 7. Herramientas de documentación

El manual se genera con **Python 3.9.13** y estas bibliotecas:

- `matplotlib` 3.9.4 para las figuras.
- `reportlab` 4.5.1 para el PDF.
- `PyMuPDF` 1.26.5 para renderizar y revisar el PDF página por página.

No hay `pandoc`, Graphviz ni ImageMagick en este equipo. El script del manual no depende de ellos. Los detalles están en `docs/manual/README.md`.

## 8. Problemas típicos

| Síntoma | Causa probable | Qué hacer |
|---|---|---|
| `arduino-cli: no se reconoce` | No está en el `PATH` | Instalarlo y abrir una consola nueva |
| Falla la subida al 921600 | Placa o cable | `cargar.ps1` ya reintenta; si persiste, `-EsperarBoot` |
| `Falta firmware_carro\secretos.h` | No se creó el archivo | Copiar `secretos.ejemplo.h` y editarlo |
| No aparece ningún COM | Cable de solo carga o driver ausente | Cable de datos, otro puerto, driver CP210x/CH340 |
| `gcc` o `make` no existen | Entorno C incompleto | Instalar MSYS2/MinGW o usar WSL |
| La app no compila por NDK/CMake | Versión distinta a la fijada | Instalar NDK 30.0.16248370 y CMake 4.1.2 |
| El APK supera 15 MB | Bibliotecas nativas sin comprimir | `useLegacyPackaging = true` ya está puesto (`build.gradle.kts:58-64`) |
| El firmware no entra en modo carga | GPIO 2 o 0 a 3.3 V | Desconectar lo que haya en esos pines y reintentar |

## 9. Lo que no se hizo en el cierre documental

- No se compiló ni cargó firmware.
- No se compiló ni instaló la app.
- No se ejecutaron las pruebas (por indicación del dueño).
- No se tocó `original_backup/` ni la copia de OneDrive.

Por eso este documento distingue **comandos verificados por el repositorio** de **resultados históricos**. Cualquier compilación nueva debe anotar fecha, comando y salida en `docs/pendientes_documentacion.md` o en el reporte correspondiente.

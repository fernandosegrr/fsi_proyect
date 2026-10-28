# Compilación, instalación y versión

El proyecto tiene productos distintos: firmware para la DevKit, firmware para la cámara, núcleo de pruebas para computadora y aplicación Android. Este capítulo indica cómo identificarlos y dónde consultar los procedimientos de compilación e instalación.

La compilación de las placas utiliza Arduino CLI y el soporte ESP32; la aplicación utiliza Gradle, Android SDK, NDK y CMake. El núcleo se compila con un compilador C y make. Las versiones y dependencias deben verificarse en los archivos de configuración antes de reproducir una compilación.

Desde la carpeta de la aplicación, `gradlew.bat assembleDebug` genera una APK de depuración. El script de carga permite compilar el firmware sin escribirlo en una placa mediante la opción `-SoloCompilar`. La instalación o carga sobre dispositivos físicos requiere preparar el equipo y verificar el puerto antes de ejecutarla.

El verificador de versiones compara las identificaciones disponibles con el repositorio y el artefacto. Una identificación de archivo o un resultado histórico no prueban qué versión está instalada actualmente en un dispositivo sin realizar la lectura correspondiente. El reporte J15 verificó la APK instalada en esa sesión; la lectura actual del firmware quedó pendiente en ese cierre.

## Requisitos registrados

La configuración Android declara SDK de compilación 37, herramientas de compilación 36.0.0, NDK 30.0.16248370 y CMake 4.1.2. El registro de compilación de las placas utiliza el núcleo ESP32 3.3.12. Los requisitos deben mantenerse acordes con los archivos de configuración de la revisión que se compile.

## Compilación y pruebas sin carga

Desde la raíz del repositorio, `./scripts/cargar.ps1 nuevo -SoloCompilar` compila el firmware del carro sin cargarlo. Para la cámara, `./scripts/cargar.ps1 camara -SoloCompilar` realiza la operación equivalente. En las carpetas del núcleo o de pruebas de firmware, `make test` ejecuta las suites correspondientes cuando están disponibles el compilador y make.

La opción `--solo-apk` del verificador limita la comprobación a la aplicación y deja pendiente la lectura del firmware. Cuando se dispone de acceso autorizado a los dispositivos, conviene registrar las versiones leídas y el resultado de la comparación. Los binarios con credenciales de red no deben publicarse.

Dónde está en el código: `cargar.ps1`, `verificar_version.py`, `build.gradle.kts`, `Makefile`.

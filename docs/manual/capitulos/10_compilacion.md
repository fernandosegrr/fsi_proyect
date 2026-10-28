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

## Identificar el producto antes de actuar

Una compilación transforma fuentes en un artefacto para una plataforma determinada. La aplicación de Android, el firmware del carro y el firmware de la cámara no son intercambiables: cada uno tiene dependencias, funciones y destino propios. El núcleo C permite ejecutar lógica en computadora y también se integra con la aplicación; una prueba de ese núcleo no carga ninguna placa.

Antes de iniciar, registre la revisión del repositorio y el resultado de `git status`. Si hay cambios locales, anótelos para saber si el resultado corresponde exactamente a un commit o a una mezcla con archivos modificados. No es necesario publicar archivos locales de configuración para conservar esa trazabilidad.

## Diferencia entre compilar y cargar

Compilar verifica que las herramientas puedan construir un programa con la configuración disponible. Cargar escribe firmware en una placa, por lo que sustituye el programa existente. La opción `-SoloCompilar` detiene el script antes de la identificación y carga de la placa; es apropiada para comprobar la construcción cuando no se pretende modificar hardware.

El script distingue los modos `baseline`, `nuevo` y `camara`. El modo nuevo usa la carpeta del firmware del carro y requiere la configuración local de secretos. El modo cámara corresponde a la placa con cámara. Es necesario seleccionar el modo que coincide con el dispositivo y comprobar el puerto antes de cargar.

Hay una particularidad importante: en los modos de identificación automática `baseline` y `camara`, una placa no reconocida puede recibir una sonda para identificarla; esa sonda sobrescribe el programa existente. El propio script describe que indicar un puerto conocido evita esa detección automática. Antes de una carga, conserve los respaldos necesarios y compruebe la identidad de la placa; no trate la detección como una operación siempre de solo lectura.

## Interpretación de errores de construcción

Un fallo por herramienta ausente debe resolverse instalando o configurando la dependencia indicada por el proyecto. Un fallo al descargar dependencias puede relacionarse con acceso de red o repositorios. Un error de compilación, en cambio, señala un problema al procesar fuentes o configuración y requiere leer el primer diagnóstico pertinente, no solo la última línea de salida.

Una construcción exitosa demuestra que se produjo un artefacto para esa configuración. No demuestra que se haya instalado, que la placa sea la correcta ni que la aplicación se comporte correctamente en el teléfono. Después de una instalación autorizada hay que comprobar la versión en el dispositivo y ejecutar pruebas apropiadas al cambio.

## Manejo de configuración sensible

Las credenciales pertenecen a la configuración local. Al compartir un registro, revise tanto la salida como los archivos adjuntos: una captura o un binario también pueden contener información sensible. El script intenta ocultar contraseñas conocidas en su salida, pero esa protección no sustituye la revisión de lo que se comparte.

Una dirección de red identifica un servicio dentro de la configuración descrita; no es una prueba de identidad del firmware. Del mismo modo, un nombre de archivo que contiene una versión no demuestra que el archivo se encuentre instalado en una placa. Para la trazabilidad se registran por separado revisión fuente, artefacto generado, dispositivo y lectura de versión disponible.

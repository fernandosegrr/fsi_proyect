# Instalar la app de rutas en el Huawei

La app (`app_android/`) es un esqueleto: dibujas una ruta en una cuadrícula y
el botón "Calcular" muestra la lista de pasos. Todavía no se conecta al carro.
No se probó en un teléfono; esta guía es el camino previsto.

## 0. Antes de empezar

- Un cable USB **de datos** (no solo de carga) y el Huawei con la pantalla desbloqueada.
- El APK compilado: `app_android/app/build/outputs/apk/debug/app-debug.apk`.
  Si no existe o quieres regenerarlo, ver la sección "Compilar el APK".
- Android del teléfono 8.0 o más nuevo (minSdk 26).
- Ojo: los Huawei con **HarmonyOS NEXT** (5.0 o superior) ya no ejecutan
  aplicaciones Android (APK). Si es tu caso, esta app no se puede instalar
  ahí y hay que usar otro teléfono Android. Los modelos con EMUI o con
  HarmonyOS 2/3/4 sí lo hacen.

## 1. Activar las opciones de desarrollador

1. Ajustes → **Acerca del teléfono**.
2. Toca 7 veces seguidas **Número de compilación**. Pide el PIN y avisa
   "Ya eres desarrollador".
3. Ajustes → **Sistema y actualizaciones** → **Opciones de desarrollador**.
4. Activa **Depuración USB**.
5. Si aparece, activa también **Permitir depuración ADB en modo solo carga**
   y **Instalar aplicaciones vía USB** (en Huawei a veces se llama
   "Instalación vía USB" o "Verificar aplicaciones vía USB": desactiva la
   verificación).

## 2. Conectar el teléfono al PC

1. Conecta el cable. En el teléfono elige el modo **Transferir archivos** (MTP).
2. Acepta el cuadro "¿Permitir depuración USB?" (marca "Permitir siempre desde
   este equipo").
3. En el PC abre PowerShell y comprueba que `adb` lo ve:

```powershell
& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" devices
```

Debe aparecer una línea con un código y la palabra `device`. Si dice
`unauthorized`, acepta el cuadro en el teléfono; si no aparece nada, cambia de
cable o de puerto USB, o instala HiSuite (el programa de Huawei trae el
controlador USB).

## 3. Instalar el APK

```powershell
& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r "C:\Users\guerr\OneDrive\Escritorio\fsi_proyecto\app_android\app\build\outputs\apk\debug\app-debug.apk"
```

`-r` reinstala sobre una versión anterior sin borrar nada. Debe terminar con
`Success`. La app aparece como **Carro: ruta**.

Errores comunes:
- `INSTALL_FAILED_USER_RESTRICTED`: activa "Instalar aplicaciones vía USB" en
  Opciones de desarrollador (paso 1.5) y vuelve a intentar.
- `INSTALL_FAILED_OLDER_SDK`: el teléfono tiene Android anterior al 8.0.
- `INSTALL_FAILED_UPDATE_INCOMPATIBLE`: ya hay una app con la misma firma
  distinta; desinstala la anterior (`adb uninstall com.fsi.carro`).
- `more than one device`: hay varios teléfonos o emuladores conectados;
  añade `-s <código>` después de `adb`.

## 4. Qué deberías ver

1. Una cuadrícula de 12 x 8 celdas y los botones **Calcular** y **Borrar**.
2. Al arrastrar el dedo se pintan las celdas: la primera es verde con una "I"
   y la última roja con una "M". Arriba de la pantalla es "adelante" del carro.
3. **Calcular** muestra la lista de pasos (AVANZAR en cm, GIRAR en grados y
   los milisegundos de cada uno). Los tiempos usan valores de ejemplo de
   `nucleo_c/calibracion.h`.
4. Un salto o un punto repetido muestra un mensaje de error en vez de pasos.

## 5. Sin cable (alternativa)

Copia `app-debug.apk` al teléfono (WhatsApp a ti mismo, Drive, USB), ábrelo
desde el administrador de archivos y permite "instalar apps de esta fuente"
cuando el teléfono lo pida.

## Compilar el APK (en el PC)

Requiere Android Studio (se usa su Java), el SDK de Android (plataforma 37,
NDK 30.0.16248370, CMake 4.1.2 y build-tools 36.0.0) e internet la primera vez
(Gradle baja unos 150 MB). Solo se define `JAVA_HOME` en esa sesión de
PowerShell:

```powershell
cd C:\Users\guerr\OneDrive\Escritorio\fsi_proyecto\app_android
```

```powershell
$env:JAVA_HOME = "C:\Program Files\Android\Android Studio\jbr"
```

```powershell
.\gradlew.bat assembleDebug
```

El APK queda en `app\build\outputs\apk\debug\app-debug.apk`.

# Descarga EfficientDet-Lite1 y Lite2 (con postproceso incluido) para el benchmark del modo "cualquier objeto".
# Estos archivos NO estan en el repositorio: se guardan en la carpeta modelos_extra\ (ignorada por git) y se copian
# al telefono con adb, a la carpeta modelos/ de los archivos de la app, donde la app los busca primero.
#
# Uso (desde la carpeta app_android, con el telefono conectado por USB):
#   powershell -ExecutionPolicy Bypass -File descargar_modelos_extra.ps1

$base    = "https://storage.googleapis.com/download.tensorflow.org/models/tflite/task_library/object_detection/android/"
$modelos = @(
    @{ nombre = "efficientdet_lite1.tflite"; origen = "lite-model_efficientdet_lite1_detection_metadata_1.tflite"; sha256 = "183afa66aad648b0dc16519ae1f55646d9cd38d2b544b917a9ab1da320ebed75" },
    @{ nombre = "efficientdet_lite2.tflite"; origen = "lite-model_efficientdet_lite2_detection_metadata_1.tflite"; sha256 = "6fd32c84ab1eb0f7e7f3a7a20a20d7df1530daa8378728f7c79571096286bd52" }
)
$carpeta = Join-Path $PSScriptRoot "modelos_extra"
New-Item -ItemType Directory -Force -Path $carpeta | Out-Null
$adb = Join-Path $env:LOCALAPPDATA "Android\Sdk\platform-tools\adb.exe"

foreach ($m in $modelos) {
    $destino = Join-Path $carpeta $m.nombre
    Invoke-WebRequest -Uri ($base + $m.origen) -OutFile $destino
    $obtenido = (Get-FileHash -Algorithm SHA256 $destino).Hash.ToLower()
    if ($obtenido -ne $m.sha256) {
        Remove-Item $destino
        throw "El hash de $($m.nombre) no coincide (obtenido $obtenido). Se borro el archivo."
    }
    Write-Host "Verificado: $destino"
    & $adb shell mkdir -p /sdcard/Android/data/com.fsi.carro/files/modelos
    & $adb push $destino "/sdcard/Android/data/com.fsi.carro/files/modelos/$($m.nombre)"
}

# Descarga el modelo EfficientDet-Lite0 (con postproceso incluido) de la fuente oficial
# de TensorFlow Lite y comprueba que sea el mismo archivo con el que se hicieron las mediciones.
#
# Fuente: ejemplos oficiales de TensorFlow Lite (github.com/tensorflow/examples,
# lite/examples/object_detection/android/app/download_models.gradle).
# El archivo ya viene en el repositorio (4.5 MB); este script sirve para volver a bajarlo.
#
# Uso (desde la carpeta app_android):
#   powershell -ExecutionPolicy Bypass -File descargar_modelo.ps1

$url      = "https://storage.googleapis.com/download.tensorflow.org/models/tflite/task_library/object_detection/android/lite-model_efficientdet_lite0_detection_metadata_1.tflite"
$destino  = Join-Path $PSScriptRoot "app\src\main\assets\efficientdet_lite0.tflite"
$sha256   = "2e04c53bfeac0ac2a30c057c7e2a777594ce39baaac35a92f74fb1e8c4fc4e0b"

New-Item -ItemType Directory -Force -Path (Split-Path $destino) | Out-Null
Invoke-WebRequest -Uri $url -OutFile $destino

$obtenido = (Get-FileHash -Algorithm SHA256 $destino).Hash.ToLower()
if ($obtenido -ne $sha256) {
    Remove-Item $destino
    throw "El hash no coincide (obtenido $obtenido). Se borro el archivo descargado."
}
Write-Host "Modelo descargado y verificado: $destino"

# compilar_app.ps1 — Compila la app (debug) y deja un resumen corto de errores y avisos.
#   .\scripts\compilar_app.ps1            compila
#   .\scripts\compilar_app.ps1 -Limpio    antes borra lo compilado (para ver todos los avisos)
# Usa el JDK de Android Studio solo en ESTA ventana (no toca JAVA_HOME global).
param([switch]$Limpio)
$raiz = Split-Path -Parent $PSScriptRoot
Set-Location (Join-Path $raiz "app_android")
$env:JAVA_HOME = "C:\Program Files\Android\Android Studio\jbr"
$salida = Join-Path $env:TEMP "gradle_out.txt"
if ($Limpio) { .\gradlew.bat clean --console=plain > $null 2>&1 }
.\gradlew.bat assembleDebug --console=plain > $salida 2>&1
$codigo = $LASTEXITCODE
Get-Content $salida | Select-String -Pattern "^e: |^w: |warning:|error:|FAILED|BUILD " -Context 0,1 | Select-Object -First 60 | ForEach-Object { $_.Line; if ($_.Line -match "^e: file.*\.kt:$") { $_.Context.PostContext } }
if ($codigo -eq 0) { "COMPILACION OK" } else { "COMPILACION FALLIDA (codigo $codigo)" }
exit $codigo

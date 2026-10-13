<#
.SYNOPSIS
  Compila y carga un firmware en la placa correcta por USB (sin tocar la red WiFi de la PC).

.DESCRIPTION
  GUIA DE LECTURA
   1. Modos:
        baseline : firmware ORIGINAL (copia temporal de original_backup/) -> ESP32 DevKit (sin PSRAM)
        nuevo    : firmware_carro/ (necesita firmware_carro/secretos.h)    -> ESP32 DevKit (sin PSRAM)
        camara   : camara_prueba/ (se crea en H2)                          -> Freenove ESP32-WROVER (con PSRAM)
   2. Primero COMPILA (en una carpeta temporal; el repositorio no se ensucia) y despues busca
      la placa. Con -SoloCompilar se detiene ahi.
   3. Cada puerto COM se consulta con esptool (chip-id, no destructivo). La placa con PSRAM es la
      Freenove y la que no la tiene es la DevKit. Como esptool no puede ver la PSRAM, la primera
      vez que ve una placa (por su MAC) carga scripts/sonda_placa (un programa minimo) y lee lo que
      imprime. ESO SOBRESCRIBE el programa que tuviera esa placa, una sola vez. La MAC y su rol
      se guardan en scripts/placas.local.json (no se sube a git). Con -Puerto COMx se salta toda
      la deteccion (usalo si sabes cual es).
   4. Sube con arduino-cli; si falla reintenta a menor velocidad (921600 -> 512000 -> 115200). Si
      aun falla imprime que hacer con el boton BOOT; con -EsperarBoot hace un ultimo intento a
      115200 despues de una cuenta regresiva en la que tu mantienes BOOT presionado.
   5. Despues de subir, abre el monitor serie a 115200 durante -MonitorSegundos (15 por defecto)
      y muestra lo que imprime la placa. Nunca imprime contrasenas: las oculta de toda salida.
   6. No usa ni cambia la conexion WiFi de la PC.

.PARAMETER Modo
  baseline | nuevo | camara

.EXAMPLE
  .\scripts\cargar.ps1 baseline
  .\scripts\cargar.ps1 nuevo -SoloCompilar
  .\scripts\cargar.ps1 camara -Puerto COM5
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet('baseline', 'nuevo', 'camara')]
    [string]$Modo,

    [string]$Puerto = '',
    [int]$MonitorSegundos = 15,
    [switch]$SoloCompilar,
    [switch]$EsperarBoot
)

$ErrorActionPreference = 'Continue'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$raiz = Split-Path -Parent $PSScriptRoot
$carpetaTemporal = Join-Path $env:TEMP 'fsi_cargar'
$archivoPlacas = Join-Path $PSScriptRoot 'placas.local.json'

# ---------------------------------------------------------------------------
# Utilidades de mensajes y secretos
# ---------------------------------------------------------------------------

# Contrasenas conocidas (se leen de los archivos locales, nunca se imprimen).
$script:secretos = @()

function Cargar-Secretos {
    $fuentes = @(
        @{ Ruta = (Join-Path $raiz 'firmware_carro\secretos.h'); Patron = 'WIFI_PASSWORD\s+"([^"]*)"' },
        @{ Ruta = (Join-Path $raiz 'original_backup\codigo_Carrito.ino'); Patron = 'password\s*=\s*"([^"]*)"' }
    )
    foreach ($f in $fuentes) {
        if (Test-Path $f.Ruta) {
            $texto = Get-Content -Raw -LiteralPath $f.Ruta
            $m = [regex]::Match($texto, $f.Patron)
            if ($m.Success -and $m.Groups[1].Value.Length -ge 4) {
                $script:secretos += $m.Groups[1].Value
            }
        }
    }
}

function Ocultar([string]$texto) {
    foreach ($s in $script:secretos) {
        $texto = $texto.Replace($s, '********')
    }
    return $texto
}

function Decir([string]$texto) { Write-Host (Ocultar $texto) }
function Fallar([string]$texto, [int]$codigo = 1) {
    Write-Host (Ocultar ("ERROR: " + $texto)) -ForegroundColor Red
    exit $codigo
}

# Ejecuta un programa y devuelve @{ Codigo; Salida } ocultando contrasenas. Si $mostrar, imprime en vivo.
function Ejecutar([string]$exe, [string[]]$argumentos, [bool]$mostrar = $false) {
    $lineas = New-Object System.Collections.Generic.List[string]
    & $exe @argumentos 2>&1 | ForEach-Object {
        $linea = Ocultar ("$_")
        $lineas.Add($linea)
        if ($mostrar) { Write-Host $linea }
    }
    return @{ Codigo = $LASTEXITCODE; Salida = ($lineas -join "`n") }
}

# ---------------------------------------------------------------------------
# Herramientas
# ---------------------------------------------------------------------------

function Buscar-ArduinoCli {
    $cmd = Get-Command arduino-cli -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $candidato = 'C:\Program Files\Arduino CLI\arduino-cli.exe'
    if (Test-Path $candidato) { return $candidato }
    Fallar 'No se encontro arduino-cli. Instalalo (https://arduino.github.io/arduino-cli/) y el core esp32:esp32.'
}

function Buscar-Esptool {
    $base = Join-Path $env:LOCALAPPDATA 'Arduino15\packages\esp32\tools\esptool_py'
    $exe = Get-ChildItem -Path $base -Filter esptool.exe -Recurse -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1
    if (-not $exe) {
        Fallar "No se encontro esptool en $base. Instala el core esp32:esp32 con: arduino-cli core install esp32:esp32"
    }
    return $exe.FullName
}

$arduino = Buscar-ArduinoCli
$esptool = Buscar-Esptool
Cargar-Secretos

# ---------------------------------------------------------------------------
# Que se compila y a que placa va cada modo
# ---------------------------------------------------------------------------

$rolObjetivo = if ($Modo -eq 'camara') { 'freenove' } else { 'devkit' }
$fqbn = if ($rolObjetivo -eq 'freenove') { 'esp32:esp32:esp32wrover' } else { 'esp32:esp32:esp32' }
$carpetaCompilacion = Join-Path $carpetaTemporal "build_$Modo"
$ssidMostrado = ''

function Preparar-Sketch {
    switch ($Modo) {
        'baseline' {
            $origen = Join-Path $raiz 'original_backup'
            if (-not (Test-Path (Join-Path $origen 'codigo_Carrito.ino'))) {
                Fallar 'No existe original_backup\codigo_Carrito.ino.'
            }
            # Copia temporal FUERA del repositorio; original_backup no se modifica.
            $destino = Join-Path $carpetaTemporal 'baseline\codigo_Carrito'
            if (Test-Path $destino) { Remove-Item -Recurse -Force $destino }
            New-Item -ItemType Directory -Force -Path $destino | Out-Null
            Copy-Item (Join-Path $origen 'codigo_Carrito.ino') $destino
            Copy-Item (Join-Path $origen 'pagina.h') $destino
            $m = [regex]::Match((Get-Content -Raw -LiteralPath (Join-Path $destino 'codigo_Carrito.ino')), 'ssid\s*=\s*"([^"]*)"')
            if ($m.Success) { $script:ssidMostrado = $m.Groups[1].Value }
            return $destino
        }
        'nuevo' {
            $sketch = Join-Path $raiz 'firmware_carro'
            $secretos = Join-Path $sketch 'secretos.h'
            if (-not (Test-Path $secretos)) {
                Fallar 'Falta firmware_carro\secretos.h (el nombre y la contrasena del WiFi). Copia secretos.ejemplo.h a secretos.h y editalo.'
            }
            $m = [regex]::Match((Get-Content -Raw -LiteralPath $secretos), 'WIFI_SSID\s+"([^"]*)"')
            if ($m.Success) { $script:ssidMostrado = $m.Groups[1].Value }
            return $sketch
        }
        'camara' {
            $sketch = Join-Path $raiz 'camara_prueba'
            if (-not (Test-Path $sketch)) {
                Fallar 'No existe camara_prueba\ (se crea en el trabajo H2). Nada que cargar todavia.'
            }
            return $sketch
        }
    }
}

function Compilar([string]$sketch, [string]$fqbnCompilar, [string]$salida) {
    $r = Ejecutar $arduino @('compile', '--fqbn', $fqbnCompilar, '--build-path', $salida, $sketch) $false
    if ($r.Codigo -ne 0) {
        Decir $r.Salida
        Fallar "La compilacion fallo (modo $Modo)."
    }
    $resumen = ($r.Salida -split "`n" | Where-Object { $_ -match 'El Sketch usa|Sketch uses' }) -join ' '
    if ($resumen) { Decir $resumen }
}

# ---------------------------------------------------------------------------
# Puertos y placas
# ---------------------------------------------------------------------------

# Lista de puertos COM con la descripcion de Windows y el chip USB-serie.
function Listar-Puertos {
    $lista = @()
    $dispositivos = Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '\((COM\d+)\)' }
    foreach ($d in $dispositivos) {
        $null = $d.Name -match '\((COM\d+)\)'
        $com = $Matches[1]
        $chip = 'desconocido'
        if ($d.PNPDeviceID -match 'VID_10C4') { $chip = 'CP210x (Silicon Labs)' }
        elseif ($d.PNPDeviceID -match 'VID_1A86') { $chip = 'CH340/CH9102 (WCH)' }
        elseif ($d.PNPDeviceID -match 'VID_0403') { $chip = 'FTDI' }
        elseif ($d.PNPDeviceID -match 'VID_303A') { $chip = 'USB nativo Espressif' }
        $lista += [pscustomobject]@{ Puerto = $com; Nombre = $d.Name; Chip = $chip; Id = $d.PNPDeviceID }
    }
    return $lista
}

# Si no hay puertos, ayuda a ver si falta un driver.
function Diagnostico-Usb {
    Decir 'No hay ningun puerto COM. Revisa:'
    Decir '  - que el cable USB sea de DATOS (no solo de carga) y la placa este conectada;'
    $raros = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
        Where-Object { $_.InstanceId -match 'VID_10C4|VID_1A86|VID_0403|VID_303A' }
    if ($raros) {
        foreach ($d in $raros) {
            Decir ("  - dispositivo USB serie visto: {0} (estado {1}, {2})" -f $d.FriendlyName, $d.Status, $d.InstanceId)
        }
        Decir '  - si el estado no es OK falta el driver: CP210x (Silicon Labs) o CH340/CH9102 (WCH).'
        Decir '    Descargalo SOLO de la pagina oficial del fabricante del chip (ver docs/hardware_cargar.md).'
    } else {
        Decir '  - Windows no ve ningun chip USB-serie conocido (CP210x, CH340, FTDI): prueba otro cable u otro puerto USB.'
    }
}

# Consulta no destructiva del chip con esptool. Devuelve $null si no responde.
function Consultar-Placa([string]$com) {
    $r = Ejecutar $esptool @('--port', $com, '--baud', '115200', '--before', 'default-reset', '--after', 'hard-reset', 'chip-id') $false
    if ($r.Codigo -ne 0 -or $r.Salida -notmatch 'Chip type:\s*(.+)') { return $null }
    $tipo = [regex]::Match($r.Salida, 'Chip type:\s*(.+)').Groups[1].Value.Trim()
    $mac = [regex]::Match($r.Salida, 'MAC:\s*([0-9a-fA-F:]{17})').Groups[1].Value.ToLower()
    $r2 = Ejecutar $esptool @('--port', $com, '--baud', '115200', '--before', 'default-reset', '--after', 'hard-reset', 'flash-id') $false
    $flash = [regex]::Match($r2.Salida, 'Detected flash size:\s*(\S+)').Groups[1].Value
    $caract = [regex]::Match($r.Salida + "`n" + $r2.Salida, 'Features:\s*(.+)').Groups[1].Value.Trim()
    return [pscustomobject]@{ Tipo = $tipo; Mac = $mac; Flash = $flash; Caracteristicas = $caract }
}

function Leer-Placas {
    if (Test-Path $archivoPlacas) {
        try {
            $o = Get-Content -Raw -LiteralPath $archivoPlacas | ConvertFrom-Json
            $h = @{}
            foreach ($p in $o.PSObject.Properties) { $h[$p.Name] = [string]$p.Value }
            return $h
        } catch { }
    }
    return @{}
}

function Guardar-Placas($tabla) {
    $o = [ordered]@{}
    foreach ($k in $tabla.Keys) { $o[$k] = $tabla[$k] }
    ($o | ConvertTo-Json) | Set-Content -LiteralPath $archivoPlacas -Encoding UTF8
}

# Lee el puerto serie $segundos y devuelve las lineas (reinicia la placa con RTS).
function Leer-Serie([string]$com, [int]$segundos, [bool]$mostrar) {
    $lineas = New-Object System.Collections.Generic.List[string]
    $sp = New-Object System.IO.Ports.SerialPort $com, 115200, ([System.IO.Ports.Parity]::None), 8, ([System.IO.Ports.StopBits]::One)
    $sp.ReadTimeout = 200
    $sp.DtrEnable = $false
    $sp.RtsEnable = $false
    try {
        $sp.Open()
        # Pulso de reinicio (EN): para que se vea todo el arranque.
        $sp.RtsEnable = $true
        Start-Sleep -Milliseconds 150
        $sp.RtsEnable = $false
        $fin = (Get-Date).AddSeconds($segundos)
        $resto = ''
        $anterior = $null
        $repetidas = 0
        while ((Get-Date) -lt $fin) {
            try {
                $trozo = $sp.ReadExisting()
            } catch { $trozo = '' }
            if ($trozo) {
                $resto += $trozo
                while ($resto.Contains("`n")) {
                    $i = $resto.IndexOf("`n")
                    $l = $resto.Substring(0, $i).TrimEnd("`r")
                    $resto = $resto.Substring($i + 1)
                    $l = Ocultar $l
                    if ($l -eq $anterior) {
                        # Una linea que se repite sin parar (un error en bucle) se resume en una sola.
                        $repetidas++
                    } else {
                        if ($repetidas -gt 0 -and $mostrar) { Write-Host ("  | (la linea anterior se repitio {0} veces mas)" -f $repetidas) }
                        $repetidas = 0
                        $anterior = $l
                        $lineas.Add($l)
                        if ($mostrar) { Write-Host ("  | " + $l) }
                    }
                }
            } else {
                Start-Sleep -Milliseconds 100
            }
        }
        if ($repetidas -gt 0 -and $mostrar) { Write-Host ("  | (la linea anterior se repitio {0} veces mas)" -f $repetidas) }
        if ($resto.Trim()) {
            $l = Ocultar $resto.Trim()
            $lineas.Add($l)
            if ($mostrar) { Write-Host ("  | " + $l) }
        }
    } catch {
        Decir ("No se pudo leer el puerto {0}: {1}" -f $com, $_.Exception.Message)
    } finally {
        if ($sp.IsOpen) { $sp.Close() }
    }
    return $lineas
}

# Carga el programa sonda y decide si la placa tiene PSRAM. Devuelve 'freenove', 'devkit' o $null.
function Sondear-Placa([string]$com) {
    Decir ("  Placa nueva en {0}: cargando la sonda de PSRAM (sobrescribe el programa que tuviera, una sola vez)..." -f $com)
    $sonda = Join-Path $PSScriptRoot 'sonda_placa'
    $salida = Join-Path $carpetaTemporal 'build_sonda'
    Compilar $sonda 'esp32:esp32:esp32wrover' $salida
    $ok = Subir $com 'esp32:esp32:esp32wrover' $salida $false
    if (-not $ok) { return $null }
    Start-Sleep -Seconds 2
    $lineas = Leer-Serie $com 8 $false
    $texto = $lineas -join "`n"
    if ($texto -match 'psram=SI') { return 'freenove' }
    if ($texto -match 'psram=NO|PSRAM ID read error|PSRAM chip not found|psram.*(fail|error)') { return 'devkit' }
    return $null
}

# ---------------------------------------------------------------------------
# Subida con reintentos
# ---------------------------------------------------------------------------

function Subir([string]$com, [string]$fqbnSubir, [string]$carpetaBuild, [bool]$conBoot) {
    $velocidades = @(921600, 512000, 115200)   # las que ofrece el core (460800 no esta en su menu)
    if ($conBoot) { $velocidades = @(115200) }
    foreach ($v in $velocidades) {
        Decir ("  Subiendo a {0} a {1} baudios..." -f $com, $v)
        $r = Ejecutar $arduino @('upload', '--fqbn', ("{0}:UploadSpeed={1}" -f $fqbnSubir, $v), '-p', $com, '--input-dir', $carpetaBuild) $false
        if ($r.Codigo -eq 0) {
            Decir '  Subida correcta.'
            return $true
        }
        $ultimas = ($r.Salida -split "`n" | Select-Object -Last 4) -join "`n    "
        Decir ("  Fallo a {0} baudios:`n    {1}" -f $v, $ultimas)
    }
    return $false
}

function Instrucciones-Boot([string]$com) {
    Decir ''
    Decir 'La placa no entro en modo de carga automaticamente. Hazlo a mano:'
    Decir '  1. Deja la placa conectada por USB.'
    Decir '  2. Ejecuta de nuevo este mismo comando agregando -EsperarBoot.'
    Decir '  3. Cuando el script diga "MANTEN PRESIONADO BOOT", presiona y no sueltes el boton BOOT (IO0).'
    Decir '  4. Suelta BOOT en cuanto aparezca "Writing at" / "Escribiendo" (si la placa tiene boton EN, pulsalo una vez antes).'
    Decir 'Si sigue sin entrar: otro cable USB de datos, otro puerto USB, y revisa el driver (docs/hardware_cargar.md).'
}

# ---------------------------------------------------------------------------
# Programa principal
# ---------------------------------------------------------------------------

New-Item -ItemType Directory -Force -Path $carpetaTemporal | Out-Null
Decir ("== cargar.ps1 modo={0} -> placa {1} ({2}) ==" -f $Modo, $rolObjetivo, $fqbn)

$sketch = Preparar-Sketch
Decir ("Compilando {0} ..." -f (Split-Path $sketch -Leaf))
Compilar $sketch $fqbn $carpetaCompilacion
Decir 'Compilacion correcta.'
if ($SoloCompilar) {
    Decir 'Listo (-SoloCompilar): no se busco ninguna placa ni se subio nada.'
    exit 0
}

# Elegir el puerto
if ($Puerto) {
    $com = $Puerto.ToUpper()
    Decir ("Puerto indicado a mano: {0} (no se comprueba el tipo de placa)." -f $com)
} else {
    $puertos = Listar-Puertos
    if (-not $puertos -or $puertos.Count -eq 0) {
        Diagnostico-Usb
        Fallar 'No se encontro ninguna placa por USB.' 2
    }
    Decir 'Puertos encontrados:'
    $conocidas = Leer-Placas
    $elegidos = @()
    foreach ($p in $puertos) {
        $info = Consultar-Placa $p.Puerto
        if (-not $info) {
            Decir ("  {0}: {1} [{2}] - no responde como ESP32 (se ignora)" -f $p.Puerto, $p.Nombre, $p.Chip)
            continue
        }
        $rol = $conocidas[$info.Mac]
        if (-not $rol) {
            $rol = Sondear-Placa $p.Puerto
            if ($rol) {
                $conocidas[$info.Mac] = $rol
                Guardar-Placas $conocidas
            }
        }
        if (-not $rol) { $rol = 'sin identificar' }
        Decir ("  {0}: {1} [{2}] chip {3}, flash {4}, MAC {5} -> {6}" -f $p.Puerto, $p.Nombre, $p.Chip, $info.Tipo, $info.Flash, $info.Mac, $rol)
        if ($rol -eq $rolObjetivo) { $elegidos += $p.Puerto }
    }
    if ($elegidos.Count -eq 0) {
        Fallar ("No hay ninguna placa identificada como {0}. Revisa las conexiones o usa -Puerto COMx." -f $rolObjetivo) 3
    }
    if ($elegidos.Count -gt 1) {
        Fallar ("Hay mas de una placa {0} ({1}). Usa -Puerto COMx para elegir." -f $rolObjetivo, ($elegidos -join ', ')) 3
    }
    $com = $elegidos[0]
    Decir ("Placa elegida: {0} ({1})" -f $com, $rolObjetivo)
}

# Subir
$subido = Subir $com $fqbn $carpetaCompilacion $false
if (-not $subido -and $EsperarBoot) {
    Decir ''
    Decir 'MANTEN PRESIONADO el boton BOOT ahora. Intento en:'
    5..1 | ForEach-Object { Decir ("  {0}..." -f $_); Start-Sleep -Seconds 1 }
    Decir 'Intentando (suelta BOOT cuando veas que escribe)...'
    $subido = Subir $com $fqbn $carpetaCompilacion $true
}
if (-not $subido) {
    Instrucciones-Boot $com
    Fallar 'No se pudo subir el firmware.' 4
}

# Monitor serie
if ($MonitorSegundos -gt 0) {
    Decir ("Monitor serie a 115200 durante {0} s (lo que imprime la placa):" -f $MonitorSegundos)
    $lineas = Leer-Serie $com $MonitorSegundos $true
    if ($ssidMostrado) { Decir ("Red WiFi del carro (nombre): {0}" -f $ssidMostrado) }
    $ip = $lineas | Where-Object { $_ -match '\d+\.\d+\.\d+\.\d+' } | Select-Object -First 1
    if ($ip) { Decir ("Linea con la IP: {0}" -f $ip) }
    if ($lineas.Count -eq 0) { Decir 'La placa no imprimio nada en ese tiempo.' }
}
Decir 'Listo.'
exit 0

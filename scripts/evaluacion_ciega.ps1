param(
    [Parameter(Mandatory = $true)]
    [uint64]$Semilla,
    [int]$Corridas = 120,
    [string]$Ruido = "0,1",
    [switch]$V5
)

# Evaluacion ciega con semilla externa. SIMULADO / NO MEDIDO EN EL CARRO.
#   .\scripts\evaluacion_ciega.ps1 -Semilla <uint64>          v3 contra v5-carbon-fix contra v6 (lo normal desde v6)
#   .\scripts\evaluacion_ciega.ps1 -Semilla <uint64> -V5      la comparacion historica de v5 (v3 contra v5-carbon, sin ruido perceptivo)
$ErrorActionPreference = 'Stop'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if ($V5) {
    python (Join-Path $scriptDir 'evaluacion_ciega.py') --semilla $Semilla --corridas $Corridas
} else {
    python (Join-Path $scriptDir 'evaluacion_ciega_v6.py') --semilla $Semilla --corridas $Corridas --ruido $Ruido
}
exit $LASTEXITCODE

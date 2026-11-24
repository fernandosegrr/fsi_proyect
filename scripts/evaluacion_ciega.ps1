param(
    [Parameter(Mandatory = $true)]
    [uint64]$Semilla,
    [int]$Corridas = 40
)

$ErrorActionPreference = 'Stop'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
python (Join-Path $scriptDir 'evaluacion_ciega.py') --semilla $Semilla --corridas $Corridas
exit $LASTEXITCODE

#!/usr/bin/env bash
# Evaluacion ciega con semilla externa (SIMULADO / NO MEDIDO EN EL CARRO).
#   scripts/evaluacion_ciega.sh <semilla_uint64> [corridas] [ruido]      v3 contra v5-carbon-fix contra v6
#   V5=1 scripts/evaluacion_ciega.sh <semilla_uint64> [corridas]         la comparacion historica de v5
set -euo pipefail
if [ "$#" -lt 1 ]; then
    echo "Uso: $0 <semilla_uint64> [corridas] [ruido]" >&2
    exit 2
fi
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ "${V5:-0}" = "1" ]; then
    python3 "$SCRIPT_DIR/evaluacion_ciega.py" --semilla "$1" --corridas "${2:-40}"
else
    python3 "$SCRIPT_DIR/evaluacion_ciega_v6.py" --semilla "$1" --corridas "${2:-120}" --ruido "${3:-0,1}"
fi

#!/usr/bin/env bash
set -euo pipefail
if [ "$#" -lt 1 ]; then
    echo "Uso: $0 <semilla_uint64> [corridas]" >&2
    exit 2
fi
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
python3 "$SCRIPT_DIR/evaluacion_ciega.py" --semilla "$1" --corridas "${2:-40}"

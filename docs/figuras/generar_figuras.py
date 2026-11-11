#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
generar_figuras.py — Genera todas las figuras y láminas del Manual Técnico v1.1.

Uso:
    python docs/figuras/generar_figuras.py
"""
from pathlib import Path
import sys

AQUI = Path(__file__).resolve().parent
sys.path.insert(0, str(AQUI))
import generar_figuras_v2  # noqa: E402


def main():
    generar_figuras_v2.main()


if __name__ == "__main__":
    main()

# Progreso autonomo v5-carbon (`feat/autonomo-v5-carbon`)

Repositorio usado: `E:\Dev\fsi_proyecto` (`D:\Dev\fsi_proyecto` no existe).  
Rama base: `feat/autonomo-v3` (`e37f77b`). Identidad de commits: `fernandosegrr <223138626+fernandosegrr@users.noreply.github.com>`.

## Fase 0 — Inventario, linea base y simulador calibrado (Completada)

### Que se hizo
- Creacion de la rama `feat/autonomo-v5-carbon` desde `e37f77b`.
- Actualizacion de `.gitignore` para excluir `cajas_negras/`, `docs/pruebas_fisicas_20261114.csv`, `docs/pruebas_fisicas_20261123.csv` y el cache temporal `docs/precision/v5/base_v3_calibrada.json`.
- Ejecucion integra de la suite base antes de editar codigo C/Python (`docs/precision/v5/fase0_verificacion.txt`, `fase0_banco_heredado_v3.txt`, `fase0_mc_precision.txt`):
  * `make -C nucleo_c test`: OK (`EXIT=0`).
  * `make -C pruebas_firmware test`: OK (`EXIT=0`, `verificar_pagina.py` OK).
  * `probar_servidor_falso.py`: 60 casos, 0 fallos (`EXIT=0`).
  * `probar_version.py`: OK (`EXIT=0`); `verificar_enlaces.py`: 155 archivos, 186 enlaces validos (`EXIT=0`).
  * Banco heredado v3 (`pruebas_autonomo_v2.exe`, N=40): 1 contacto en `silla_patas`, 1 contacto y 13 AYUDA en `caja_y_pared_al_lado` (`exit=1`).
  * `compuertas_autonomo.py --corridas 200 --solo actual --sin-guardar`: 12 fallos (paradas en `falso_sol`, `falso_sombra`, `falso_reflejo`; contactos/AYUDA en `banquito35x45`, `silla_patas`, `caja_y_pared_al_lado`).
  * `comparar_esquives.py --corridas 1000`: 4 fallos estrictos de +1 mm con RUMBO (`17/40 -> 18/44` mm).
- Analisis de los registros fisicos locales (`docs/pruebas_fisicas_20261114.csv` y 7 cajas negras `cn_20261115_*.txt`): velocidad real con cinta **25.7 a 29.4 cm/s** (4 corridas), cadencia `VTEL` med 69 ms / p95 147 ms, intervalo `SENS` inferido med 100 ms / p95 174 ms, y falsos disparos frecuentes de vision aislada en piso limpio (`fv!=0` en 280/518 ticks, `VISTAZO_DIAG` en 6/7 corridas).
- Calibracion del simulador en `nucleo_c/mundo_v2.h`, `nucleo_c/mundo_v2.c` y `nucleo_c/pruebas_autonomo_v2.c`, mas creacion de `scripts/compuertas_v5.py` y `docs/precision/calibracion_v5.md`.
- Comprobado que:
  1. Sin variables `AV5_*`, `pruebas_autonomo_v2.exe` reproduce exactamente (`True`, 24 vs 24 lineas) el banco heredado v3.
  2. Compilando `e37f77b` contra el mismo arnes calibrado, `compuertas_v5.py --apagado` da igualdad exacta en las 4 matrices (`DESARROLLO`/`RESERVADO` x `normal`/`pesimista`).

### Linea base de v3 (`e37f77b`) en el banco calibrado v5 (SIMULADO, N=40) — `docs/precision/v5/fase0_linea_base.txt`
- **G1 (piso limpio y falsos):** CUMPLE frente a si mismo (`0` AYUDA y `0` esquivas en piso limpio; paradas en `falso_sol`: 3 dev / 2 res, `falso_sombra`: 4 / 1, `falso_reflejo`: 1 / 5).
- **G2 (solidos y `objeto_bajo`):** NO CUMPLE (`caja20_lateral` 1 contacto en `desarrollo_pesimista`, `objeto_bajo` 19 contactos `desarrollo_pesimista` y 14 `reservado_pesimista`, `pared_al_frente` 1 contacto en `reservado_normal`, `objeto_largo90` 2 contactos en `reservado_pesimista`).
- **G3 (`bloque_sin_eco`):** NO CUMPLE (v3: 7/40 contactos en `desarrollo`, 16/40 en `reservado_normal`, 19/40 en `reservado_pesimista`).
- **G4 (muebles de patas finas):** NO CUMPLE (`banquito35x45`: 15–25 contactos dev, 7–11 res + 5–12 AYUDA; `silla_patas`: 4–12 contactos + 22–33 `bajo_mueble`; `dos_muebles`: 6–13 contactos + 6 AYUDA; `mueble_con_pared`: 11–21 contactos + 14–20 AYUDA).
- **G5 (`pasillo`, `pasillo_angosto`, `caja_y_pared_al_lado`):** NO CUMPLE (`caja_y_pared_al_lado` 13–18 AYUDA; `pasillo` 14–18 AYUDA dev / 3–4 res; `pasillo_angosto` 5 AYUDA res).
- **G6 (`terminado_falso` con 4 esquinas):** NO CUMPLE (`terminado_falso = 1` en 4 escenarios de estres/muebles).
- **G7 (sin regresion en otras metricas):** CUMPLE en linea base (`error=0`, `sin_terminar=0`).

### Siguiente
- **Fase 1:** Extraer trazas (`depurar`) de v3 en `DESARROLLO` sobre los modos de falla anteriores y redactar `docs/autonomo_v5_diseno.md`.

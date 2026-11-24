# Progreso autonomo v5-carbon (`feat/autonomo-v5-carbon`)

Repositorio usado: `E:\Dev\fsi_proyecto` (`D:\Dev\fsi_proyecto` no existe).  
Rama base: `feat/autonomo-v3` (`e37f77b`). Identidad de commits: `fernandosegrr <223138626+fernandosegrr@users.noreply.github.com>`.

## Fase 0 — Inventario, linea base y simulador calibrado (Completada)
- Rama `feat/autonomo-v5-carbon` creada desde `e37f77b`, `.gitignore` actualizado y commit `817a844` generado tras verificar toda la suite base sin regresiones.
- Validada igualdad exacta `AV5_APAGADO=1` vs `e37f77b` en modo heredado (24/24 lineas) y en el banco calibrado de 4 matrices (`DESARROLLO`/`RESERVADO` x `normal`/`pesimista`).

## Fase 1 — Diagnostico de fallas reales/simuladas y diseno v5 (Completada)

### Que se hizo
- Ajuste realista de `bloque_sin_eco` (`E_SIN_ECO`) como bloque oscuro/mate/transparente con reflectancia ~0 y `recall` bajo de clase fuera de COCO (0.05 en `DESARROLLO`, 0.03–0.08 en `RESERVADO`), reproduciendo exactamente el hueco reportado en `v3`: **40/40 contactos en las 4 matrices** (`docs/precision/v5/fase0_linea_base.txt` recalculado con `--recalcular-v3`).
- Extraccion de trazas (`AV2_TODO=1 depurar`) sobre corridas fallidas de `v3` en `DESARROLLO` guardadas en `docs/precision/v5/fase1_trazas_v3.txt`.
- Hallazgos raiz verificados:
  1. En `bloque_sin_eco`, color si detecta el bloque desde `[900..2550 ms]` hasta `cercania=3` (`80 celdas`), pero `fusion.c` lo descarta al faltar la caja neuronal; ademas AEC eleva ganancia cuando el bloque oscuro tapa el cuadro (`[2750 ms]`), borrando despues el contraste de color.
  2. En `objeto_bajo` pesimista, una perdida momentanea de la caja neuronal en `DETENIDO_OBSTACULO` reanuda `FORWARD` contra el obstaculo, y un giro a `cercania >= 2` sin retroceso pivota dentro de la huella del obstaculo.
  3. En `banquito35x45` y `silla_patas`, `vistazo` en `cercania >= 2` tuerce al carro bajo el mueble, la erosion borra patas verticales de 1–2 columnas, el escaneo ToF pasa entre patas (`ancho_mm=0`) y un error de longitud a velocidad real deja la esquina trasera a `-15 mm` (`terminado_falso = 1`, G6).
  4. En `caja_y_pared_al_lado` y `pasillo`, `v3` elige el lado obstruido y aborta por `AYUDA` 16/18 sin deshacer el giro ni probar el otro lado (pese a registrar `otro_lado_libre=1`) y sin aprovechar el pivote izquierdo mas corto (~90 mm vs ~158 mm derecho).
- Documentado el diseno completo por etapas 1–5 en `docs/autonomo_v5_diseno.md` con respaldo bibliografico primario auditado en `docs/investigacion_autonomo/`.

### Siguiente
- **Fase 2, Etapa 1 (`v5_coincidencia_estable`)**: Completada en el siguiente bloque.

## Fase 2 — Implementacion gradual por etapas

### Etapa 1: `v5_coincidencia_estable` (SIMULADO)
- **Que se hizo**:
  - Agregada la estructura de banderas v5 en `nucleo_c/navegador.h` y `nucleo_c/navegador.c` (`v5_activo`, `v5_coincidencia_estable`, `v5_masa_sin_eco`, `v5_patas_delgadas`, `v5_pasillo_pared`, `v5_fallo_seguro`) con control maestro `navegador_poner_v5()` y `navegador_poner_coincidencia_estable()`.
  - En `DETENIDO_OBSTACULO`, si `fuentes == 3` ya confirmo obstaculo en `ZONA_CENTRO` y el color cercano (`cercania >= 2`) persiste dentro del corredor sin bloquear flancos, se retiene `hay = 1` en centro hasta 500 ms (`NAV_V5_RETENCION_MS`) ante parpadeos de 1 cuadro de la red neuronal; se limpia `v5_coinc_hasta_ms` al escapar, reanudar o iniciar esquive.
  - Desempate en `elegir_lado` cuando al menos un flanco tiene `cercania <= 1` (usando `-vistazo_confirmado` o menor conteo de celdas, preservando el caso `LADOS_BLOQUEADOS` de `cercania = 3`).
  - Banco `pruebas_autonomo_v2.c`: soporte de seleccion de banderas y comprobacion exacta con `AV5_APAGADO=1`.
- **Resultados verificados (SIMULADO, 40 corridas)**:
  - `make -C nucleo_c test`: 0 fallos en toda la suite.
  - `python scripts/compuertas_v5.py --corridas 40 --apagado`: igualdad exacta `SI`.
  - `docs/precision/v5/etapa1.txt`: G1 `CUMPLE`, G7 `CUMPLE` (0 paradas/esquives/AYUDA nuevos en piso limpio en `DESARROLLO` y `RESERVADO`). En `objeto_bajo` pesimista los contactos bajaron de 19 a 13 (`DESARROLLO`) y de 14 a 10 (`RESERVADO`) con `ayuda=0`; `bloque_sin_eco` normal paso de 40 a 39 en `DESARROLLO`.
- **Siguiente**: Etapa 2 (`v5_masa_sin_eco` + `cfg_color.v5_solido`).

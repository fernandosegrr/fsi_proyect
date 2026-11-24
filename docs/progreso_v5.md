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
- **Fase 2, Etapa 1 (`v5_coincidencia_estable`)**: retencion breve de coincidencia con presencia de color, bloqueo de reanudacion falsa detenido y retroceso recto previo al giro cuando no hay distancia ToF.

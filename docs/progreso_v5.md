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

### Etapa 2: `v5_masa_sin_eco` + `cfg_color.v5_solido` (SIMULADO)
- **Que se hizo**:
  - `obstaculos.h/.c`: agregados `v5_solido`, `v5_rachas`, `masa_solida[OBS_ZONAS]`, `racha_vertical[OBS_ZONAS]` y `cercania_color[OBS_ZONAS]`. `clasificar_masa_solida` detecta masa compacta de alto contraste o profundidad oscura con cambio cromatico (`dcroma >= 22`, excluyendo resaltes `max_rgb >= 235`, sombras suaves y piso no iluminado tras parches de sol) y tope global `< 68 %`.
  - `navegador.h/.c`, `puente.c`, `pruebas_autonomo_v2.c`: activada bandera `v5_masa_sin_eco`, confirmacion con 2 cuadros seguidos en corredor (respetando fin de ruta), retencion hasta 1800 ms en centro ante lavado AEC en `DETENIDO_OBSTACULO`, bloqueo de `vistazo` hacia zona solida y pruebas unitarias en `pruebas_obstaculos.c` / `pruebas_navegador.c`.
- **Resultados verificados (SIMULADO, 40 corridas)**:
  - `make -C nucleo_c test`: 0 fallos en toda la suite (incluyendo `pruebas_v5_masa_solida` y `pruebas_v5_etapa1_etapa2`).
  - `python scripts/compuertas_v5.py --corridas 40 --apagado`: igualdad exacta `SI`.
  - `docs/precision/v5/etapa2.txt`: **G1 `CUMPLE`**, **G7 `CUMPLE`** (0 paradas/esquives/AYUDA nuevos en piso limpio en ambas matrices y modelos laser); `objeto_bajo` pesimista bajo a **0/40** contactos en `DESARROLLO` y `RESERVADO` (`v3`: 19 y 14); `bajo_solo_color` pesimista bajo a **3/40** y **2/40** (`v3`: 23 y 17); `bloque_sin_eco` bajo de **40/40** en `v3` a **14/40** (`DESARROLLO`) y **13/40** (`RESERVADO`), quedando los contactos restantes ligados al giro derecho sin retroceso previo en seco.

### Etapa 3: `v5_patas_delgadas` + `cfg_color.v5_rachas` + margen de cola (SIMULADO)
- **Que se hizo**:
  - `obstaculos.c`: unificacion en `es_celda_contraste_fisico` + `es_trazo_horizontal_delgado` + `clasificar_rachas_verticales` (`v5_rachas = 1` por defecto) para conservar rachas verticales delgadas (`>= 3` filas en `<= 3` cols) de patas finas sin disparar en juntas ni sombras suaves; ajuste global al `< 96 %` de celdas activas para no perder `bloque_sin_eco` a corta distancia.
  - `navegador.h/.c`: activada `v5_patas_delgadas = 1`; confirmacion en `ZONA_CENTRO` con racha vertical + caja neuronal / par bilateral / eco ToF reciente sin bloquear flancos; bloqueo de `vistazo` cercano (`vistazo_cerc >= 2`) tambien para solo red; reversa recta previa al pivote en paradas muy cercanas o sin eco (`cercania >= 3`); ancho minimo conservador (`210` mm) solo cuando `tof_dist_mm == 0 && !rodeo_por_vistazo && scan_muestras <= 2`; margen de cola en recta final y paso minimo `520` mm en `PAUSA_PASAR` para cubrir las 4 esquinas a velocidad real.
  - Pruebas unitarias nuevas en `pruebas_obstaculos.c` (`pruebas_v5_rachas_verticales`) y `pruebas_navegador.c` (`v5_patas_delgadas`).
- **Resultados verificados (SIMULADO, 40 corridas)**:
  - `make -C nucleo_c test`: 0 fallos en toda la suite.
  - `python scripts/compuertas_v5.py --corridas 40 --apagado`: igualdad exacta `SI`.
  - `docs/precision/v5/etapa3.txt`: **G1 `CUMPLE`**, **G3 `CUMPLE`**, **G7 `CUMPLE`**, **G6 `terminado_falso = 0` en `DESARROLLO`** (`1` residual en `RESERVADO` `silla_patas`). `bloque_sin_eco` bajo a **7/40** contactos en `DESARROLLO` y **9/40** en `RESERVADO` (`v3`: 40); `banquito35x45` pesimista bajo de **25 a 6** contactos (`DESARROLLO`) y **11 a 1** (`RESERVADO`, `bm`: 21->9); `dos_muebles` pesimista bajo de **13 a 5** (`bm`: 20->7); `mueble_con_pared` pesimista bajo de **21 a 1** contacto.

### Etapa 4: `v5_pasillo_pared` — eleccion de costado y pasillo adaptado (SIMULADO)
- **Que se hizo**:
  - `navegador.h/.c`: declarada y activada por defecto `v5_pasillo_pared = 1` con `navegador_poner_pasillo_pared()`, controlada tambien desde `navegador_poner_v5()`.
  - En `elegir_lado`, cuando ambos costados estan libres y no viene de `vistazo_confirmado`, se suma un costo simetrico de borde lejano (`borde_lejos * 4 + zona_celdas`) para evitar girar hacia paredes lejanas antes de acercarse. Se descarto un sesgo fijo a la izquierda y se descarto activar `deshacer_giro` agresivo en pasillos tras verificar en `DESARROLLO` y `RESERVADO` que aumentaban los contactos (confirmando la advertencia previa de `NAV_DESHACER_GIRO_MAX` en `navegador.h`).
  - En `tick_mirar`, ajuste lateral adaptativo `pasillo_adaptado` (`lateral = tope`) cuando la lectura ToF inicial midio pared lateral sin corte (`!scan_corte && scan_pared_mm > 0 && tof_dist_mm > 0`) y `tope >= max(320, ancho_est + 185)`, conservando holgura segura para el giro de retorno sin abortar con `NAV_CAUSA_FRENTE_ANCHO`.
  - Correccion puntual en `deshacer_giro` (`tof_dist_mm > 0` antes de restar avance) y reversa previa recta si `!arco_cabe` tras deshacer giro con camino pisado detras; margen de cola en `PAUSA_PASAR` hasta `560` mm y `min_pasado = 160` mm en `reinc_ya_paso_el_final`.
  - Prueba unitaria nueva en `pruebas_navegador.c` verificando el desempate por `borde_lejos` y el control por bandera.
- **Resultados verificados (SIMULADO, 40 corridas)**:
  - `make -C nucleo_c test`: 0 fallos en todas las suites (`275/0` en `pruebas_esquive_confirmado`, `206/0` en `pruebas_navegador`, `46/0` en `J16`).
  - `python scripts/compuertas_v5.py --corridas 40 --apagado`: igualdad exacta `SI`.
  - `docs/precision/v5/etapa4.txt`: **G1 `CUMPLE`**, **G3 `CUMPLE`**, **G7 `CUMPLE`**, **G6 `terminado_falso = 0` en `DESARROLLO`** (`1` residual en `RESERVADO` `silla_patas`). Mejoras en pasillos con **0 contactos**: `pasillo` normal pasa de `22` a `29/40 ok` en `DESARROLLO` (AYUDA `18 -> 11`) y de `36` a `40/40 ok` en `RESERVADO` (`ayuda 4 -> 0`); `pasillo` pesimista pasa de `26` a `32/40 ok` en `DESARROLLO` (`ayuda 14 -> 8`) y `37` a `39/40 ok` en `RESERVADO`; `pasillo_angosto` sube a `37/40 ok` (`ayuda 5 -> 3`); `caja_y_pared_al_lado` mantiene `0` contactos en las 4 matrices (mejorando a `0` vs `1` en `DESARROLLO` normal y bajando `ayuda` en `RESERVADO` de `16->14` y `13->12`).
- **Siguiente**: Etapa 5 (`v5_fallo_seguro` en `nucleo_c`, mejoras seguras de ToF/I2C `SEG=4` en `firmware_carro`, JNI/toggle UI en `app_android`, compilacion firmware + APK, `scripts/evaluacion_ciega.ps1` probado con semilla 1 y auditoria).

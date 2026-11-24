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

### Etapa 5: `v5_fallo_seguro`, firmware `SEG=4`, app Android, sanitizadores y auditoria (Completada)
- **Que se hizo**:
  - `nucleo_c/navegador.h/.c`: agregada bandera `v5_fallo_seguro = 1` (controlada por `navegador_poner_fallo_seguro()` y `navegador_poner_v5()`), `NAV_CAUSA_ATASCO = 20` (`NAV_CAUSAS = 21`), correccion de acceso en `ubicar_en_linea` para rutas de 1 punto (`n_puntos < 2`), deteccion de atasco verificada por laser ToF (`FORWARD >= 1800 ms` con `>= 5` lecturas frescas validas `< 850 mm` dentro de `+-20 mm`), tope de 3 continuaciones tras `AYUDA` sin avance `>= 200 mm` hacia `NAV_ERROR`, y guardas `#ifdef OBS_V5` en `pruebas_autonomo_v2.c` + `#define OBS_V5 1` en `obstaculos.h` para compilacion limpia de la referencia `e37f77b`.
  - `firmware_carro/`: actualizado `CAPS` a `SEG=4` conservando `TOF_PARO_MM=150`, `TOF_BLOQUEO_MM=180`, watchdog `1000 ms`, prioridad `STOP`/`EBRAKE` y `SENS:` de 3 enteros; rechazo de lecturas basura `< 8 mm` y `>= 8000 mm` / `rstat == 4`, consulta `TOFE? -> TOFE:<vivo>,<recup>,<invalidas>`, y recuperacion I2C de 9 pulsos SCL (`hw_i2c_recuperar`) restringida estrictamente al carro detenido (`>= 300 ms` quieto y sin giro activo). Pruebas actualizadas en `pruebas_firmware/`.
  - `app_android/`: interruptor maestro `Autonomo v5-carbon` (encendido por defecto) en `Ajustes.kt`, `SensoresActivity.kt`, `SesionCarro.kt`, `ControlLogica.kt`, `ModoSemiautonomo.kt`, `NavegadorNativa.kt` y `puente.c` (`ponerV5`), con registro `v5=1/0` en la cabecera de `CajaNegra`.
  - `scripts/evaluacion_ciega.py`, `scripts/evaluacion_ciega.ps1`, `scripts/evaluacion_ciega.sh` y `.github/workflows/sanitizadores.yml` (ASan/UBSan en Linux incluyendo `pruebas_fusion`, `pruebas_obstaculos` y `pruebas_autonomo_v2` en `RESERVADO` pesimista).
- **Auditoria independiente (`research`, conversacion `e22fd22c`) y correcciones aplicadas**:
  - Sin hallazgos criticos ni altos sobre autoridad del laser, freno, watchdog ni corrupcion de memoria. (Nota de transparencia: el subagente auditor probo por su cuenta `evaluacion_ciega.py --semilla 123456789` durante su revision de solo lectura; no se ajusto ningun parametro ni umbral a partir de esa corrida.)
  - Se corrigieron de forma puntual todos los defectos medios/bajos accionables sin cambiar la logica ni compromisos de navegacion: (1) sincronizacion de `v5_activo_global` bajo `candado_vision` y lectura unica en `puente.c`, (2) limpieza de `ultimo_*` en `cargarRuta` y omision cuando `celdas_zona_global < 0` (modo sin camara), (3) compuerta de `nav->v5_activo` y expiracion de `v5_coinc_cercania` en `navegador.c` (reproduciendo paridad exacta incluso en el caso borde `vistazo_confirmado` con `v5=0`), (4) chequeo completo de `a["contactos"] > b["contactos"]` en `G4` de `scripts/compuertas_v5.py`, (5) rechazo `< 8 mm` y `< 0` antes de refrescar `ultima_lectura_ms` en `tof.c` (permitiendo apagar `tof_vivo` ante basura continua `> 500 ms`) y (6) cobertura ASan/UBSan ampliada en `.github/workflows/sanitizadores.yml`, con nuevas pruebas unitarias en `pruebas_navegador.c` y `pruebas_sensores.c`.
- **Resultados verificados post-correccion (SIMULADO / SOLO COMPILADO)**:
  - `make -C nucleo_c test`: 24 ejecutables, **1922 casos, 0 fallos** (tambien `pruebas_navegador` 212/0 y `pruebas_fusion` 86/0 sin traps con `-ftrapv`).
  - `make -C pruebas_firmware test`: 5 ejecutables (**573 casos, 0 fallos**) + `verificar_pagina.py` OK.
  - `python pruebas_firmware/probar_servidor_falso.py`: **61 casos, 0 fallos**; `python scripts/probar_version.py`: **21 pruebas, OK**.
  - `scripts/compilar_app.ps1`: **COMPILACION OK** (`BUILD SUCCESSFUL`).
  - `arduino-cli compile` (`secretos.ejemplo.h` en `$env:TEMP`): `firmware_carro` **1,034,982 bytes (78 %)** / 52,128 bytes RAM; `camara_prueba` **1,084,365 bytes (82 %)** / 71,872 bytes RAM.
  - `python scripts/compuertas_v5.py --corridas 40 --apagado`: **Igualdad exacta vs `e37f77b`: SI**.
  - `docs/precision/v5/etapa5.txt`: **G1 `CUMPLE`**, **G3 `CUMPLE`**, **G7 `CUMPLE`** (mismas cifras limpias que `etapa4.txt`, sin regresiones; `G2`, `G4`, `G5`, `G6` permanecen `NO CUMPLE` en casos puntuales documentados).
  - `scripts/evaluacion_ciega.ps1 -Semilla 1`: ejecutado **solo con semilla 1** tras correcciones para comprobar funcionamiento (`Exit 0`).

## Fase 3 — Documentacion, protocolo fisico A/B y manual PDF (Completada)
- Creado `docs/estado_20261123.md` con estado técnico completo, cifras verificadas post-correccion de `d758e0f`, banderas de reversion, requisitos opcionales de firmware `SEG=4`, riesgos residuales y compuertas G1–G7 (`SIMULADO`).
- Actualizados `docs/limites_conocidos.md` (seccion v5-carbon y `Causa 20`), `README.md` y `docs/autonomo_v5_diseno.md` (decisiones finales y variantes descartadas).
- Creado `docs/protocolo_prueba_fisica_v5.md` y `docs/pruebas_fisicas_v5_plantilla.csv` (reglas de seguridad, `adb install -r` sin desinstalar ante fallo de firma, carga opcional de `SEG=4` no ejecutada, mediciones baratas `M-v5.1`..`M-v5.6`, bloques progresivos `P0`..`P4` y seccion A/B contra `v3`).
- Actualizados capitulos del manual (`04_firmware.md`, `06_protocolo.md`, `07_navegacion.md`, `08_vision.md`, `09_aplicacion.md`, `12_pruebas.md`, `13_limites.md`, `15_problemas.md`) y portada en `docs/manual/generar_manual.py`.
- Regenerado `docs/manual/Manual_Proyecto_Carro_FSI.pdf` con `--sin-figuras` (**75 paginas, 12 figuras, 30 tablas, 3,838,232 bytes / 3748.3 KB < 15 MB**; portada verificada con `pymupdf`) y validado `python docs/verificar_enlaces.py` (**160 archivos Markdown, 202 enlaces validos, 0 rotos**).
- **Siguiente**: Fase 4 (verificacion previa, push y tag anotado `freeze/v5-carbon`, obtencion de semillas externas A/B/C y evaluacion ciega unica sobre el commit congelado).

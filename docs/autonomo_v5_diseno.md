# Diseno autonomo v5-carbon (`feat/autonomo-v5-carbon`)

**Estado del documento:** Diseno basado en diagnostico de cajas negras reales del 15-nov-2026 (`cajas_negras/cn_20261115_*.txt`, `docs/pruebas_fisicas_20261114.csv`) y trazas simuladas de `v3` (`e37f77b`) en el conjunto `DESARROLLO` (`docs/precision/v5/fase1_trazas_v3.txt`, `fase0_linea_base.txt`). Prohibido usar resultados de `RESERVADO` para ajustar umbrales.

---

## 1. Diagnostico de causa raiz (evidencia observada)

### 1.1 Leccion del intento descartado (`archive/feat-autonomo-v4-a200df4`)
La guardia de `v4` (`guardia_v4`, causas `20` y `21`) detuvo y pidio `AYUDA` cuando `tof_fresco == 0` / `secuencia_tof < 0` o cuando color aislado marco 3 cuadros a < 100 mm lateral, causando 31/40 `AYUDA` en `libre_limpio` y 87–137 `AYUDA`/esquives en `falso_sol`/`falso_sombra`/`falso_reflejo`.  
En nuestras propias cajas negras reales del 15-nov (`cn_20261115_*.txt`, 518 ticks `CN`):
- `fv != 0` ocurrio en **280 de 518 ticks (54.1 %)** en piso limpio sin obstaculo.
- `nz > 0` ocurrio en **382 de 518 ticks (73.7 %)** y hubo `VISTAZO_DIAG` en 6 de 7 corridas (`sin_eco` -> seguir).
- Por eso es regla inviolable: **una lectura ToF sin eco, una brecha pasajera de `SENS` o vision de una fuente parecida al piso NUNCA deben disparar `AYUDA` ni esquives en trayecto limpio (`G1`).**

### 1.2 Fallas concretas de `v3` en el conjunto `DESARROLLO` (SIMULADO)

1. **`bloque_sin_eco` (v3: 40/40 contactos) y `objeto_bajo` / `bajo_solo_color` con laser pesimista (19 y 23/40 contactos):**
   - **Caso A (`bloque_sin_eco_norm` r0 y `bajo_solo_color_pes_r0`):** Cuando el bloque no pertenece a clases COCO (`recall ~0.05` o `0.0`) y no da eco (`sin eco`), el clasificador de color marca `cercania=1..3` hasta `80 celdas`, pero `fusion.c` anula la zona al exigir `color && red`. `vistazo` tuerce 14–16°, confirma `sin_eco`, sigue adelante y choca (`[3000 ms] CONTACTO`). Ademas, en `bloque_sin_eco` `r0` (`[2550->2750 ms]`), la exposicion automatica (AEC) sube la ganancia al oscurecerse el cuadro por el bloque cercano, desplazando su RGB hacia el tono del piso.
   - **Caso B (`objeto_bajo_pes_r5`):** Aunque `color && red` coinciden y detienen el carro (`[2500..7000 ms]`), en cuanto cae la caja de la red en un cuadro siguiente sin eco ToF, `tick_detenido` interpreta camino libre y reanuda `FORWARD` directo al choque (`[8300 ms] CONTACTO`). En paradas de `cercania >= 2` sin distancia ToF valida, girar en el sitio sin retroceso previo mete el frente en el obstaculo al pivotar.

2. **`banquito35x45` (v3: 15–25 contactos, 10–12 AYUDA) y `silla_patas` (4 contactos, 27–33 `bajo_mueble`):**
   - En `banquito_norm_r0` y `silla_norm_r0`, las patas verticales de 25–35 mm ocupan pocas columnas (~1–2 cols) y la erosion horizontal de `confirmar_celdas` las recorta.
   - Aun cuando color si ve 8–20 celdas (`r0`, `r2`), `navegador` inicia `vistazo` incluso en `cercania=2` (`[2900 ms]` en `r2`), girando el chasis **hacia adentro del mueble** (`BAJO EL MUEBLE`, contacto en `[3500 ms]`).
   - Al frenar cerca de una pata (`r0`: `laser=109`), retrocede insuficientemente (~60 mm de holgura frente al mueble) y al girar hacia la otra pata el chasis choca en `cmd=LEFT` (`[6000 ms]`).
   - En `escaneo` de esquive, el cono ToF pasa entre las patas (`con_eco=0..4`, `ancho_mm=0`), subestimando el ancho real del mueble y cerrando el rodeo demasiado pronto.
   - Con la verificacion de **4 esquinas** y velocidad real (25.7–29.4 cm/s vs 30.8 cm/s planeado), la corrida 26 de `banquito` pesimista dejo la esquina trasera en `cola = -15 mm` (`terminado_falso = 1`, falla G6).

3. **`pasillo` (v3: 14–18 AYUDA) y `caja_y_pared_al_lado` (15–18 AYUDA, 1 contacto):**
   - En `caja_pared_norm_r0` y `banquito_norm_r11`, el primer giro va hacia el lado con pared/pata, `escaneo` detecta `lado_cerrado` e incluso imprime `otro_lado_libre=1`, pero aborta de inmediato con `AYUDA causa=16` sin deshacer el giro ni intentar el otro lado.
   - En `caja_pared_norm_r5`, `guardia_giro` frena a mitad del giro (`61 deg`) contra la pared (`laser_mm=199`) con `cambio_lado=0` y aborta con `AYUDA causa=18` sin revertir al origen ni cambiar de lado.
   - En `pasillo` (semiancho 550–800 mm), exige holguras fijas excesivas o elige indiferentemente el pivote derecho (~158 mm) frente al izquierdo (~90 mm), abortando por `causa=16` pese a existir hueco fisico suficiente para rebasar centrado.

4. **Fallo seguro, bus I2C y diagnostico del hardware/app (NO MEDIDO / analisis estatico y logs):**
   - No hay deteccion de atasco (`NAV_CAUSA_ATASCO`): si las ruedas patinan o traban en `FORWARD` manteniendo PWM, el driver TC1508A soporta 2.2–3.0 A de pico frente a 1.8 A continuos (`04_motores_cinematica.md`).
   - En `ubicar_en_linea`, una ruta de 1 punto indexa `ruta[-1]` (UB detectado por UBSan en v4).
   - `firmware_carro/tof.c` no reintenta recuperacion I2C controlada cuando `err_seguidos >= 3` ni filtra valores `< 8 mm` / `>= 8000 mm` de forma consistente; `SENS:` debe conservar 3 campos para retrocompatibilidad con `ControlLogica.kt:573`.

---

## 2. Respaldo bibliografico primario verificado

Todas las referencias provienen de fuentes primarias o secundarias auditadas el **2026-11-16** en `docs/investigacion_autonomo/` (sin citas inventadas):

1. **STMicroelectronics, *VL53L0X Datasheet DS11555 Rev 6* (junio 2024).** Copia tecnica en `https://www.makerguides.com/wp-content/uploads/2024/09/vl53l0x-datasheet.pdf` (consulta 2026-11-16, **Confianza: alta**; sec. 6.1, Tablas 12–15). El cono de vision de **25 deg** asume el haz totalmente cubierto; blancos oscuros (<17 % reflectancia) o angostos caen bajo umbral sin devolver eco ("ausencia de eco != camino libre").
2. **M. Sridharan (Univ. de Edimburgo), *Robot Sensing* (`S6_RobotSensing.pdf`, basado en Thrun, Burgard, Fox 2005 cap. 6).** `https://homepages.inf.ed.ac.uk/msridhar/Teaching/MobileRobotics/Slides/S6_RobotSensing.pdf` (consulta 2026-11-16, **Confianza: media**). Modelo de haz: medidas de rango maximo ("sin eco") corresponden a fallo de deteccion o reflexion/absorcion, no a espacio libre.
3. **I. Ulrich e I. Nourbakhsh, *Appearance-Based Obstacle Detection with Monocular Color Vision*, AAAI-2000, pp. 866–871.** `https://cdn.aaai.org/AAAI/2000/AAAI00-133.pdf` (consulta 2026-11-16, **Confianza: alta**). Separar apariencia del suelo por intensidad y cromaticidad; pisos brillantes y reflejos generan falsos positivos clasicos; nunca incorporar el frente inmediato bloqueado al modelo dinamico del piso.
4. **Th. Gevers y A. W. M. Smeulders, *Color-based object recognition*, Pattern Recognition 32(3), 1999 (resumido en `05_vision_monocular.md` [3]) y T. Horprasert et al. (`05_vision_monocular.md` [4]).** Consulta 2026-11-16 (**Confianza: media-alta**). Bajo modelo dicromatico, sombras suaves mantienen la cromaticidad normalizada con menor brillo (`>= 0.40–0.55` tipico), mientras que brillos especulares saturan sin cambio de cuerpo; una distorsion fuerte de cromaticidad u oscuridad profunda distinta del piso senala un objeto fisico real.
5. **M. Tan, R. Pang, Q. V. Le, *EfficientDet: Scalable and Efficient Object Detection*, CVPR 2020 (arXiv:1911.09070).** `https://arxiv.org/abs/1911.09070` (consulta 2026-11-16, **Confianza: alta**) y evaluacion oficial TFLite *EfficientDet-Lite0* (320x320 COCO 80 clases; AP pequeno 12.4 vs 52.7 en D0). Objetos genericos oscuros, planos o patas verticales de 2–3 cm estan fuera de distribucion respecto a fotos estandar de COCO.
6. **J. Nubert, N. Funk, F. Meier, F. Oehler, *Developing a Purely Visual Based Obstacle Detection using Inverse Perspective Mapping*, arXiv:1809.01268 (2018).** `https://arxiv.org/abs/1809.01268` (consulta 2026-11-16, **Confianza: alta**). Consenso temporal de 2–3 cuadros consecutivos reduce falsos positivos por ruido instantaneo, manteniendo reaccion en 1 cuadro ante objetos inmediatos.
7. **J. Borenstein y L. Feng, *Measurement and Correction of Systematic Odometry Errors in Mobile Robots*, IEEE Trans. Robotics and Automation, 1996.** `https://cs.au.dk/~ocaprani/legolab/DigitalControl.dir/NXT/Lesson10.dir/paper58.pdf` (consulta 2026-11-16, **Confianza: alta**). Errores sistematicos de giro/diametro (aqui agravados por pivotes asimetricos 90 mm vs 158 mm) acumulan desvio angular; usar referencias absolutas y corregir simetria/calibracion en ambos sentidos.
8. **IFA/DGUV Report 2/2017e (*Functional safety of machine controls – Application of EN ISO 13849*, jul-2019, `https://publikationen.dguv.de/widgets/pdf/download/article/4150`) y P. Koopman (*Proper Watchdog Timer Use*, 2014, `https://betterembsw.blogspot.com/2014/05/proper-watchdog-timer-use.html`).** Consulta 2026-11-16 (**Confianza: alta / media-alta**). Documentar estado seguro, monitoreo de vivacidad + progreso real (deteccion de atasco), e histeresis de rearme controlado.

---

## 3. Plan de arquitectura v5 por etapas y banderas

Todas las mejoras quedan controladas por banderas individuales y por el interruptor maestro `v5` (por defecto activado en `v5`, pero reversible a `0` / `AV5_APAGADO=1` en tiempo de ejecucion, reproduciendo exactamente `v3`):

| Idea / Etapa | Banderas | Modulos | Problema que resuelve | Criterio de exito en `DESARROLLO` |
|---|---|---|---|---|
| **A / Etapa 1** Coincidencia estable y retroceso sin ToF | `v5_coincidencia_estable` | `fusion.c`, `navegador.c/.h` | Parpadeo de red entre cuadros al estar detenido; giro sin espacio cuando ToF no tiene eco | `objeto_bajo` pesimista y bloqueos sin eco con caja no reanudan contra el objeto |
| **B / Etapa 2** Masa compacta no-piso de alto contraste sin eco | `v5_masa_sin_eco`, `cfg_color.v5_solido` | `obstaculos.c/.h`, `navegador.c` | `bloque_sin_eco` (40/40 v3) y `bajo_solo_color` cuando no hay eco ToF ni caja COCO, **excluyendo resaltes y sombras suaves** | `G1`: **0** esquives/AYUDA/paradas nuevas; `G3`: contactos cercanos a 0 en `bloque_sin_eco` |
| **C / Etapa 3** Patas finas, no vistazo cercano y margen de cola | `v5_patas_delgadas`, `cfg_color.v5_rachas` | `obstaculos.c/.h`, `navegador.c` | Patas borradas por erosion, `vistazo` a `cercania >= 2` hacia muebles, subestimacion de ancho en escaneo y `terminado_falso` de 4 esquinas | Reduccion drastica de contactos y `bajo_mueble` en `banquito` y `silla`; `G6`: `terminado_falso = 0` |
| **D / Etapa 4** Deshacer giro con pared y pasillo asimetrico | `v5_pasillo_pared` | `navegador.c/.h` | `AYUDA` causas 16/18 cuando el otro lado si esta libre; preferencia pivote izq (~90 mm) y paso adaptado en `pasillo` | Reduccion clara de `AYUDA` en `pasillo` y `caja_y_pared_al_lado` sin contactos ni regresion en `G1` |
| **E–H / Etapa 5** Fallo seguro, `SEG=4`, JNI/APK y eval ciega | `v5_fallo_seguro` | `navegador.c`, `firmware_carro/`, `app_android/`, `scripts/` | Fix UB 1 punto, atasco ToF (`NAV_CAUSA_ATASCO=20`), tope `Continuar`, rafaga TCP, `SEG=4` (`tof.c` `<8mm`/`>=8000` + I2C quieto + `TOFE?`), APK + `CAB v5=...` | Compilacion limpia APK/ESP32, pruebas nuevas pasando, `AV5_APAGADO=1 == v3`, script ciego validado con semilla 1 |

# Calibracion del simulador y separacion DESARROLLO / RESERVADO (v5-carbon)

**Workspace usado:** `E:\Dev\fsi_proyecto` (`D:\Dev\fsi_proyecto` no existe).  
**Base de partida:** `feat/autonomo-v3` (`e37f77b`).

## 1. Evidencia fisica medida usada para calibrar (SOLO LECTURA, sin versionar)

De `docs/pruebas_fisicas_20261114.csv` (7 corridas fisicas del 2026-11-15 en rectas de 150 cm A1/A2) y las 7 cajas negras `cajas_negras/cn_20261115_*.txt` (518 ticks `CN`, 341 intervalos `VTEL`, 293 intervalos inferidos `SENS`), analizamos cuantitativamente:

- **Velocidad real en modo AUTO (VALIDADO EN EL CARRO con cinta y cronometro):**
  * Corrida #2 (A1): `140 cm / 4.90 s = 28.57 cm/s`
  * Corrida #3 (A1): `145 cm / 4.95 s = 29.29 cm/s`
  * Corrida #6 (A2): `145 cm / 4.94 s = 29.35 cm/s`
  * Corrida #7 (A2, con 1 pausa breve de 0.52 s): `140 cm / 5.45 s = 25.69 cm/s` (~28.4 cm/s sin pausa)
  * Rango real medido: **25.7 a 29.4 cm/s** (mientras la app estimaba a `325` decimas ms/cm = `30.8 cm/s` y el simulador v2/v3 heredado usaba `518` decimas ms/cm = `19.3 cm/s`).
- **Cadencia de video y `SENS` (MEDIDO EN CAJAS NEGRAS):**
  * Intervalo entre cuadros `VTEL` (`n = 341`): `min = 51 ms`, `p5 = 54 ms`, `med = 69 ms`, `mean = 90.5 ms`, `p95 = 147 ms`, `max = 374 ms`. La corrida #4 aborto por perdida de video tras `cu = 720 ms` (`ERROR causa 5`).
  * Intervalo inferido de llegada `SENS` (`n = 293`): `min = 62 ms`, `p5 = 79 ms`, `med = 100 ms`, `mean = 108.8 ms`, `p95 = 174 ms`, `max = 646 ms`. Edad del ToF al `tick` (`n = 518`): `p5 = 7 ms`, `med = 42 ms`, `p95 = 151 ms` (antes de primer SENS / corte: `99999`).
- **Falsos disparos de vision en piso limpio (MEDIDO EN CAJAS NEGRAS):**
  * En 518 ticks `CN` sin obstaculo en la trayectoria: `fv != 0` en 280 ticks (54.1 %), `nz > 0` en 382 ticks (73.7 %), `hay != 0` en 12 ticks (en A2), `pr = 1` en 12 ticks (2 pausas breves ~0.52-0.56 s en #5 y #7) y `VISTAZO_DIAG` en 6 de 7 corridas (`sin_eco` en piso libre sin frenar ni esquivar).

## 2. Parametros aleatorizados del simulador v5 (SIMULADO / NO MEDIDO fuera de las rectas anteriores)

Al activar `AV5_CONJUNTO=1|2` o `AV5_CIEGA`:
- **Modo heredado (`AV5_CONJUNTO` sin definir):** conserva intactos `cal.ms_por_cm_x10 = 518`, semillas `170000 + e*10007 + i` y los 23 escenarios de v3 (reproduccion exacta verificada).
- **Avance y frenado:** el navegador planifica con `cal.ms_por_cm_x10 = 325` (~30.8 cm/s); la fisica sortea `sim.real.ms_por_cm_x10` en `[315, 390]` (~25.6-31.7 cm/s, centrado en los 25.7-29.4 cm/s medidos) con `pct_avance` en `[96, 104]` % (mas caida por bateria en `bateria_baja`) y `frenado_mm` en `[25, 35]` mm.
- **Giros y pivotes asimetricos:** pivote izquierdo nominal 90 mm (`85-115 %`), pivote derecho nominal ~158 mm (`85-115 %`), dispersion temporal de giro `±0.6 ms/grado` (`ms_por_grado_{izq,der}_x10 ± 6`), sesgo de giroscopio residual `[−50, 50] mdps`, escala fina `[−15000, 15000] ppm`. Los giros bajo carga en piso real siguen **NO MEDIDOS**.
- **Camara, SENS y fotometria:** cadencia base por corrida en `[55, 130]` ms con jitter por cuadro `[−15, +25]` ms y perdida `[2, 4] %`; retraso de transporte `[110, 170]` ms; latencia `SENS` en `[45, 145]` ms con jitter de periodo `[85, 135]` ms; ruido de piso `[1.2, 2.8]` y luma objetivo `[122, 136]`.
- **Modelo laser VL53L0X con mezcla de cono 25° (`mv_laser_mezcla = 1`) y retiro del sensor `[0, 35]` mm:**
  * **Normal (`AV5_LASER=0`):** perdida `[1, 5] %`, umbral de senal `[0.8, 1.2]`, ruido relativo `[1, 3] %`, ruido absoluto `[2, 5]` mm.
  * **Pesimista (`AV5_LASER=1`, estres):** perdida `[8, 20] %`, umbral de senal `[1.5, 3.0]`, ruido relativo `[1, 6] %`, ruido absoluto `[3, 10]` mm. (Hipotesis de sensibilidad **NO MEDIDA** en el carro.)
- **Verificacion de `Terminado`:** cuando el simulador esta en modo calibrado v5, `cola_s` toma el minimo de la proyeccion de las **4 esquinas del chasis completo** (`200 x 240 mm`) sobre la direccion de llegada para detectar si alguna esquina quedo detras de la cara lejana de un objeto `en_ruta`.

## 3. Separacion estricta DESARROLLO vs RESERVADO y Evaluacion Ciega

- **DESARROLLO (`AV5_CONJUNTO=1`):** semillas `2100000 + indice * 10007 + corrida`, 24 escenarios (`0..23`: los 23 escenarios base con `pasillo` de semiancho `550..800 mm`, mas `bloque_sin_eco` con bloque oscuro/mate `reflectancia = 0.002`). Todos los ajustes de diseño se evaluan aqui.
- **RESERVADO (`AV5_CONJUNTO=2`):** semillas `4300000 + indice * 10007 + corrida`, 27 escenarios (`0..26`: incluye exclusivamente `dos_muebles` [24], `mueble_con_pared` [25], `pasillo_angosto` [26, semianchos asimetricos `500-620` vs `660-760` mm con paso libre por un lado], ademas de variantes perturbadas en geometria, patas de `22-42 mm`, alturas de objetos bajos `12-24 mm`, bloques sin eco variados y fotometria/iluminacion distinta). **Prohibido ajustar umbrales contra RESERVADO.**
- **CIEGA (`AV5_CIEGA=<u64>`):** generador determinista por `SplitMix64` (parseo de 64 bits con `strtoull` para semillas mayores de 32 bits), re-sorteando geometrias, iluminacion y fisica por escenario y corrida entre las 27 familias.

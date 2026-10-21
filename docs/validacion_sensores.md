# Validación física del giroscopio y del láser (Fase C de J11)

Qué se prueba, cómo y qué número debe salir. **Estas pruebas necesitan el carro** (hay que mover cosas o mirar el piso); el código ya está probado en la PC con sensores simulados (ver `docs/sensores.md`). Anota cada resultado en la tabla del final.

## Antes de empezar

- DevKit cargado con `scripts\cargar.ps1 nuevo` y conectado por USB. El giroscopio y el láser ya están verificados (`docs/sensores.md`): `CAPS?` debe decir `CAPS:IMU=1,TOF=1`.
- Las pruebas 1 a 4 se hacen **por USB** con el script (no hace falta el WiFi del carro ni las pilas de motores):

```bash
pruebas_firmware\.venv\Scripts\python.exe scripts\verificar_sensores.py <modo> --puerto COM6
```

  Modos: `estado`, `mano`, `deriva`, `vacio`, `distancia`, `calibrar`. Por USB **nunca** se mueven los motores.
- Las pruebas 5 a 8 usan la app (pantalla **Calibración → Sensores**, y las rutas) con las pilas de motores puestas, el teléfono unido al WiFi del carro y ~2 m libres.

## Las pruebas

| # | Prueba | Cómo | Número esperado |
|---|---|---|---|
| 1 | **Polaridad** | `scripts\verificar_sensores.py mano`. Con el carro quieto sobre la mesa, gíralo a mano 90° a la **izquierda** (antihorario visto desde arriba) y déjalo quieto. | Ángulo final **+90 ± 5**. Si sale **negativo**: poner `IMU_SIGNO -1` en `firmware_carro/config.h`, volver a cargar y repetir (el script lo avisa). |
| 2 | **Deriva** | `... deriva --segundos 60`, carro quieto (sin tocar la mesa ni el cable). | El ángulo **no se mueve más de 1°**. (Medido el 18 oct: 0.0°. Las dos veces que dio 0.8° y 25° alguien movió el carro.) |
| 3 | **Piso vacío** | `... vacio` con el láser mirando al frente y **nada a menos de 1.5 m**, 5 s. | **Libre** (más de 800 mm o fuera de rango). Si marca **150-350 mm** está viendo el piso: súbelo o inclínalo más hacia arriba (anota cuántos grados) y repite. |
| 4 | **Caja de 10 cm de alto a 30 cm** | `... distancia`. Primero la caja justo al frente a 30 cm; luego desplazada hacia un lado. | Al frente: **280-320 mm**. Desplazada a un lado: **no debe marcar** (libre). |
| 5 | **Giros: tiempo contra ángulo** | App → Calibración → Sensores → *Prueba de giro de 90°*. Pon una cinta en el piso. Haz **10 giros de 90° a la izquierda y 10 a la derecha primero «por tiempo» y después «por ángulo»**, midiendo con la cinta el ángulo real cada vez (también queda el ángulo del giroscopio en el log, `SENS_DIAG prueba_giro`). | **Por ángulo debe quedar a ±3° en al menos 9 de cada 10** (por lado). Reporta media y desviación de cada método. Si todos pasan de 90 (o se quedan cortos) el mismo valor, se ajusta `GIRO_ANTICIPACION_MS` (la anticipación empieza en 20 ms y se corrige sola; `ANT?` por USB dice el valor aprendido). |
| 6 | **Ruta en L** | Ruta en L con marcas de cinta en las esquinas y el destino. Dos veces cada una: con **Usar giroscopio** encendido y apagado. | Con giroscopio el error final debe ser **menor** que sin él. Referencia de la simulación (carro que gira ±10 %): sin giroscopio error medio **153 mm** (p95 378 mm); con giroscopio **45 mm** (p95 116 mm). Con el carro real los números serán otros: anótalos. |
| 7 | **Caja delante** | (a) Caja delante del carro **antes** de iniciar la ruta; (b) recta de 2 m con una caja en medio, **3 veces por lado**. Cada vez con **Usar sensor de distancia** encendido y apagado. | El carro no choca. Con el láser, si la cámara no ve la caja, debe frenar a ~12 cm (`TOF_PARO` en el log) y esquivar hacia el lado libre (si no sabe, a la izquierda). |
| 8 | **Corte de WiFi en una recta** | Ruta o botón avanzando; a mitad de la recta apaga el WiFi del teléfono. | Frena por el watchdog (**~20 cm** de avance en AUTO). Lo mismo si es un giro por ángulo: el giro termina solo por su tiempo máximo (500 ms + 15 ms por grado) o se aborta al desconectarse. |

## Qué mirar en el log

Etiquetas nuevas (`adb logcat` o el log de la app): `SENS_DIAG` (qué sensor decidió qué: capacidades, cada giro con objetivo y ángulo real, vetos y confirmaciones del láser, paros, caídas a tiempo), `TURN_DIAG` (ahora con `por=angulo|tiempo`) y `RUTA_DIAG`. Al final de la ruta, la pantalla de resultado dice cuántos giros fueron por ángulo y cuántos por tiempo.

## Si algo no sale

- **El ángulo crece solo con el carro quieto**: `scripts\verificar_sensores.py calibrar` (IMU_CAL, carro quieto) y repetir la prueba 2.
- **El láser frena en falso** (por ejemplo ve el piso): prueba 3; si pasa por rebotes de un objeto lateral, ver `TOF_PARO_MM` y la inclinación.
- **El giro por ángulo pasa o se queda corto siempre lo mismo**: ver `ANT?`; si queda en el tope (150 ms) el carro tiene mucha inercia.
- **Se pierde la conexión con los sensores**: `CAPS?` por USB. Cableado: `docs/sensores.md` (errores típicos).

## Pasos nuevos de J12 (los hace el dueño; Claude no mueve el carro)

0. **Cargar el firmware de J12** (USB, `scripts\cargar.ps1 nuevo`) e instalar la app nueva. Con USB: `CAPS?` (debe traer `TOFR=` y `TOF=1`), `LOOP?` (el máximo debe seguir bajo 5 ms con el carro en marcha; anotar también el máximo con el láser caído y el carro quieto) y `ANT_BORRAR` si se quiere empezar de cero.
9. **Calibración del giro con el giroscopio:** carro quieto, 1 m libre, Calibración → «Calibrar el giro con el giroscopio» → «Empezar los 6 giros». Esperar ~1 min. Anotar los dos «antes → después» de la pantalla (y si salió el aviso de más de 10 %). Repetir una vez: el segundo resultado debe cambiar menos de 5 % contra el primero. Después hacer un giro de 90° por tiempo (Sensores → «por tiempo») y ver que el giroscopio mida 90° ± 3°.
10. **Anticipación:** hacer 4 giros de 90° por ángulo a cada lado (Sensores → «por ángulo»), pedir `ANT?` (cuatro números en ms) y después otros 10 giros por lado: objetivo ±3° en 9 de 10 por sentido.
11. **Arranques del láser:** apagar y encender el carro 5 veces; la pantalla Sensores debe decir «Últimos arranques con reintentos del láser: k de n». Si pasa de 1 de 5 aparece el aviso con las causas probables.
12. **Giro con mensaje perdido:** no se puede provocar a propósito sin tocar el carro; si algún día el log trae `SENS_DIAG giro mensaje_perdido`, revisar que el giro no se duplicó.

## Resultados (se llena al probar)

| # | Fecha | Resultado | ¿Pasa? |
|---|---|---|---|
| 1 Polaridad | 18 oct | Con el carro por WiFi y la app: `GIRAR:+90` (izquierda) terminó en `GIRO_OK` con ángulo POSITIVO (sin tocar `IMU_SIGNO`). Confirmado a ojo por el dueño: el carro giró a la izquierda. | sí |
| 2 Deriva | 18 oct | 0.0° en 60 s (sin tocar) | sí |
| 3 Piso vacío | 20 oct | Por la pantalla Sensores (carro en el piso, nada delante): **«libre (más de 1.2 m)» o «1200 mm»** en todas las capturas en que se veía esa línea (de las 21:29 a las 22:09), y ni un veto, confirmación ni paro del láser en el log de toda la sesión. **Mínimos, máximos y conteo de lecturas falsas NO medidos**: la pantalla no los da; falta `verificar_sensores.py vacio` por USB. | pasa por pantalla; mín/máx NO MEDIDOS |
| 4 Caja a 30 cm | 20 oct | **Frente:** con un objeto parecido a la caja (distancia real **no medida con regla**; el dueño confirma que cuadra) el láser marcó **411, 409 y 410 mm** en tres capturas seguidas (±1 mm); no se pudo comparar con los 280-320 mm del enunciado. **Lado:** objeto a 30 cm adelante y 20 cm a la derecha (~34° fuera del eje): **«libre (más de 1.2 m)»** en 5 capturas seguidas (10 s), el láser no reaccionó. `TOF_PARO` no se probó (solo salta dentro de una ruta). | frente: NO MEDIDA contra 300 mm; lado: pasa |
| 5 Giros 10+10, tiempo y ángulo | 18 oct | **Medidos por el giroscopio** (pedido 90°; algunas tandas con menos de 10 giros porque fallaron toques del script que maneja la app por adb). POR TIEMPO: izquierda 14 giros, media 88.8°, 10 de 14 a ±3°; derecha 17 giros, media 82.4° (se queda corto ~8°: la calibración de ese lado ya no coincide con el carro), 2 de 17 a ±3°. POR ÁNGULO: izquierda 10 giros, media 90.6° (89.7 a 91.7), **10 de 10 a ±3°**; derecha 8 giros, media 92.7° (90.9 a 94.4), 5 de 8 a ±3° (se pasa ~2.7° constantes). **Con cinta** (cuadrado alineado): 4 giros de 90° a la izquierda por ángulo (el giroscopio midió 362.4° en total) dejaron el carro **4.5 cm a la izquierda y 5 cm hacia adelante** de donde empezó (posición, por el desplazamiento del pivote: 4 giros con el mismo pivote se suman; el navegador lo compensa con su modelo). **El ángulo con el que quedó (si quedó paralelo a la cinta) todavía NO se midió con cinta.** **Ángulo de esos 4 giros a la izquierda: alineado con la cinta** (dicho por el dueño, sin cuantificar). **Un solo giro de 90° a la derecha con cinta:** el giroscopio midió **92.7°** y el dueño lo vio en **~92°** (coinciden); el centro se movió **~15 cm hacia adelante y ~16 cm a la derecha**, igual que lo medido en la calibración del pivote derecho (15 y 16.5 cm). **4 giros seguidos a la derecha (giroscopio: 367.1°, unos 7° de más):** el dueño los vio como ~60° de inclinación a la derecha, algo que no cuadra con un giroscopio que acierta un giro suelto a 1°; la causa más probable es que el punto de partida no estuviera bien alineado o la estimación a ojo, pero NO está resuelto: queda pendiente repetirlos con el cuadrado bien alineado. | giro suelto a la derecha y 4 a la izquierda: coinciden con la cinta; 4 seguidos a la derecha sin resolver |
| 6 Ruta en L con y sin giroscopio | | | |
| 7 Caja delante / recta con caja | | | |
| 8 Corte de WiFi | | | |

## Resultados del 20 oct 2026 (J12: pruebas con el carro, hechas por Claude con la app por adb)

**Condiciones.** Carro en el piso con la powerbank y las 4 pilas AA, teléfono en la red CarroFSI (192.168.4.10), todo por la app (pantallas Sensores y Calibrar giro); el DevKit **no** estuvo por USB, así que no hay lecturas por serie. Cada número sale del log `SENS_DIAG`/`CALGIRO` del teléfono o de capturas de esas dos pantallas.

**Versiones verificadas antes de medir.**
- App (`com.fsi.carro` 0.3, versionCode 2): idéntica en sus 980 entradas a un build limpio del HEAD `427b740`. No se reinstaló.
- Firmware: el chip tenía el de **J11 fase B (`09c2b76`, compilado el 18 oct 11:34)**, sin `TOFR`, `ARR?` ni `ANT_BORRAR`. Se cargó el del HEAD con `scripts\cargar.ps1 nuevo` y se verificó con `esptool verify-flash` (MD5 del chip) contra el build del script. Las pruebas físicas de J12 hechas **antes** de esta recarga no valen. Al arrancar ya imprime `CAPS:IMU=1,TOF=1,TOFR=1`.

**Prueba 2, deriva: NO MEDIDA.** No se hizo la prueba de 60 s con el carro garantizado quieto. Solo hay una pista: el ángulo marcó 0.2° a las 22:09 y 0.2° a las 22:14; después el carro se movió a mano (−78.6° a las 22:18), así que no sirve como prueba.

**Prueba 3, piso vacío, y prueba 4, caja:** ver las filas 3 y 4 de la tabla de arriba.

**Prueba 5, giros de 90° medidos por el giroscopio.**
- **Por ángulo, izquierda (12 giros):** 99.5, 90.8, 89.2, 90.8, 91.7, 91.3, 91.7, 89.4, 89.1, 89.6, 89.1, 88.9. Media 90.9°, mín 88.9°, máx 99.5°. A ±3°: **11 de 12** (9 de los primeros 10: PASA). Sin el primer giro: media 90.2°, desv 1.1°.
- **Por ángulo, derecha (11 giros):** 106.6, 90.7, 90.2, 93.0, 90.8, 93.9, 92.4, 92.2, 90.3, 92.5, 92.3. Media 93.2°, mín 90.2°, máx 106.6°. A ±3°: **9 de 11**, pero solo **8 de los primeros 10: FALLA** el criterio de 9 de 10. Sobregiro medio a la derecha: **+3.2°** con el primer giro y **+1.8°** sin él (media 91.8°, desv 1.2°).
- **Por tiempo, izquierda (10 giros, 918 ms planeados):** 116.6, 120.4, 118.5, 91.2, 92.3, 93.0, 94.5, 94.2, 95.6, 93.4. Media 101.0°, desv 11.6°, **3 de 10** a ±3°. Los 3 primeros se pasan ~28°; los 7 siguientes quedan en 93.5° de media (desv 1.4°). (El criterio de ±3° es del método por ángulo; aquí es informativo.)
- **Por tiempo, derecha (9 giros, 724 ms planeados):** 72.8, 73.2, 86.4, 83.8, 91.4, 90.9, 93.3, 89.5, 94.0. Media 86.1°, desv 7.6°, **3 de 9** a ±3° (informativo); sin los 3 primeros, media 90.5° y desv 3.3°.
- **4 giros seguidos a la derecha (por ángulo):** los 4 primeros sumaron **380.5°** (con el de 106.6°); los 4 del medio (giros 5-8) **369.3°**; los 4 últimos **367.3°**, igual que el 367.1° del 18 oct (unos 7° de más, ~1.8° por giro). El ángulo acumulado de la pantalla coincide con la suma de los giros del log (1093.6° contra 1091.1° tras los 12 a la izquierda), así que no hubo giros ocultos. No se midió con cinta, pero el dueño dijo el 20 oct (a ojo, sin cuantificar) que **el carro sí gira lo que marca el giroscopio**; eso apunta a que el «~60°» a ojo del 18 oct fue un error de estimación y no del giroscopio, y a que el sobregiro de ~2° por giro a la derecha es real.

**Paso 9, calibración del giro con el giroscopio: FALLA (3 de 3 corridas, error 6 «inconsistente»).** Corridas a las 21:45:10, 21:47:43 y 21:56:11, de ~18 s cada una; las tres terminaron con `error=6` y **no guardaron nada**. Por eso **no hay «después»**, no hay aviso de cambio mayor de 10 %, y el ajuste fino de 90° **no** volvió a 1000. El «antes» que tenía guardado la app (perfil AUTO): izquierda 10.2 ms/°, derecha 8.3 ms/°; frenado izquierda 0.0° y derecha 12.2°; ajuste fino del giro de 90° izquierda 1000 y derecha **1119**. No se pudo comparar la corrida 1 contra la 2.

**Paso 10, anticipación: giros medidos sí, `ANT?` NO MEDIDO** (solo se lee por USB). El primer giro por ángulo de cada sentido se pasa (99.5° y 106.6°) y desde el segundo quedan en 89 a 94°, compatible con una anticipación por sentido que empieza en su valor inicial y se corrige sola.

**Paso 11, arranques del láser: PASA.** El historial del firmware pasó de `anotados=5 con_reintentos=2 detalle=11000` (21:20, arranques por USB y de la recarga) a `anotados=5 con_reintentos=0 detalle=00000` (22:08:46) y la pantalla dice **«0 de 5»**. El dueño encendió y apagó el carro «creo que 5 veces» (no verificado). En la recarga, el primer arranque trajo `TOFR=1` (1 reintento).

**Paso 12, giro con mensaje perdido: sin eventos.** Cero líneas `mensaje_perdido`, `giro_a_tiempo`, `veto_tof`, `confirma_tof` ni `TOF_PARO` en todo el log de la sesión.

**Máximo del loop (`LOOP?`): NO MEDIDO** (solo por USB, y se pierde al apagar el carro). **Errores, reinicios y watchdog:** ningún `FATAL EXCEPTION` de la app; desde las 21:30 no se vio ningún reinicio del carro que el dueño no provocara.

**Otros hallazgos del día.**
1. **Giroscopio «MAL» tras encender con la powerbank** (visto durante al menos 1 minuto, hasta que se recalibró). Causa más probable por el código (`imu.h`, motivo 4): el sesgo no se pudo medir porque el carro se movió durante los ~3 s de calibración del arranque; el firmware no lo reintenta. Se recuperó con «Recalibrar giroscopio» (sesgo −0.45 °/s). Lo confirmaría repetir un arranque con el carro quieto (en los arranques de las 22:08 salió bien) o leer `IMU:`/`IMU?` por USB tras un arranque movido.
2. **Los primeros giros después de un reposo salen muy distintos de los siguientes**, sobre todo por tiempo (izquierda +28°, derecha −17° y muy irregular). Hipótesis: caída de tensión de las pilas AA o efecto de calentar. La contradice en parte que la 3.ª calibración falló ~2 min después de moverse. Lo confirmaría repetir 5 giros por tiempo tras 3 min de reposo y, mejor, con pilas nuevas o midiendo su tensión.
3. **Calibración con error 6 tres veces.** Por `calgiro.c`, un giro cae a más de 6° (y más del 6 %) de la recta ajustada a los 3 giros de ese lado; los datos por tiempo de arriba (derecha con desv 7.6°) explican que no cuadre. No se sabe qué lado ni qué giro falló porque la app no registra los 6 pares (ms, grados) cuando falla. Lo confirmaría registrar esos pares en el log (cambio de código, pendiente) o repetir la calibración después de calentar el carro con pilas nuevas.
4. **La app apuntaba a `http://127.0.0.1:8085/stream`** (la cámara falsa de pruebas, guardada por un extra de adb) y por eso decía «La cámara no responde» aunque la cámara real contestaba al ping y estaba en la red. Se arregla en Ajustes → Red → restablecer direcciones. No se cambió.
5. **Incidente:** tras la 3.ª calibración el carro chocó (los giros alternados avanzan y yo no pedí reposicionarlo entre corridas). Sin daños: después ambos sensores seguían «ok» con el mismo sesgo.
6. **El puerto serie del DevKit mostró un chorro de «Balance MANUAL cambiado» a ~390 KB/s** (30 veces lo que da 115200 baud) mientras estaba conectado a la PC; el firmware seguía contestando `CAPS?`. Causa no determinada; no se tocó.

**Falta por medir.** Deriva de 60 s, piso vacío con mínimos y máximos y caja a 30 cm exactos (`verificar_sensores.py vacio` y `distancia`, por USB); `ANT?`, `LOOP?` y `IMU?` por USB; giros con cinta para cuantificar el ángulo real (a ojo el dueño ya confirma que coincide con el giroscopio); calibración del giro (necesita el registro de los 6 pares); pruebas 6 a 8 (rutas, caja delante y corte de WiFi).

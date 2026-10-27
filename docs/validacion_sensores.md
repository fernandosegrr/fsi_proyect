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
| 2 Deriva | 18 oct y 20 oct | 0.0° en 60 s (sin tocar) el 18 oct; 0.0° en 60 s por USB el 20 oct | sí |
| 3 Piso vacío | 20 oct | Por la pantalla Sensores (carro en el piso, nada delante): **«libre (más de 1.2 m)» o «1200 mm»** en todas las capturas en que se veía esa línea (de las 21:29 a las 22:09), y ni un veto, confirmación ni paro del láser en el log de toda la sesión. **Por USB** (`verificar_sensores.py vacio`, 5 s, nada delante): 52 lecturas, **mínimo 1200 mm y máximo 1200 mm** (fuera de rango), 52 marcadas libres, **0 falsos obstáculos**. | pasa |
| 4 Caja a 30 cm | 20 oct | **Frente:** con un objeto parecido a la caja (distancia real **no medida con regla**; el dueño confirma que cuadra) el láser marcó **411, 409 y 410 mm** en tres capturas seguidas (±1 mm); no se pudo comparar con los 280-320 mm del enunciado. Por USB (`verificar_sensores.py distancia`, 15 s) con la caja que el dueño dice que está a **37 cm** (antes dijo «creo que 30» y luego «como a 35»; sin regla confirmada): **152 lecturas, mín 367, máx 378, promedio 371 mm**, estable (±6 mm), **+1 mm contra los 370 mm**: el láser acierta a esa distancia. No es la prueba exacta de 30 cm. **Lado:** objeto a 30 cm adelante y 20 cm a la derecha (~34° fuera del eje): **«libre (más de 1.2 m)»** en 5 capturas seguidas (10 s), el láser no reaccionó. `TOF_PARO` no se probó (solo salta dentro de una ruta). | frente: pasa en exactitud (371 mm con la caja a 37 cm, +1 mm), pero no se hizo a 30 cm exactos; lado: pasa |
| 5 Giros 10+10, tiempo y ángulo | 18 oct | **Medidos por el giroscopio** (pedido 90°; algunas tandas con menos de 10 giros porque fallaron toques del script que maneja la app por adb). POR TIEMPO: izquierda 14 giros, media 88.8°, 10 de 14 a ±3°; derecha 17 giros, media 82.4° (se queda corto ~8°: la calibración de ese lado ya no coincide con el carro), 2 de 17 a ±3°. POR ÁNGULO: izquierda 10 giros, media 90.6° (89.7 a 91.7), **10 de 10 a ±3°**; derecha 8 giros, media 92.7° (90.9 a 94.4), 5 de 8 a ±3° (se pasa ~2.7° constantes). **Con cinta** (cuadrado alineado): 4 giros de 90° a la izquierda por ángulo (el giroscopio midió 362.4° en total) dejaron el carro **4.5 cm a la izquierda y 5 cm hacia adelante** de donde empezó (posición, por el desplazamiento del pivote: 4 giros con el mismo pivote se suman; el navegador lo compensa con su modelo). **El ángulo con el que quedó (si quedó paralelo a la cinta) todavía NO se midió con cinta.** **Ángulo de esos 4 giros a la izquierda: alineado con la cinta** (dicho por el dueño, sin cuantificar). **Un solo giro de 90° a la derecha con cinta:** el giroscopio midió **92.7°** y el dueño lo vio en **~92°** (coinciden); el centro se movió **~15 cm hacia adelante y ~16 cm a la derecha**, igual que lo medido en la calibración del pivote derecho (15 y 16.5 cm). **4 giros seguidos a la derecha (giroscopio: 367.1°, unos 7° de más):** el dueño los vio como ~60° de inclinación a la derecha, algo que no cuadra con un giroscopio que acierta un giro suelto a 1°; la causa más probable es que el punto de partida no estuviera bien alineado o la estimación a ojo, pero NO está resuelto: queda pendiente repetirlos con el cuadrado bien alineado. | giro suelto a la derecha y 4 a la izquierda: coinciden con la cinta; 4 seguidos a la derecha sin resolver |
| 6 Ruta en L con y sin giroscopio | | | |
| 7 Caja delante / recta con caja | | | |
| 8 Corte de WiFi | | | |

## Resultados del 20 oct 2026 (J12: pruebas con el carro, hechas por Claude con la app por adb)

**Condiciones.** Carro en el piso con la powerbank y las 4 pilas AA, teléfono en la red CarroFSI (192.168.4.10), todo por la app (pantallas Sensores y Calibrar giro); el DevKit **no** estuvo por USB, así que no hay lecturas por serie. Cada número sale del log `SENS_DIAG`/`CALGIRO` del teléfono o de capturas de esas dos pantallas.

**Versiones verificadas antes de medir.**
- App (`com.fsi.carro` 0.3, versionCode 2): idéntica en sus 980 entradas a un build limpio del HEAD `427b740`. No se reinstaló.
- Firmware: el chip tenía el de **J11 fase B (`09c2b76`, compilado el 18 oct 11:34)**, sin `TOFR`, `ARR?` ni `ANT_BORRAR`. Se cargó el del HEAD con `scripts\cargar.ps1 nuevo` y se verificó con `esptool verify-flash` (MD5 del chip) contra el build del script. Las pruebas físicas de J12 hechas **antes** de esta recarga no valen. Al arrancar ya imprime `CAPS:IMU=1,TOF=1,TOFR=1`.

**Prueba 2, deriva: PASA.** Por USB (`verificar_sensores.py deriva --segundos 60`, carro quieto sobre su cable, con el DevKit ya conectado al PC): **0.0° de desviación en 60 s**.

**Prueba 3, piso vacío, y prueba 4, caja:** ver las filas 3 y 4 de la tabla de arriba.

**Prueba 5, giros de 90° medidos por el giroscopio.**
- **Por ángulo, izquierda (12 giros):** 99.5, 90.8, 89.2, 90.8, 91.7, 91.3, 91.7, 89.4, 89.1, 89.6, 89.1, 88.9. Media 90.9°, mín 88.9°, máx 99.5°. A ±3°: **11 de 12** (9 de los primeros 10: PASA). Sin el primer giro: media 90.2°, desv 1.1°.
- **Por ángulo, derecha (11 giros):** 106.6, 90.7, 90.2, 93.0, 90.8, 93.9, 92.4, 92.2, 90.3, 92.5, 92.3. Media 93.2°, mín 90.2°, máx 106.6°. A ±3°: **9 de 11**, pero solo **8 de los primeros 10: FALLA** el criterio de 9 de 10. Sobregiro medio a la derecha: **+3.2°** con el primer giro y **+1.8°** sin él (media 91.8°, desv 1.2°).
- **Por tiempo, izquierda (10 giros, 918 ms planeados):** 116.6, 120.4, 118.5, 91.2, 92.3, 93.0, 94.5, 94.2, 95.6, 93.4. Media 101.0°, desv 11.6°, **3 de 10** a ±3°. Los 3 primeros se pasan ~28°; los 7 siguientes quedan en 93.5° de media (desv 1.4°). (El criterio de ±3° es del método por ángulo; aquí es informativo.)
- **Por tiempo, derecha (9 giros, 724 ms planeados):** 72.8, 73.2, 86.4, 83.8, 91.4, 90.9, 93.3, 89.5, 94.0. Media 86.1°, desv 7.6°, **3 de 9** a ±3° (informativo); sin los 3 primeros, media 90.5° y desv 3.3°.
- **4 giros seguidos a la derecha (por ángulo):** los 4 primeros sumaron **380.5°** (con el de 106.6°); los 4 del medio (giros 5-8) **369.3°**; los 4 últimos **367.3°**, igual que el 367.1° del 18 oct (unos 7° de más, ~1.8° por giro). El ángulo acumulado de la pantalla coincide con la suma de los giros del log (1093.6° contra 1091.1° tras los 12 a la izquierda), así que no hubo giros ocultos. No se midió con cinta, pero el dueño dijo el 20 oct (a ojo, sin cuantificar) que **el carro sí gira lo que marca el giroscopio**; eso apunta a que el «~60°» a ojo del 18 oct fue un error de estimación y no del giroscopio, y a que el sobregiro de ~2° por giro a la derecha es real.

**Paso 9, calibración del giro con el giroscopio: FALLA (3 de 3 corridas, error 6 «inconsistente»).** Corridas a las 21:45:10, 21:47:43 y 21:56:11, de ~18 s cada una; las tres terminaron con `error=6` y **no guardaron nada**. Por eso **no hay «después»**, no hay aviso de cambio mayor de 10 %, y el ajuste fino de 90° **no** volvió a 1000. El «antes» que tenía guardado la app (perfil AUTO): izquierda 10.2 ms/°, derecha 8.3 ms/°; frenado izquierda 0.0° y derecha 12.2°; ajuste fino del giro de 90° izquierda 1000 y derecha **1119**. No se pudo comparar la corrida 1 contra la 2.

**Paso 10, anticipación.** `ANT?` por USB al final de las pruebas: **`ANT:121,150,20,20`**, o sea AUTO izquierda 121 ms, **AUTO derecha 150 ms (el tope `GIRO_ANTICIPACION_MAX_MS`)**, MANUAL izquierda 20 y MANUAL derecha 20 (sin uso, valores iniciales); los de AUTO se aprendieron hoy, empezando en 20 ms. El primer giro por ángulo de cada sentido se pasa (99.5° y 106.6°) y desde el segundo quedan en 89 a 94°, compatible con una anticipación por sentido que empieza en su valor inicial y se corrige sola.

**Paso 11, arranques del láser: PASA.** El historial del firmware pasó de `anotados=5 con_reintentos=2 detalle=11000` (21:20, arranques por USB y de la recarga) a `anotados=5 con_reintentos=0 detalle=00000` (22:08:46) y la pantalla dice **«0 de 5»**. El dueño encendió y apagó el carro «creo que 5 veces» (no verificado). En la recarga, el primer arranque trajo `TOFR=1` (1 reintento).

**Paso 12, giro con mensaje perdido: sin eventos.** Cero líneas `mensaje_perdido`, `giro_a_tiempo`, `veto_tof`, `confirma_tof` ni `TOF_PARO` en todo el log de la sesión.

**Máximo del loop (`LOOP?`): solo en reposo.** Por USB: `LOOP:1797,0` (máximo 1.8 ms, 0 excesos), pero parece que el DevKit se reinició al pasar a USB (el ángulo volvió a 0), así que ese máximo **no incluye los giros ni el movimiento**; con el carro en marcha NO MEDIDO. **Errores, reinicios y watchdog:** ningún `FATAL EXCEPTION` de la app; desde las 21:30 no se vio ningún reinicio del carro que el dueño no provocara.

**Otros hallazgos del día.**
1. **Giroscopio «MAL» tras encender con la powerbank** (visto durante al menos 1 minuto, hasta que se recalibró). Causa más probable por el código (`imu.h`, motivo 4): el sesgo no se pudo medir porque el carro se movió durante los ~3 s de calibración del arranque; el firmware no lo reintenta. Se recuperó con «Recalibrar giroscopio» (sesgo −0.45 °/s). Lo confirmaría repetir un arranque con el carro quieto (en los arranques de las 22:08 salió bien) o leer `IMU:`/`IMU?` por USB tras un arranque movido.
2. **Los primeros giros después de un reposo salen muy distintos de los siguientes**, sobre todo por tiempo (izquierda +28°, derecha −17° y muy irregular). Hipótesis: caída de tensión de las pilas AA o efecto de calentar. La contradice en parte que la 3.ª calibración falló ~2 min después de moverse. Lo confirmaría repetir 5 giros por tiempo tras 3 min de reposo y, mejor, con pilas nuevas o midiendo su tensión.
3. **Calibración con error 6 tres veces.** Por `calgiro.c`, un giro cae a más de 6° (y más del 6 %) de la recta ajustada a los 3 giros de ese lado; los datos por tiempo de arriba (derecha con desv 7.6°) explican que no cuadre. No se sabe qué lado ni qué giro falló porque la app no registra los 6 pares (ms, grados) cuando falla. Lo confirmaría registrar esos pares en el log (cambio de código, pendiente) o repetir la calibración después de calentar el carro con pilas nuevas.
4. **La app apuntaba a `http://127.0.0.1:8085/stream`** (la cámara falsa de pruebas, guardada por un extra de adb) y por eso decía «La cámara no responde» aunque la cámara real contestaba al ping y estaba en la red. Se arregla en Ajustes → Red → restablecer direcciones. No se cambió.
5. **Incidente:** tras la 3.ª calibración el carro chocó (los giros alternados avanzan y yo no pedí reposicionarlo entre corridas). Sin daños: después ambos sensores seguían «ok» con el mismo sesgo.
6. **El puerto serie del DevKit mostró un chorro de «Balance MANUAL cambiado» a ~390 KB/s** (30 veces lo que da 115200 baud) mientras estaba conectado a la PC; el firmware seguía contestando `CAPS?`. Causa no determinada; no se tocó. No se repitió después: 0 bytes pasivos en 3 s al conectar el DevKit por USB.
7. **`ANT?` derecha AUTO = 150 ms, el tope.** La anticipación (cuánto sigue girando el carro después de frenar) ya no puede crecer más, y a la derecha los giros siguen pasándose ~1.8° (media 91.8° sin el primero); a la izquierda quedó en 121 ms, sin tocar el tope (media 90.2°). Causa más probable por `config.h`: a la derecha hay más inercia o más retraso al frenar de lo que 150 ms puede compensar. Lo confirmaría repetir 10 giros a la derecha con el tope más alto (cambio de `GIRO_ANTICIPACION_MAX_MS`, pendiente) o comprobar que `ANT?` vuelve a 150 tras `ANT_BORRAR` y 10 giros.

**Falta por medir.** Caja a 30 cm exactos medidos con regla (opcional: el láser ya acertó a 37 cm con +1 mm; `verificar_sensores.py distancia`, por USB); `LOOP?` con el carro en marcha; giros con cinta para cuantificar el ángulo real (a ojo el dueño ya confirma que coincide con el giroscopio); calibración del giro (necesita el registro de los 6 pares); pruebas 6 a 8 (rutas, caja delante y corte de WiFi).
## J13 sin movimiento

21 de octubre de 2026, 07:28 a 07:38, hora de Ciudad de México. Motores sin pilas ni powerbank, según declaración del dueño. No se enviaron FORWARD, REVERSE, LEFT, RIGHT, GIRAR ni rutas al hardware, ni se tocaron sus botones de movimiento. PC sin conectarse al WiFi de las placas. No se vio ni guardó imagen de cámara; no hubo capturas de video.

Fuente y pruebas: revisión 4233201, publicada en origin/main. Núcleo 20 programas / 1570 casos; firmware 152+241+15+34, servidor falso 42, siete pruebas del verificador de versiones; todo verde. Sanitizadores success: https://github.com/fernandosegrr/fsi_proyect/actions/runs/39548476207.

Firmware cargado: FW:4233201-20261021, compilado desde esa revisión con cargar.ps1 nuevo. Antes de cargar había una sola placa serie en COM6 y su identidad guardada correspondía al DevKit; se comprobó mediante esptool. Carga correcta a 921600, sin erase-flash. verify-flash confirmó los cuatro archivos (bootloader, particiones, boot_app0, aplicación). La NVS de anticipaciones se conserva: ANT:121,150,20,20 antes y después. No se borraron anticipaciones ni calibraciones.

Lectura USB del 21 oct a las 07:35:11:
- CAPS:IMU=1,TOF=1,TOFR=1,IMUR=0,IMUM=0,IMUE=2,FW:4233201-20261021.
- IMU:0,-48,2,104,0: lista, sesgo -0.48 grados/s, WHO_AM_I 0x68, sin motivo de fallo. El reintento IMUR no fue necesario en este arranque; recuperación tras movimiento probada solo en simulación.
- TOF:253,3: sensor vivo y lectura válida, sin distancia real de referencia para juzgar exactitud. Antes de cargar marcaba 252 mm.
- ANT:121,150,20,20: exactamente igual que antes de cargar. El tope ahora es 250, pero no se giró para aprender nuevos valores.
- ARR:5,4,11101,1,3,0,00000,0: cinco arranques TOF anotados, cuatro con reintento; actual uno. Tres arranques IMU, ninguno con reintentos. Esta lectura es anterior a la comprobación final de reset, que añadió otro arranque. Los reinicios por esptool/detección/carga se incluyen; no es el ensayo de cinco encendidos normales con powerbank.
- LOOP:12858,1: máximo 12.858 ms, un exceso sobre 5 ms. Antes de cargar el máximo era 2.126 ms, cero excesos. Causa no determinada; queda pendiente aislarlo en reposo y medir después en movimiento. No se atribuye a una causa sin evidencia.

APK: se compiló APP:0.4-4233201, versionCode 3. Se comprobó su metadato dentro del APK y que coincide con la revisión del firmware. Se intentó solo adb install -r, conservando datos. Un bloqueo por PIN impidió acceder a la confirmación de instalación. No se escribió ningún PIN ni contraseña. Se detuvo el cliente de instalación pendiente por su PID exacto; dumpsys package seguía indicando app 0.3, versionCode 2. Por ello NO se afirma que la APK nueva esté instalada ni que la pantalla Sensores nueva esté validada. No hubo capturas: la pantalla autorizada no era accesible. La comparación completa de FW y paquete instalado con verificar_version.py queda pendiente hasta que el dueño desbloquee e instale la app nueva.

Cierre: app cerrada, stay-on restaurado a su valor inicial 2 y teléfono apagado con reboot -p (orden aceptada; desapareció de adb). Se intentó deshabilitar solo el DevKit con pnputil: Acceso denegado. No se cortó VBUS; quedó detenido en reset por RTS con el helper PID 19532 (lanzador 14244), comprobando silencio, arranque del firmware al soltar y silencio al volver a afirmar la línea. Para liberarlo, terminar esos PID y desconectar/reconectar USB. Detalle completo en reporte_J13.md y progreso_J13.md.

Reanudación solicitada por el dueño, 21 de octubre de 2026, 07:49 a 08:05 (sustituye ese cierre):
- Helper de reset detenido y ambos PID comprobados ausentes; RTS liberada. DevKit encendido y respondiendo, como pide la nueva solicitud.
- HEAD adbdd13 compilado: FW:adbdd13-20261021 y APP:0.4-adbdd13, app 0.4/código 3. Una placa serie COM6, identidad conocida de DevKit; cargar.ps1 nuevo y verify-flash de los cuatro archivos pasan. Sin borrar NVS ni mover motores.
- USB 07:52:46: CAPS:IMU=1,TOF=1,TOFR=1,IMUR=0,IMUM=0,IMUE=2,FW:adbdd13-20261021; IMU:0,-47,2,104,0; TOF:254,3; ANT:121,150,20,20; ARR:5,5,11111,1,5,0,00000,0. Ambos sensores listos, sesgo -0.47 grados/s, anticipaciones conservadas. Historial incluye reinicios por carga/verificación; no es ensayo con powerbank.
- LOOP inicial:12935,1. Tras LOOP_CERO, 60 s en reposo: 1690 us máximo y cero excesos de 5 ms (1686 us a los 15/30 s). Dos pasadas aislando consultas CAPS?, IMU?, TOF?, ANT? y ARR? dieron máximos entre 1401 y 1764 us, cero excesos. El pico inicial no se reprodujo con esas consultas y sigue sin causa demostrada.
- El dueño encendió/desbloqueó el teléfono y habilitó la conexión USB; ADB apareció a las 08:01. No hizo falta introducir ni guardar el PIN. Antes de tocar la app se guardó stay-on=2 y pantalla encendida. adb install -r terminó con Success, de 0.3/código 2 a 0.4/código 3, sin desinstalar ni borrar datos. Sin toques de confirmación.
- verificar_version.py --puerto COM6 --apk app_android/app/build/outputs/apk/debug/app-debug.apk: PASA. FW:adbdd13-20261021; APP:0.4-adbdd13; versionName=0.4; APK instalada idéntica al build=True; fuentes limpias=True. SHA-256 del APK D8C0577D0E744B173220709B8A526C4F086B4FE01BA2D54FD041DC6576495903. Los cambios posteriores son solo documentos.
- Sensores comprobada mediante capturas autorizadas: versiones correctas; giroscopio ok, 0.2 grados, IMUR=0 e historial IMU 0 de 5; láser ok, 254 mm, TOFR=1 e historial TOF 5 de 5, aviso de revisar cableado visible. El historial incluye reinicios de carga/verificación; no equivale a cinco arranques normales con powerbank. Interruptores de uso encendidos; Autoridad del láser apagada y sin tocar. Capturas locales %TEMP%/j13_sensores_instalada.png y %TEMP%/j13_sensores_estado.png. No se vio ni guardó cámara.
- Teléfono 192.168.4.10 con alcance al DevKit 192.168.4.1 (ping 3.607 ms); PC sin unirse a esa red. USB 08:03:43 con app conectada: CAPS:IMU=1,TOF=1,TOFR=1,IMUR=0,IMUM=0,IMUE=2,FW:adbdd13-20261021; IMU:2,-47,2,104,0; TOF:253,3; ANT:121,150,20,20; ARR:5,5,11111,1,5,0,00000,0; LOOP:8000,4. Cuatro excesos de 5 ms y máximo 8 ms, causa pendiente. La ventana aislada en reposo pasó, pero el límite global de 5 ms no se da por pasado.
- Cierre actualizado: app cerrada con force-stop, stay-on restaurado y comprobado en 2, pantalla encendida como al inicio; UI temporal retirada. Sin helper de reset ni diagnósticos/Gradle activos. Ambos dispositivos quedan encendidos por la petición nueva. Mantener Autoridad del láser apagada hasta validar físicamente.

## J13 con movimiento

21 de octubre de 2026, 20:03 a 20:25, hora de Ciudad de México. Firmware FW:adbdd13-20261021 y APK APP:0.4-adbdd13 (iguales, comprobado en la pantalla Sensores). Carro en el piso con pilas y powerbank, DevKit sin cable USB; el teléfono controló el carro por la red del propio carro y la PC lo manejó por adb. Solo se pulsaron los botones 90° izq., 90° der. y PARO de la pantalla Sensores. No hubo cámara, ni video, ni Autoridad del láser, ni modelo de objetos. Todos los ángulos son los que midió el giroscopio del carro; no hubo cinta ni transportador, así que el ángulo físico real no se verificó. Detalle en reporte_J13_fisico.md y en bitacora.md.

Resultado por prueba:
- Calibración de giros, corrida 1, corrida 2 y botón Deshacer: NO MEDIDO. No se ejecutó por seguridad: la secuencia de ocho giros alterna lados (40/40/40/40/70/70/100/100) y, con los desplazamientos de pivote dados, el centro llegaría a 61.8 cm tras el giro 6 y a 97.8 cm tras el giro 8, más de 60 cm.
- Giros de 90° a la izquierda, 12 giros (3 bloques de 4): PASA. Todos con estado OK y los 12 dentro de ±3° de 90°. Media 90.16°, desviación estándar 1.06°, mínimo 88.5°, máximo 91.8°, sobregiro medio +0.16°. Sumas por bloque: 360.1°, 360.3°, 361.5° (cierran 360° ± 15°).
- Giros de 90° a la derecha, 12 giros (3 bloques de 4): PASA con una observación. Todos con estado OK; de los primeros 10, 9 dentro de ±3° (el giro 21 dio 94.0°); de los 12, 11. Media 90.32°, desviación estándar 1.71°, mínimo 88.4°, máximo 94.0°, sobregiro medio +0.32°. Sumas por bloque: 357.6°, 361.3°, 364.9° (cierran 360° ± 15°, pero crecen de bloque en bloque; medias de bloque 89.4°, 90.3°, 91.2°).
- Primer giro tras reposo de 3 min: PASA (ya no es atípico, con n=1 por lado). Izquierda 91.5° contra 90.04° de media de los otros 11; derecha 89.2° contra 90.42° de los otros 11. Quedan dentro de ±3° y con signos contrarios; con un solo giro por reposo no es concluyente. Primeros de bloque tras la pausa: izquierda 89.5° y 91.8°; derecha 89.3° y 94.0°.
- Duración de los giros: izquierda media 696 ms (638 a 748); derecha media 810 ms (758 a 870), unos 16% más larga.
- Comparación con J12 (sección anterior): la derecha ya no se pasa de 1.8° sino de +0.3° (media sin el primer giro 91.8° antes, 90.42° ahora); la izquierda sigue en +0.2°. Posible causa: el tope de anticipación subió de 150 a 250 ms y el arranque automático es de 100 ms. No se pudo confirmar porque ANT no aparece en el log de la app.
- Anticipación (ANT): NO MEDIDO desde el teléfono. Última lectura por USB: ANT:121,150,20,20; falta leer ANT? por USB después de estos 24 giros.
- LOOP: NO MEDIDO desde teléfono (la app no consulta LOOP?). Requiere USB al DevKit, que no se conectó.
- Giroscopio: ok durante toda la prueba, sin necesidad de Recalibrar. Láser ok, sin usarse como autoridad.
- Conexión del teléfono con el carro: sin pérdidas; PING con latencia de 6 a 30 ms.

Anomalías:
1. Los botones de giro se desplazan en pantalla cuando el texto de resultado pasa de una a dos líneas (el 90° izq. baja de y≈2215 a y≈2297) y un toque de coordenadas fijas puede no registrarse. Causa más probable: el diseño de la pantalla (el texto de resultado no reserva altura). Lo confirmó el dueño ("la pantalla se mueve") y las capturas antes y después. Se confirmaría con minLines fijo en ese texto; no se cambió código.
2. Giros derechos más largos y con más dispersión que los izquierdos (810 ms contra 696 ms; desviación 1.71° contra 1.06°; giro 21 a 94.0°). Causa más probable: más inercia en el lado derecho, como ya se vio en J12. Se confirmaría leyendo ANT? por USB y repitiendo 10 giros a la derecha con cinta.
3. Suma de bloques a la derecha creciente (357.6°, 361.3°, 364.9°). Causa más probable: la anticipación AUTO adaptándose (mediana de cinco) durante la prueba. Se confirmaría comparando ANT? antes y después de 12 giros.
4. Incidencias de mi guion de apoyo (no del producto), detalladas en bitacora.md: PARO y cierres de la app con el carro quieto después de los giros 1, 4 y 5, y pausas entre bloques de la izquierda de unos 2 min y 1 min en vez de 20 s.

Cierre: PARO, am force-stop com.fsi.carro, stay_on_while_plugged_in=2 (valor original) comprobado, pantalla encendida como al inicio, sin wake-locks ni procesos de la app. No se cambió código ni firmware ni se instaló nada. No se midió físicamente dónde quedó el carro tras los 24 giros.

Lectura por USB posterior a los giros, 21 de octubre de 2026, 20:36 a 20:38 (actualiza los puntos «ANT» y «LOOP» de arriba).
- El dueño conectó el DevKit a la PC con un cable corto; por eso el carro no se movió y solo hice consultas de lectura por COM6 (única placa serie), con DTR y RTS apagados. La placa sí se reinició al conectar el USB: ARR cambió y el LOOP volvió a mostrar el pico de arranque.
- 20:36:41: CAPS:IMU=1,TOF=1,TOFR=0,IMUR=0,IMUM=0,IMUE=2,FW:adbdd13-20261021; IMU:0,-47,2,104,0 (sesgo -0.47 grados/s, sin cambio); TOF:1200,1 (libre); ANT:126,189,20,20; ARR:5,2,11000,0,5,0,00000,0; LOOP:12856,1.
- ANT (AUTO izquierda, AUTO derecha, MANUAL izquierda, MANUAL derecha): antes de los giros 121,150,20,20; ahora 126,189,20,20. La izquierda subió 5 ms y la derecha 39 ms; MANUAL no cambió (no se usó) y ninguno llegó al tope de 250 ms. La derecha ya salió del tope anterior de 150 ms, lo que apoya (no demuestra) que el sobregiro derecho bajó de 1.8° a 0.3° por eso. No explica que las sumas por bloque a la derecha crecieran (357.6°, 361.3°, 364.9°): con más anticipación se esperaría menos sobregiro. Solo tengo el valor final guardado, no su trayectoria durante la prueba: esa anomalía sigue sin causa determinada.
- LOOP: el de los 24 giros NO se pudo leer, porque el reinicio al conectar el USB borró los contadores (viven en RAM). El pico inicial de 12.856 ms con un exceso de 5 ms aparece en los tres arranques observados (12.858, 12.935 y 12.856 ms): es del arranque, no de la marcha. Tras LOOP_CERO, en reposo, con la app cerrada y sin cliente WiFi: 1684 us a los 15 s, 1717 us a los 30 y 60 s, cero excesos. LOOP con el carro en marcha y la app conectada: NO MEDIDO; hace falta conectar el USB antes de mover el carro (cable largo, o carro con las ruedas en el aire) y no reiniciarlo.
- Sobre el ángulo real: el dueño indica que el giroscopio es muy exacto; los ángulos de arriba se toman como reales, sin que yo los haya comprobado con cinta.
- Sin cambios en firmware ni en NVS. LOOP_CERO solo pone a cero dos contadores en RAM. Sin movimiento.

LOOP con el carro en el aire y la app conectada, 21 de octubre de 2026, 20:48 a 20:51 (actualiza el punto «LOOP en marcha» de arriba). El dueño dejó el carro sobre un soporte con las ruedas en el aire y el USB conectado; solo se pulsaron 90° izq. y 90° der. de la pantalla Sensores (giro por ángulo).
- Fase A, app conectada y carro en reposo, 60 s tras LOOP_CERO: LOOP:8431,2 (máximo 8.431 ms, dos excesos de 5 ms). Antes, con la app conectada, ya se había visto 8.000 ms y cuatro excesos; sin cliente WiFi, en reposo: 1.717 ms y cero excesos.
- Fase B, 36 s con la app conectada y cinco giros (cuatro a la izquierda y uno a la derecha): LOOP:3251,0 (máximo 3.251 ms, cero excesos) con los motores encendidos unos 9 s en total. Lectura final: la misma, LOOP:3251,0.
- Giros 1 a 4 (izquierda): GIRO_TIMEOUT a 1850 ms, con 1.1, 0.3, 0.4 y 0.0 grados medidos, como se espera con las ruedas en el aire. Giro 5 (derecha): GIRO_TIMEOUT a 1850 ms pero con 71.3 grados medidos: el carro sí giró. Causa no determinada (puede haber rotado sobre su soporte o una rueda tocó algo; el giro izquierdo no lo hizo). Abortó el guion: PARO, am force-stop. Faltan los giros 6 a 8 y el movimiento real del soporte está por revisar por el dueño.
- ANT no cambió (126,189,20,20): GIRO_TIMEOUT no corrige la anticipación, solo GIRO_OK. IMU:211,-47,2,104,0 al final (ángulo 21.1 grados, el giroscopio sigue vivo). Las consultas ANT? y LOOP? del inicio de esta prueba no recibieron respuesta (None) y luego todo respondió; causa no determinada, no se repitió.
- Lectura: el pico de LOOP (8 ms, dos excesos por minuto) aparece con la app conectada y el carro en reposo, y en los 36 s con motores encendidos no apareció. Es una muestra corta, así que el límite de 5 ms NO se da por pasado con la app conectada. Causa más probable: el tráfico WebSocket de la app (PING y MODO_AUTO cada pocos segundos, flujo SENS), no los motores. Se confirmaría con ventanas largas (10 min) con la app conectada en reposo contra el mismo tiempo con motores, y con WiFi sin cliente (ya medido: 1.7 ms).
- Cierre: app cerrada con force-stop, stay_on_while_plugged_in=2 comprobado, sin cambios en firmware ni NVS.

LOOP con el carro en el piso y el cable USB de 1 m, 21 de octubre de 2026, 20:54 a 20:56 (actualiza el punto «LOOP en marcha» de arriba). Carro en el piso con 1 m despejado, DevKit por USB a la PC, app conectada en Sensores, LOOP_CERO antes de empezar. Bloque de cuatro giros a la izquierda y después cuatro a la derecha (el segundo bloque no se llegó a hacer).
- Giros a la izquierda: 85.9, 94.4 y 91.6 grados (642, 706 y 674 ms), todos OK. Giro 4: GIRO_TIMEOUT a 1850 ms con solo 24.3 grados medidos de 90: el carro no completó el giro. Causa más probable: el cable USB se atoró o se tensó (el carro se movió más lejos o se enredó), o una rueda quedó trabada. Se confirmaría revisando cómo quedó el cable y repitiendo con más holgura. Se abortó por la regla de 25 grados: PARO y am force-stop. Los motores estuvieron como máximo 1850 ms atascados por el límite del firmware.
- LOOP tras 3 giros OK y 1 giro atascado, con la app conectada: LOOP:6775,3 (máximo 6.775 ms, tres excesos de 5 ms) en unos 40 s con la app conectada y motores encendidos unos 8 s. Con el carro en el aire: 3.251 ms y cero excesos; en reposo con la app conectada: 8.431 ms y dos excesos. Con tan pocos datos no se puede atribuir los excesos a los motores, a la app o al cable; el límite de 5 ms NO pasa en esta prueba.
- ANT sin cambios (126,189,20,20) tras los tres giros OK; IMU:3635,-47,2,104,0 (el ángulo acumulado del giroscopio, sin importancia). Las consultas ANT? e IMU? de una pasada no recibieron respuesta y a la siguiente sí; ya había pasado antes, causa no determinada.
- Cierre: app cerrada con force-stop, stay_on_while_plugged_in=2, sin cambios en firmware ni NVS. No se repitió el giro 4 ni se hizo el bloque de la derecha. Pendiente: que el dueño revise la posición del carro y del cable.

Segundo intento en el piso con el cable USB, 21 de octubre de 2026, 20:57 a 21:00 (sustituye la causa probable del giro atascado de arriba). El dueño había acomodado el cable y confirma que durante el bloque de la derecha agarraba el carro con la mano.
- Bloque a la izquierda, con LOOP_CERO antes: 90.3, 88.0, 91.3 y 86.5 grados (661, 631, 673 y 695 ms), todos OK; suma 356.1. LOOP tras ese bloque: LOOP:6783,2 (máximo 6.783 ms, dos excesos de 5 ms, unos 35 s con la app conectada y motores encendidos unos 3 s).
- Bloque a la derecha: giro 5 OK con 86.8 grados pero en 1196 ms (lo normal es unos 800 ms) y giro 6 GIRO_TIMEOUT con 79.1 grados: ambos NO son válidos como medida del giro porque el carro estaba sujeto con la mano. Se abortó por la regla de estado distinto de OK. El LOOP no cambió después de la izquierda (6783,2).
- Efecto en la anticipación: el giro 5 terminó con GIRO_OK y corrigió ANT con un dato falso: ANT pasó de 126,189,20,20 a 126,144,20,20 (la derecha AUTO bajó 45 ms). La izquierda quedó igual tras sus cuatro giros válidos. Esa anticipación derecha de 144 ms es un valor contaminado, menor que los 189 ms aprendidos con giros válidos; los siguientes giros a la derecha se pasarán más hasta que se reaprenda. No se tocó (ANT_BORRAR escribiría en NVS y no se autorizó). Se corregiría con dos o tres bloques de cuatro giros válidos a la derecha, sin tocar el carro.
- Con esta explicación, el atasco del primer intento (giro 4, 24.3 grados) también pudo ser por manipular el carro o el cable; no se confirmó cuál.
- LOOP en el piso, resumen de las tres mediciones con la app conectada: carro en el aire 3.251 ms y cero excesos; reposo 8.431 ms y dos excesos; piso 6.783 ms y dos excesos. Siguen siendo pocos datos: el límite de 5 ms NO pasa y la causa se mantiene como el tráfico de la app (sin demostrar).
- Cierre: app cerrada con force-stop, stay_on_while_plugged_in=2. Sin cambios en firmware; la NVS cambió solo por el aprendizaje normal de ANT.

## J14 con movimiento (27 de octubre de 2026)

Condiciones: FW:453ed20-20261027 y APP:0.4-453ed20 (después se instaló la APK final de J14 con los cambios de código de esta sesión); teléfono Huawei ELS-NX9; carro con pilas y powerbank; app manejada por ADB con identificadores estables. No se vieron ni guardaron imágenes de la cámara; solo se usaron las pantallas Sensores y Calibración. El dueño colocó el carro y las cajas y dio LISTO antes de cada corrida.

B0 a B3, sin movimiento: versiones y sensores correctos; ANT 126,144,20,20 ms al inicio (B1 terminó en 126,142,20,20; B3 no volvió a leer ANT). Tres bloques de cuatro giros a la derecha en B1 y dos bloques por lado en B3, todos dentro de ±3°. B2 no se midió porque la calibración instalada no publica los ángulos de cada giro hasta terminar los ocho.

B4, recta de 150 cm, 4 corridas:
- Autoridad APAGADA: desvío lateral ~15 cm y ~20 cm; 150 cm recorridos; 5 s; 0 esquivas; sin paros ni eventos STOP/TOF.
- Autoridad ENCENDIDA: desvío ~5.5 cm y ~18 cm; 150 cm recorridos; 5 s; 0 esquivas; sin paros.
- 4 de 4 rutas terminadas; 0 falsos paros en piso vacío.

B5, ruta en L (100 cm, giro 90° a la derecha, 100 cm), 3 corridas:
- Con giroscopio: 7 s y 8 s, 0 esquivas, giros de ~91° y ~90°; posición final estimada x=1406 mm, y=209 mm, rumbo 91° y x=1389 mm, y=203 mm, rumbo 90°.
- Sin giroscopio: 7 s, 0 esquivas, giro por tiempo ~93°; posición final estimada x=1394 mm, y=244 mm, rumbo 93°.
- Las 3 corridas válidas terminaron. El dueño no dio una cifra de error final; contra el objetivo y=0, las estimaciones de navegación dejan un error lateral de ~20 a 24 cm. Una corrida previa con un sillón al final de la L hizo una esquiva y terminó con rumbo 324°; no se cuenta como corrida limpia.

B6, caja delante, ruta Recta 200 cm, 4 corridas:
- Con la caja a 40 cm la app no arrancó: el aprendizaje del piso detectó la caja y pidió despejar; el carro no se movió. Es una protección de la app, no un fallo.
- La caja quedó finalmente a ~100 cm del frente (el dueño aclaró que todas las corridas siguientes fueron a ~100 cm, no a 70 cm).
- Autoridad APAGADA: corrida 1, 23 s, 2 esquivas, sin tocar la caja; terminó en x=394 mm, y=5 mm, rumbo 117°, pero al reincorporarse detectó un obstáculo lejano y no volvió limpio. Corrida 2, 56 s, 3 esquivas, terminó en «Necesito ayuda: toma el control» en el punto 3/10; el dueño cree que sí tocó la caja.
- Autoridad ENCENDIDA: corrida 3, 26 s, 2 esquivas, sin tocar; x=393 mm, y=-12 mm, rumbo 119° (cerca del objetivo pero mal orientado). Corrida 4, 15 s, 1 esquiva, sin tocar; x=409 mm, y=6 mm, rumbo 0°, pero el dueño la vio 40 cm a la derecha de la línea recta.
- El algoritmo de esquiva no hizo el rodeo limpio que el dueño esperaba (90°, 50 cm, 90°, 50 cm, 90°): durante la reincorporación se enredó con obstáculos lejanos. Es un fallo real de la vuelta a la ruta, no falta de espacio.

B7, B8, B9 y B2: no realizadas por decisión del dueño el 27 de octubre. B7 se consideró cubierta por B6 porque la caja ya estaba a ~100 cm; B8 (caja lateral) y B9 (corte de WiFi) se cancelaron; B2 queda NO MEDIDA. Por eso no se puede afirmar que el watchdog de WiFi, la caja lateral o la calibración nueva por bloques estén validados físicamente en J14.

B10: el dueño decidió que la Autoridad del láser quede siempre ENCENDIDA. No es una conclusión de una comparación pareada B4-B8; es una decisión del dueño y así se registra. Se cambió el valor por defecto de la app a true.

Cierre de J14, 27 de octubre de 2026, ~15:10:
- La APK final es APP:0.4-699fcc5 (14,500,144 bytes, SHA-256 C50A9757DE59B1B604680BE9731D8C610F9EC4A66780EB18ECEB558E6E00312A), instalada con adb install -r y verificada por hash al extraerla del teléfono. Sensores muestra App 0.4 (699fcc5), los dos sensores encendidos y la Autoridad del láser encendida.
- El firmware de entrega es FW:453ed20-20261027 (1,031,712 bytes, SHA-256 9FD1BB5103D1AD66652ADEA68B009D54D30B5C5B6F07F8CA7488A4FBF274048E), cargado en la Etapa 4. La lectura final por Sensores no se pudo repetir porque el punto de acceso CarroFSI no estaba visible y no había puerto COM; no se inventa ese resultado.
- El teléfono quedó con la app cerrada por am force-stop, stay_on_while_plugged_in=2, pantalla encendida, WiFi habilitado y batería al 82 % cargando por USB. Quedó en la red normal INFINITUM porque CarroFSI no estaba visible; no se escribió ninguna contraseña ni PIN. El carro quedó quieto y apagado.
- Se conservan solo capturas de Sensores y Calibración. Dos capturas de la pantalla de rutas incluyeron por error la vista en vivo de la cámara y se borraron inmediatamente; no se guardaron ni publicaron imágenes reales.

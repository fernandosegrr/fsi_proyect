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
| 3 Piso vacío | | | |
| 4 Caja a 30 cm | | | |
| 5 Giros 10+10, tiempo y ángulo | 18 oct | **Medidos por el giroscopio** (pedido 90°; algunas tandas con menos de 10 giros porque fallaron toques del script que maneja la app por adb). POR TIEMPO: izquierda 14 giros, media 88.8°, 10 de 14 a ±3°; derecha 17 giros, media 82.4° (se queda corto ~8°: la calibración de ese lado ya no coincide con el carro), 2 de 17 a ±3°. POR ÁNGULO: izquierda 10 giros, media 90.6° (89.7 a 91.7), **10 de 10 a ±3°**; derecha 8 giros, media 92.7° (90.9 a 94.4), 5 de 8 a ±3° (se pasa ~2.7° constantes). **Con cinta** (cuadrado alineado): 4 giros de 90° a la izquierda por ángulo (el giroscopio midió 362.4° en total) dejaron el carro **4.5 cm a la izquierda y 5 cm hacia adelante** de donde empezó (posición, por el desplazamiento del pivote: 4 giros con el mismo pivote se suman; el navegador lo compensa con su modelo). **El ángulo con el que quedó (si quedó paralelo a la cinta) todavía NO se midió con cinta.** **Ángulo de esos 4 giros a la izquierda: alineado con la cinta** (dicho por el dueño, sin cuantificar). **Un solo giro de 90° a la derecha con cinta:** el giroscopio midió **92.7°** y el dueño lo vio en **~92°** (coinciden); el centro se movió **~15 cm hacia adelante y ~16 cm a la derecha**, igual que lo medido en la calibración del pivote derecho (15 y 16.5 cm). **4 giros seguidos a la derecha (giroscopio: 367.1°, unos 7° de más):** el dueño los vio como ~60° de inclinación a la derecha, algo que no cuadra con un giroscopio que acierta un giro suelto a 1°; la causa más probable es que el punto de partida no estuviera bien alineado o la estimación a ojo, pero NO está resuelto: queda pendiente repetirlos con el cuadrado bien alineado. | giro suelto a la derecha y 4 a la izquierda: coinciden con la cinta; 4 seguidos a la derecha sin resolver |
| 6 Ruta en L con y sin giroscopio | | | |
| 7 Caja delante / recta con caja | | | |
| 8 Corte de WiFi | | | |

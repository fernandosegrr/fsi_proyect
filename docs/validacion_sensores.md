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

## Resultados (se llena al probar)

| # | Fecha | Resultado | ¿Pasa? |
|---|---|---|---|
| 1 Polaridad | | | |
| 2 Deriva | 18 oct | 0.0° en 60 s (sin tocar) | sí |
| 3 Piso vacío | | | |
| 4 Caja a 30 cm | | | |
| 5 Giros 10+10, tiempo y ángulo | | | |
| 6 Ruta en L con y sin giroscopio | | | |
| 7 Caja delante / recta con caja | | | |
| 8 Corte de WiFi | | | |

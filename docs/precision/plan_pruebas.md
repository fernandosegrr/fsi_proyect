# Plan de pruebas físicas J16 (precisión)

Para cuando el dueño decida probar. Nada de esto se ha medido: todo lo de J16 está SIMULADO. Los criterios de aceptación salen de la simulación (`docs/precision/monte_carlo_1000.txt`) con margen, porque la simulación no prueba que el carro mejore.

## Antes de empezar

- Piso liso, una línea de cinta de al menos 2.5 m y 1 m libre a cada lado. Una marca de salida en la cinta.
- Cinta en el centro del techo del carro (para medir dónde quedó el centro) y una regla o escuadra.
- Pilas cargadas; anotar la carga al empezar y al terminar cada bloque.
- Botón PARO a la mano. Recolocar el carro en la marca antes de CADA corrida.
- Registro: después de cada bloque, `adb logcat -d -s SENS_DIAG:I RUTA_DIAG:I TURN_DIAG:I REINC_DIAG:I ESQUIVE_DIAG:I NAV:I` (líneas `POSE_DIAG` incluidas en `SENS_DIAG`/`RUTA_DIAG`). Sin imágenes de la cámara. Con RUMBO, al final de cada ruta `SENS_DIAG` trae `ruta: rumbo_objetivos_enviados=N` y `firmware: RUMBO:<encendido>,<en marcha>,<objetivo x10>,<corrección>,<aprendido>`: anotar la corrección aprendida de cada corrida (es el desbalance de los lados que encontró el firmware).
- Cómo medir cada corrida: (1) desvío lateral = distancia del centro del carro a la cinta, en cm, con signo (+ derecha mirando hacia donde avanza); (2) largo = distancia desde la marca de salida, en cm; (3) rumbo final = ángulo entre el costado del carro y la cinta, en grados (con la escuadra o midiendo la distancia a la cinta de las dos esquinas delanteras). En la L, el cuadrado y los esquives, además, la distancia del centro al punto final.

## Orden

1. **P0. Instalación y versión (sin movimiento).** `adb install -r dist/carro_fsi_0.4-097008d.apk`. En Sensores debe decir `App 0.4 (097008d)`. Con el firmware de siempre (`FW:453ed20-20261027`) el registro al iniciar una ruta dice `ruta: rumbo_fw=false (caps=false ...)`: la app no manda nada de RUMBO. Acepta: versión correcta y `caps=false`.

2. **P1. Línea base con el firmware de siempre y la APK nueva.** Ruta «Recta 150 cm», 4 corridas. Mide desvío, largo y rumbo. Qué cambió sin firmware: ahora el largo debe ser ~150 cm (antes la app perdía el largo fijo y hacía las 8 celdas, 160 cm). Acepta: largo medio entre 145 y 155 cm; el desvío servirá de comparación (en J14 fue de 5.5 a 20 cm). Simulación sin firmware nuevo: desvío medio 7.4 cm, p95 11.7 cm.

3. **P2. Cargar el firmware nuevo (con el carro en el aire o quieto).** `.\scripts\cargar.ps1 nuevo -Puerto COM6`. Después `python scripts\verificar_version.py --puerto COM6 --apk dist\carro_fsi_0.4-097008d.apk` (o Sensores): debe decir `FW:097008d-<fecha>`, CAPS con `RUMBO=1` y, en Sensores, «Rumbo al avanzar: disponible · en uso en las rutas». Al iniciar una ruta el registro dice `ruta: rumbo_fw=true`. Acepta: versión y `RUMBO=1`. Si el giroscopio no queda listo (`IMU=0`), RUMBO no actúa: no seguir hasta resolverlo.

4. **P3. Primera recta con RUMBO (corta).** «Recta 100 cm» con el interruptor «Sostener el rumbo al avanzar» encendido, 2 corridas, mirando que no zigzaguee. Acepta: sin zigzag visible, sin tirones raros y desvío menor de 5 cm (simulación: media 0.8 cm, p95 1.9 cm, máximo 2.7 cm). Si zigzaguea: apagar el interruptor y anotar; es señal de ganancia alta (ver «Si algo sale mal»).

5. **P4. Comparación pareada de rectas.** «Recta 150 cm», 5 corridas con el interruptor APAGADO y 5 ENCENDIDO, alternando (apagado, encendido, apagado...), mismas pilas. Mide desvío, largo y rumbo. Acepta: con RUMBO, desvío medio de 6 cm o menos, ninguno mayor de 10 cm, y la media con RUMBO a la mitad o menos que sin él. Simulación: sin RUMBO 7.4 cm de media; con RUMBO 1.0 cm de media, p95 2.6 cm, máximo 3.5 cm. Un desvío grande con el rumbo final casi en cero indicaría deslizamiento de lado (mecanum): eso RUMBO no lo corrige y hay que anotarlo.

6. **P5. Recta larga.** «Recta 200 cm», 3 corridas con RUMBO. Acepta: desvío de 8 cm o menos (simulación: media 1.4 cm, p95 3.3 cm, máximo 4.9 cm).

7. **P6. Escala del giroscopio y giros.** (a) Antes de corregir: en Sensores, un bloque de 4 giros de 90° a la izquierda y otro a la derecha; tras cada bloque (360°) mide el rumbo contra la cinta y anótalo. (b) Recalibra el giroscopio y, en la tarjeta «Escala del giroscopio», mide la escala 3 veces sin guardar (Empezar, dos vueltas completas a mano sin levantar el carro, alinear, Terminé) y anota cada corrección que propone; luego mide una cuarta vez y toca Guardar en el carro. (c) Repite los bloques de (a). Acepta: las 3 mediciones dentro de ±3000 ppm entre sí (si no, la alineación no es repetible: anotarlo y no guardar), CAPS dice `IMUS=<la guardada>` y, tras corregir, el carro paralelo a la cinta a ±2° tras cada bloque. Simulación (giro de 180° con RUMBO): sin calibrar 4.1 / 8.8 / 13.9 cm (media / p95 / máximo); con la escala calibrada y un residuo supuesto de ±0.3 %, 3.5 / 6.8 / 10.0 cm.

8. **P7. Ruta en L.** 3 corridas con RUMBO (y 3 sin RUMBO si hay tiempo). Mide la distancia del centro a la línea del segundo tramo y al punto final. Nota: los «20 a 24 cm» de J14 no fueron una medición (comparaban la pose estimada contra y = 0 cuando el tramo final está en y = 200 mm); la L nunca se midió. Acepta: con RUMBO, distancia al punto final de 8 cm o menos (simulación con la escala calibrada: media 3.4 cm, p95 6.8 cm; sin RUMBO 4.4 cm de media; J14 14.6 cm).

9. **P8. Cuadrado cerrado.** No hay ruta predefinida: dibujarlo en el editor, 60 cm por lado, de (2,6) a (2,3), (5,3), (5,6) y de vuelta a (2,6). 3 corridas con RUMBO. Mide la distancia del centro al punto de salida. Acepta: 10 cm o menos (simulación con la escala calibrada: media 2.1 cm, p95 4.3 cm; sin calibrarla 2.6 / 5.9 cm).

10. **P9. Esquive con caja de 20 x 20 cm.** «Recta 200 cm» con la caja centrada en la cinta a 100 cm de la salida (entre centros), 5 corridas con RUMBO. Mide toques, si pidió ayuda y el desvío final. Acepta: 0 toques, 0 ayudas, desvío final de 8 cm o menos (simulación: 0 toques en 1000, media 1.7 cm, p95 4.0 cm, máximo 6.3 cm; J14 tuvo ~40 cm en B6-4). Repetir 2 corridas sin RUMBO para comparar.

11. **P10. Caja ancha (40 x 20 cm).** Igual que P9 pero con la caja de x = -10 a +30 cm respecto de la cinta (tapa la línea y se sale a la derecha), 3 corridas. Acepta: 0 toques y desvío final de 8 cm o menos (simulación: media 1.7 cm, p95 4.0 cm). Una caja de 60 cm centrada tapa los tres sectores de la cámara: el carro pide ayuda, como está diseñado.

12. **P11 (opcional). Corte de WiFi en una recta con RUMBO.** Debe frenar por el watchdog (~20 cm). La app detiene la ruta: «Sin datos del carro por más de 1.5 s» (enlace muerto) o «El carro se frenó solo» (si llega el aviso `WD`). Al reconectar, CAPS vuelve a anunciar RUMBO pero el lazo queda apagado hasta la siguiente ruta. Acepta: el carro no vuelve a arrancar solo al regresar el WiFi.

13. **P12 (opcional). Freno del láser.** «Recta 100 cm» con una caja a 60 cm de la salida y el láser en uso. Acepta: el carro frena (o esquiva) sin tirones; en el registro, `TOF_PARO` seguido de la decisión del navegador y ningún `FORWARD` repetido después del freno.

## Qué anotar en cada corrida

Hora, pilas, ruta, interruptor RUMBO (sí o no), desvío lateral (cm, con signo), largo (cm), rumbo final (°), distancia al punto final (cm), toques, ayudas, observaciones (zigzag, tirones, deslizamiento). Pose final que muestra la app.

## Si algo sale mal

- Zigzag o tirones con RUMBO: apagar el interruptor (vuelve exactamente a lo de antes). Si hace falta, bajar `RUMBO_KP_MIL_GRADO` de 10 a 6 en `firmware_carro/config.h`, recompilar y repetir P3.
- Peor con RUMBO que sin él: apagar el interruptor y conservar los registros.
- Volver todo a v1.0: `git switch --detach v1.0`, cargar el firmware 453ed20 (`dist/FIRMWARE_LOCAL.txt`) e instalar la APK de esa etiqueta.
- Las banderas de cada cambio están en `docs/reporte_J16.md`.

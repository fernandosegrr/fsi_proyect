# Pruebas y resultados

Los resultados se interpretan según el entorno en el que se obtuvieron. Una prueba en computadora comprueba una parte de la lógica; una prueba física incluye además el vehículo, el piso y las condiciones de operación.

## Pruebas físicas registradas

El registro del ajuste MANUAL describe una recta de 150 cm y un desvío de 3 cm con los valores 252 y 255. En pruebas posteriores se observaron diferencias de rumbo y comportamiento durante esquives. En una de cuatro corridas históricas con caja se observó que el carro la evitó pero terminó aproximadamente 40 cm a la derecha; otra probablemente tocó la caja. Estos resultados corresponden a la versión y condiciones de esas pruebas, no a la corrección posterior.

En el registro del ciclo del firmware se observaron máximos de 8.431 ms en reposo con la app conectada y de 6.775 ms en una prueba en piso, ambos con excesos del umbral de 5 ms. La muestra es reducida y la causa no quedó demostrada.

## Software y simulación

El reporte J15 registra 1598 casos del núcleo y 447 de firmware sin fallos en las ejecuciones documentadas. También registra una prueba de estrés de 100000 comandos sin pérdida de STOP. Esos resultados son registros históricos de software; no son mediciones físicas del vehículo.

El escenario de esquive **SIMULADO** usó 400 semillas por versión. Tras la corrección, el error lateral medio fue 1.9 cm y el percentil 95 fue 4.6 cm; antes fueron 13.2 y 21.9 cm. El modelo usa supuestos y no reproduce todos los efectos del piso ni toda la percepción real.

## Pendientes físicos

La corrección J15 no tuvo corridas físicas con movimiento. El criterio de cinco de seis corridas sin contacto y con desvío final menor de 25 cm está **NO MEDIDO**. También permanecen sin medir el balance AUTO, el consumo eléctrico y la eficacia del circuito asociado a GPIO 2.

Dónde está en el código: `pruebas_reincorporacion_j15.c`, `simulador_navegador.c`, `config.h`.

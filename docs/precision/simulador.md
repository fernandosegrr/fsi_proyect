# Simulador de precisión J16

Todo lo de este documento es SIMULADO. Sirve para comparar versiones del software con las mismas condiciones; no reemplaza las pruebas en el carro (`plan_pruebas.md`).

## Qué es

- `nucleo_c/simulador_navegador.c` con el modelo físico J16 activado (`sim_j16_activar`). Con `j16_activo = 0` el simulador hace exactamente lo de antes (las 380 pruebas del navegador no cambian).
- `nucleo_c/pruebas_precision_j16.c`: el «mundo» independiente del estimador, el Monte Carlo y las pruebas de la réplica y de la pose.
- `make test` (en `nucleo_c`) lo corre con 150 corridas por escenario y configuración y comprueba que nada empeore. `make precision` lo corre con 1000 (unos 20 s). `./pruebas_precision_j16 calibrar` muestra solo la configuración J14 contra lo medido. `./pruebas_precision_j16 depurar <escenario> <config> <corrida>` imprime una corrida con su registro.

## Modelo del carro (supuestos)

- Avance: la velocidad media es la calibrada (AUTO 19.3 cm/s) por un factor de 0.96 a 1.04 por corrida (pilas, piso). El giro sale de la diferencia de empuje de los lados: empuje = fuerza del lado x (PWM - zona muerta). El izquierdo es más fuerte; la zona muerta es de 60 a 120 de PWM (por eso 1 % de PWM mueve 1.4 a 2.3 % de empuje). Durante los primeros 250 ms los dos lados van a 255 sin balance (como el firmware); después a 207 y 210. La diferencia responde con un retraso de primer orden de 40 a 100 ms. Ejes a 120 mm.
- La posición se integra con el rumbo en centésimas de grado (antes el simulador truncaba a grados enteros: un rumbo de 0.9° no movía nada de lado).
- Giro por ángulo: el modelo de firmware que ya existía (anticipación que aprende) más un error de cierre normal de 1.1° (izquierda) y 1.7° (derecha), con tope de ±3°, que también se informa (como J15). Escala del giroscopio de -1 a +1 %. Sesgo residual de ±0.05 °/s.
- Pivote: el medido (izquierda 90/-90 mm, derecha 150/165 mm a 90°) por un factor de 0.85 a 1.15 por corrida y lado (dispersión NO MEDIDA).
- Carro colocado a mano con ±1° y ±10 mm (el giroscopio no ve ese error inicial).
- Frenado mecánico de 25 a 35 mm. Comandos con 0 a 2 ticks (0 a 100 ms) de retraso; SENS con 50 a 150 ms de atraso. Mientras el firmware hace un GIRAR por el WiFi no viaja ningún LEFT ni RIGHT (como en la app).
- RUMBO del firmware: la misma cuenta entera que `firmware_carro/rumbo.c` (la Parte A del banco lo comprueba en 20000 pasos al azar), cada 10 ms con el ángulo que mide el giroscopio.
- Cámara: `sr_vista` (zonas y cercanía por geometría) desde la pose de hace 200 a 300 ms, a 10 cuadros por segundo. Láser: rayo desde el frente contra las cajas, hasta 1.2 m. Choque: carro de 20 x 24 cm contra cajas de 20 x 20 cm por ejes separadores en cada tick.

## Calibración contra lo medido en el carro

La configuración «J14» reproduce el software que corrió en J14 (sin rumbo medido, sin los arreglos de J16, la «Recta 150 cm» haciendo 160 cm por el error de la app). Se eligió la diferencia de empuje que queda después del balance: de 0.3 a 2.1 %, siempre el izquierdo (B4-1 se desvió a la derecha). Resultado (`calibracion_j14.txt`, 1000 corridas):

- Rectas B4 (8 celdas, 160 cm): desvío simulado medio 13.7 cm, p95 23.2 cm, máximo 27.9 cm. Los cuatro desvíos medidos (5.5, 15, 18 y 20 cm) caen en los percentiles 9, 57, 73 y 82.
- Esquive B6-4 (~40 cm a la derecha, una sola corrida medida a ojo): el esquive simulado J14 da media 20 cm, p95 32.4 cm, máximo 40.8 cm; los 40 cm quedan en el percentil 99. El simulador reproduce la magnitud (decenas de cm), solo en la cola.
- Giros: dentro de ±3° por construcción (en J14, 28 de 28 giros medidos por el giroscopio).
- Ruta en L: no hay medición física (los «20 a 24 cm» de J14 eran una lectura equivocada de la pose estimada). El simulador J14 predice una distancia final media de 14.6 cm (p95 23.3 cm): es una predicción, no un ajuste.
- La duración de B4 («150 cm en 5 s») no cuadra con la calibración de 19.3 cm/s; no se usó.

## Límites conocidos del simulador

- No modela deslizamiento lateral sin giro (se puede probar con `j16_lateral_um_mm`; no hay datos), ni golpes, ni variación del piso dentro de una corrida, ni cambios de pilas durante la corrida.
- La dispersión del pivote, la escala del giroscopio y la ganancia de los motores son supuestos. La configuración «con RUMBO y escala calibrada» (J16, tercera ronda) cambia el error de escala de ±1 % por un residuo de ±0.3 % (fw_escala_ppm, sacado de la semilla para conservar el emparejamiento): también es un supuesto (±1° al alinear el carro al empezar y al terminar dos vueltas). El error de escala ahora se lleva en partes por millón; con el residuo en 0 da exactamente los mismos números que antes.
- La cámara es geométrica (no reproduce el detector ni imágenes) y el láser es un rayo ideal.
- Las ganancias de RUMBO se eligieron con este modelo: pueden no ser las mejores en el carro.

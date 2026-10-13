# Latencia: de la cámara al STOP, y cuánto avanza el carro

Meta del proyecto: **menos de 300 ms** desde que el obstáculo aparece en la cámara hasta que el
carro recibe el STOP. Aquí se suma toda la cadena con lo medido y se dice qué es suposición.

Velocidad de ejemplo: **20 cm/s** (con los valores de partida `MS_POR_CM` = 50: 1 cm cada 50 ms).
A esa velocidad, **cada 100 ms de retraso son 2 cm**.

## La cadena, etapa por etapa

1. **Cámara (Freenove, GC0308) hasta que el cuadro llega al teléfono: 320 ms.** Medido por el
   usuario con cronómetro en HQVGA 240x176 (~12 fps). Incluye la exposición y lectura del
   sensor (~100 ms), la compresión JPEG por software en la ESP32 (menos que los ~107 ms de QVGA; la edad en la placa fue de ~137 ms en HQVGA),
   el WiFi y la decodificación en el teléfono. Es una sola medida; no se sabe cómo se reparte.
2. **Convertir y detectar en el teléfono: ~7 ms.** Medido en el Huawei (conversión + detector
   en C: 6.5 a 6.9 ms por cuadro; el análisis solo, ~0.6 ms). El modelo EfficientDet no entra en
   esta cadena (45 ms) porque el detector de color manda en seguridad.
3. **Antirrebote del navegador** (`MS_CONFIRMAR_LEJOS` = 150 ms):
   - obstáculo cerca (cercanía 3 en cualquier zona, o 2 en el centro): **0 ms**, se detiene con el primer cuadro;
   - obstáculo de cercanía 2 a un lado: hay que verlo 150 ms y en 2 cuadros como mínimo (cercanía 1: no frena). Con cuadros
     cada 80 ms se confirma al tercero (160 ms después del primero); cada 120 ms, 240 ms
     después (probado en `pruebas_navegador.c`).
4. **Tick del navegador, envío por WebSocket y recepción: ~28 ms con USB.** Es lo que sobró
   de la medida del TRABAJO 11 (obstáculo simulado → STOP en el servidor: mediana 128 ms, de los
   que 100 ms eran el antirrebote de entonces). El tick es de 50 ms, así que en el peor caso
   espera 50 ms. **Por WiFi real no está medido**: la latencia PING/PONG por USB fue de 7.6 ms;
   por WiFi con el carro habrá que leerla en la pantalla de control. Suposición: 10 a 30 ms.
5. **Firmware: del STOP a los motores a cero**: lo hace en la misma vuelta del `loop`
   (`carro_detener`), unos milisegundos. No medido.
6. **Frenado mecánico (cm extra tras el STOP)**: depende de las ruedas, el peso y el piso. No
   está medido: lo mide la calibración (`docs/calibracion.md`, valor `frenado_cm`). En las
   cuentas de abajo se muestra aparte.

## Suma y distancia recorrida a 20 cm/s

- **Obstáculo cerca (el caso que importa para frenar)**: 320 + 7 + 0 + 28 = **355 ms**
  → el carro avanza **7.1 cm** antes de recibir el STOP. Con WiFi real (suposición +15 ms)
  serían unos 370 ms = 7.4 cm.
- **Obstáculo de cercanía 2 a un lado** (cuadros cada 80 ms): 355 + 160 = **515 ms** → 10.3 cm.
  Con cuadros cada 120 ms: 355 + 240 = 595 ms → 11.9 cm. Como es lejano, hay margen: lo
  importante es que el STOP salga antes de llegar a él.
- A todo eso se suman los **cm de frenado** (a medir).

**Conclusión honesta: con esta cámara no se llega a 300 ms.** Solo la cámara ya son 320 ms.
Lo que sí se puede hacer:

- bajar la velocidad: la distancia hasta el STOP es `velocidad × retraso` (≈ 0.36 s):
  10 cm/s → 3.6 cm; 15 cm/s → 5.4 cm; 20 cm/s → 7.2 cm; 30 cm/s → 10.8 cm;
- elegir `OBS_CERCA_MIL` y la inclinación de la cámara para que la **cercanía 3 empiece al
  menos a (distancia de la cadena + frenado + margen)** del carro, por ejemplo 7.2 + frenado +
  5 cm de margen a 20 cm/s. Hay que medir a cuántos cm del carro está la fila donde empieza la
  cercanía 3 (con una cinta métrica en el piso);
- atacar la cámara: la compresión por software y el reloj del sensor son lo que más pesa
  (`docs/camara_prueba.md`); QQVGA bajaría el retraso, pero es muy chico para detectar.

## Qué falta medir (PENDIENTE HARDWARE)

- Retraso real cámara → teléfono con el cronómetro después de cargar el firmware nuevo de la
  cámara (HQVGA por defecto): el 320 ms es de antes de esta parte.
- Latencia PING/PONG por WiFi con el carro (pantalla de control).
- `frenado_cm`, `ARRANQUE` y `MS_POR_CM` reales (Parte 2, calibración).
- A cuántos cm del carro queda cada cercanía.

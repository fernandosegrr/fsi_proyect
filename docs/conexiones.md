# Conexiones del carrito: ESP32 DevKit + driver TC1508A

Esta guía sale solo de tres fuentes: `firmware_carro/config.h` (y el firmware original `original_backup/codigo_Carrito.ino` y su
documentación PDF, que solo se leyeron), y la hoja de datos del **TC1508A** de Fine Made (FM), versión 1.0, documento S&CIC2159. Lo que
ninguna de las tres confirma está marcado como **sin confirmar**. Todavía no se conectó nada de esto: es una guía para hacerlo.

## 1. Pines de la DevKit que usa el firmware

El firmware usa cinco GPIO y nada más (no hay LED propio, ni botones, ni sensores):

- **GPIO 27**: motor izquierdo, entrada A. Va a la entrada **INA** del TC1508A.
- **GPIO 26**: motor izquierdo, entrada B. Va a **INB**.
- **GPIO 25**: motor derecho, entrada A. Va a **INC**.
- **GPIO 33**: motor derecho, entrada B. Va a **IND**.
- **GPIO 2**: pulso "mantener encendida la powerbank" (ver sección 3). No va al driver.
- **GND** de la DevKit: tierra común con el driver y con las pilas.

Nombres de las entradas del driver:

- En la hoja de datos del chip se llaman **INA, INB, INC, IND** y las salidas **OUTA, OUTB, OUTC, OUTD**.
- El comentario de `config.h` las llama IN1 a IN4, y la documentación original las llama INT1 a INT4. **Sin confirmar**: qué
  rotulado trae TU módulo; lo normal es que IN1 = INA, IN2 = INB, IN3 = INC, IN4 = IND, pero míralo en la placa antes de conectar.
- Canal izquierdo = INA/INB con salidas OUTA/OUTB. Canal derecho = INC/IND con salidas OUTC/OUTD (hoja de datos, página 2).

Cómo ubicar cada pin en la DevKit: por el **nombre impreso**, no contando patitas. Busca `27`, `26`, `25`, `33` y `2`, que según la
placa aparecen como `D27`, `IO27` o `GPIO27`. El número del GPIO no es el número de la patita (la patita 27 de la hilera no es el
GPIO 27). La documentación original dice que la placa es una ESP32 WROOM-32 de 38 pines; en qué hilera quedan, **sin confirmar**
(depende del fabricante de tu DevKit).

Pines de arranque (strapping) o delicados:

- **GPIO 2: sí es de arranque** y es el delicado de este proyecto. Debe estar libre o a nivel bajo al arrancar y al cargar un programa;
  si algo externo lo sube a 3.3 V, la placa puede no arrancar o no dejarse programar. En muchas DevKit además tiene el LED de la placa
  (esto último, sin confirmar en la tuya). El repositorio ya lo anota en `docs/pendientes_firmware.md`, punto 5.
- **GPIO 25, 26, 27 y 33 no son de arranque**. Esto sale de la lista de pines de arranque del ESP32 (GPIO 0, 2, 5, 12 y 15) de la hoja
  de datos del ESP32, que no está en el repositorio: verifícalo ahí si quieres certeza.
- Las entradas del TC1508A llevan una resistencia a tierra de 1.5 MΩ típica (hoja de datos, página 2): si un GPIO queda flotando al
  arrancar, el motor no se mueve solo.

## 2. Diagrama (texto)

```
   4 pilas AA (~6 V)                          ESP32 DevKit  <-- USB <-- powerbank 5 V
   +--------+                                +---------------------+
   |  (+)   |---------------+                |                     |
   |  (-)   |-----+         |                |                     |
   +--------+     |         |                |  GPIO27  ------------------> INA ----+
                  |         |                |  GPIO26  ------------------> INB     |  canal
                  |         |                |  GPIO25  ------------------> INC ----+  izquierdo = INA/INB
                  |         |                |  GPIO33  ------------------> IND        canal derecho = INC/IND
                  |         |                |                     |
                  |         |                |  GPIO2   --(pulso)--> (sin confirmar)
                  |         |                |  GND ----------+    |
                  |         |                +----------------|----+
                  |         |                                 |
                  |         v (solo aqui)                     |
                  |      +--------------------------+         |
                  |      | VDD / VCC del driver     |         |
                  |      |        TC1508A           |         |
                  +------| GND                      |---------+  (GND del driver con GND de la ESP32)
                         |                          |
                         | OUTA --+-- motor TT --+--+-- motor TT       canal izquierdo:
                         | OUTB --+               |                      2 motores TT en paralelo
                         | OUTC --+-- motor TT --+--+-- motor TT       canal derecho:
                         | OUTD --+                                     2 motores TT en paralelo
                         +--------------------------+

   Regla: el (+) de las pilas va SOLO al VDD/VCC del driver. Nunca a un pin de la ESP32.
   Regla: pilas (-), GND del driver y GND de la ESP32 van unidos entre si.
```

## 3. Pulso para mantener encendida la powerbank (GPIO 2)

Lo que se sabe, y de dónde:

- El código (`config.h`, `carro.c`): GPIO 2 es una salida que sube a nivel alto 50 ms cada 2000 ms y el resto del tiempo está en bajo.
- La documentación original (PDF) dice que ese pin está "conectado a una resistencia o LED de carga" para simular consumo y que la
  powerbank no corte la salida.
- **Sin confirmar**: qué hay realmente conectado, de qué valor es la resistencia y si la powerbank realmente lo necesita. El repositorio
  no lo dice (`config.h` y `docs/pendientes_firmware.md` también lo marcan como pendiente).

Qué hay que conectar, entonces:

- **Para empezar, nada.** El código solo pone el pin en alto y bajo; sin carga externa no pasa nada malo. Si tu DevKit tiene el LED de la
  placa en GPIO 2, lo verás parpadear corto cada 2 s.
- Si más adelante la powerbank se apaga con el carrito quieto, la prueba razonable es una **carga resistiva entre GPIO 2 y GND** (o un
  LED con su resistencia en serie hacia GND), como dice la documentación original. El valor no está en el repositorio. Como referencia,
  y como cálculo mío, no como dato del proyecto: 3.3 V sobre 100 Ω son unos 33 mA. La corriente máxima que aguanta un GPIO del ESP32 está
  en su hoja de datos, que no está en el repositorio: consúltala antes de bajar de ahí. Hay que verificar con la powerbank real, porque la
  ESP32 con WiFi ya consume por sí sola.
- **Nunca** conectes GPIO 2 a 3.3 V ni a las pilas.

## 4. Alimentación y tierra

- **Pilas**: el (+) de las 4 AA va solo al **VDD (pines 4 y 8 del chip) = VCC de motores** del driver; el (-) al GND del driver. El
  TC1508A **no tiene alimentación lógica aparte**: VDD alimenta a la vez la lógica y los motores (hoja de datos, página 1). Rango
  recomendado 2.4 a 7.2 V, y 7.2 V es también el **máximo absoluto**. Cuatro pilas alcalinas nuevas dan alrededor de 6 V
  (la documentación original dice "~6 V"; en vacío pueden pasar un poco de 6 V, dato de las pilas, no del proyecto): cabe, pero con poco
  margen. No pongas una quinta pila ni otra fuente de más voltaje.
- **ESP32**: por USB desde la powerbank. No alimentes la ESP32 desde las pilas ni el driver desde la ESP32.
- **Tierra**: pilas (-), GND del driver y GND de la ESP32 van unidos. Sin esa tierra común las señales no tienen referencia
  (la documentación original también lo exige).
- **Las entradas del driver nunca deben superar su VDD** (hoja de datos, página 1: tensión de entrada máxima = VCC). Por eso, no mandes
  comandos de movimiento mientras las pilas estén desconectadas: una señal de 3.3 V sobre un driver sin alimentar rompe ese máximo.
  El firmware arranca con todo en cero, así que encender la ESP32 sin pilas es seguro; moverlo sin pilas no.
- Umbrales de entrada (página 2): alto desde 2.0 V y bajo hasta 0.8 V, medidos con VCC = 3 V. Una salida de 3.3 V de la ESP32 cumple.
  A ~6 V de VDD la hoja no da el umbral: **sin confirmar** (el cableado original ya funcionó así según `config.h`).
- Condensadores (circuito de referencia, páginas 4 y 5 de la hoja): 220 µF entre VDD y GND cerca del chip y 0.1 µF en paralelo con cada
  motor. **Sin confirmar** si tu módulo ya los trae; si no, el de 0.1 µF en cada motor (soldado en sus terminales) reduce ruido.

## 5. Cuántos motores por canal y el límite de corriente

- Son **2 motores TT por salida** del driver (4 motores y 2 canales), conectados **en paralelo y con la misma polaridad**: los dos motores
  izquierdos a OUTA/OUTB y los dos derechos a OUTC/OUTD.
- Límites de la hoja de datos del TC1508A (página 1): corriente continua **1.8 A por canal**, pico **2.5 A por canal**. La página 6 añade que
  el arranque no debe pasar de 2.5 A y que el valor continuo depende del encapsulado, la tensión, la temperatura y la placa: la propia hoja
  pide dejar margen.
- Resistencia de conducción: 1 Ω típica y 1.6 Ω máxima, medida a 200 mA (página 2). Potencia máxima del chip: 1.73 W, con 75 °C/W de la unión
  al aire (página 1).
- Conclusión mía, con esos números: si el 1 a 1.6 Ω es el de todo el puente (**sin confirmar** en la hoja), un canal a 1.8 A disiparía de
  3.2 a 5.2 W, más que los 1.73 W del chip entero. Con 1.73 W el chip llegaría a unos 1.0 A (peor caso) o 1.3 A (típico) continuos en un
  solo canal. Y los dos canales comparten ese límite.
- La documentación original dice que el arranque es de unos 500 mA por motor, **sin confirmar** (no hay medición en el repositorio). Con dos
  motores por canal son unos 1 A de arranque por canal: dentro de los 1.8 A de la hoja, pero cerca de lo que el chip aguanta sin calentarse.
  **Con las ruedas bloqueadas o el carrito empujando contra algo, la corriente puede ser mucho mayor que la de arranque**: la corriente de
  motor bloqueado de tu TT es el dato que falta. Si es mayor que 0.9 A por motor, dos motores bloqueados en un canal pasan de 1.8 A.
- Qué hacer: medir con un multímetro en serie (escala de 10 A) la corriente de un solo motor, primero girando libre y luego frenado con la
  mano apenas un par de segundos; sumar dos motores por canal; y vigilar la temperatura del chip. Si se queda corto, las salidas
  hay que repartir la carga o usar un driver con más corriente. No inventes soluciones con las salidas en paralelo: la hoja de datos no lo
  menciona (sin confirmar).

## 6. Orden seguro de conexión

Todo con **pilas fuera del portapilas**, la powerbank desconectada, la ESP32 sin USB y el carrito con las **ruedas en el aire** (sobre una caja).

1. Con un multímetro, revisa cada tramo de cable antes de conectar. Cables cortos y bien pelados, sin hilos sueltos.
2. **Tierras primero.** Une el (-) del portapilas, el GND del driver y un GND de la DevKit (los tres juntos).
3. **Señales después.** Conecta GPIO 27 → INA, GPIO 26 → INB, GPIO 25 → INC, GPIO 33 → IND. Verifica dos veces cada cable con la lista de la sección 1.
4. Conecta **UN solo motor** (por ejemplo, el delantero izquierdo) a OUTA/OUTB. Los otros tres, desconectados.
5. Conecta el (+) del portapilas al VDD/VCC del driver **sin pilas todavía**.
6. **Revisa cortos con el multímetro en continuidad** (con pilas aún fuera):
   - entre (+) del portapilas y (-): no debe haber continuidad (al principio puede marcar algo mientras se cargan los condensadores; debe subir, no quedar en cero);
   - entre cada salida (OUTA...OUTD) y GND, y entre salidas contiguas: sin continuidad;
   - entre 3.3 V de la DevKit y GND: sin continuidad.
7. Conecta la ESP32 al USB de la powerbank (aún sin pilas). Debe encender el LED de la placa; el firmware arranca solo con el motor sin energía.
8. Abre la red del carrito en el teléfono y la página de control. Todavía no pulses nada.
9. **Mete las pilas** (un solo lado). Con un motor y ruedas en el aire, pulsa ADELANTE un segundo y suelta. Qué esperar: la rueda gira
   hacia adelante mientras pulsas y se detiene al soltar. Si gira al revés, **cambia entre sí los dos cables de ese motor en la salida del
   driver** (no el código). Si no gira y suena, revisa pilas y conexiones.
10. Prueba REVERSE y los giros (LEFT/RIGHT) con ese motor. Luego añade el segundo motor del mismo canal (también ruedas en el aire), y
    repite con el canal derecho. Al final, los cuatro, siempre con las ruedas en el aire la primera vez.
11. Solo cuando todo gire bien y nada se caliente, baja el carrito al suelo y prueba cortos tramos.

Detalle sobre el sentido: `config.h` dice que el motor izquierdo avanza con GPIO 26 (INB) y el derecho con GPIO 25 (INC). Así quedó el
cableado original probado. Si recableas con otra polaridad de motores, ajusta cables, no código.

Si algo se calienta o huele a quemado:

- **Saca las pilas primero**, y luego el USB. No toques el chip del driver hasta que se enfríe: puede quemar.
- Espera a que se enfríe. Busca la causa antes de volver a probar: cable en corto, polaridad de pilas invertida, motor bloqueado,
  o dos motores en un canal que piden más de 1.8 A (sección 5).
- Regla práctica mía: si no puedes tener el dedo 3 segundos sobre el chip, apaga y revisa.
- Si el chip quedó dañado, no se pueden dar por buenas las salidas: cámbialo antes de seguir.

## 7. Lo que sigue sin confirmar

- Nombre impreso de las entradas del driver en tu módulo (IN1...IN4, INT1...INT4 o INA...IND).
- Qué hay realmente conectado a GPIO 2 (resistencia, LED o nada) y si la powerbank lo necesita.
- Corriente de arranque y de motor bloqueado de tus motores TT (la hoja del motor no está en el repositorio).
- Si la resistencia de conducción del chip (1 a 1.6 Ω) es de un lado o del puente completo.
- Si el módulo trae sus propios condensadores de filtro.
- En qué hilera de la DevKit queda cada GPIO (depende del fabricante).
- La corriente máxima de un GPIO del ESP32 (para dimensionar la carga de GPIO 2): está en la hoja de datos del ESP32, fuera del repositorio.

## 8. Sensores I2C: giroscopio MPU6050 y láser VL53L0X (J11)

Los dos módulos van en el MISMO bus I2C de la DevKit, alimentados con 3V3 (cada módulo trae sus propias resistencias de pull-up):

| Señal | GPIO de la DevKit | GY-521 (MPU6050) | VL53LXX-V2 (VL53L0X) |
|---|---|---|---|
| SDA | **21** | SDA | SDA |
| SCL | **22** | SCL | SCL |
| 3V3 | 3V3 | VCC | VIN |
| GND | GND | GND | GND |

- Direcciones: **0x68** el MPU6050 (AD0 libre o a GND; a 3V3 sería 0x69) y **0x29** el láser. Los GPIO 21 y 22 estaban libres; los demás pines no cambian.
- **Empalmes soldados** (o una regleta): un nudo atado a mano da mal contacto y el láser aparece y desaparece. Hay que revisar de verdad que SDA y SCL del GY-521 lleguen a la DevKit (el 18 oct un SCL sin contacto dejó al giroscopio invisible aunque el LED del módulo encendía; el LED solo prueba que le llega energía).
- Montaje: giroscopio plano sobre la caja de pilas, flecha X al frente, eje Z hacia arriba; láser al frente, a 6-8 cm del piso, inclinado 5-8° hacia arriba. Detalle, parámetros y errores típicos en `docs/sensores.md`.
- Diagnóstico del bus por USB: `I2C?` (qué direcciones contestan), `PINES?` (nivel eléctrico de SDA/SCL) y `PRUEBAI2C?` (escaneo cruzado y a 100/400 kHz); ver `scripts/verificar_sensores.py`.

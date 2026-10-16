# Prueba de la cámara Freenove ESP32-WROVER (v3.0)

Programa: `camara_prueba/camara_prueba.ino`. Se carga con `.\scripts\cargar.ps1 camara` (ver `docs/hardware_cargar.md`).
La cámara se conecta como cliente a la red del carrito (la que emite la DevKit), con IP fija **192.168.4.2**, y sirve video por HTTP.

## Resumen del trabajo H3 (13/10/2026)

- **Antes**: QVGA con JPEG por software a **6,7 fps** (captura sola 9,9 fps), todo en una sola tarea: capturar, comprimir y enviar uno detrás de otro.
- **Ahora**: tubería en dos núcleos con casilla única (nunca se acumulan cuadros viejos). QVGA, reloj 20 MHz, RGB565, JPEG calidad 50:
  **8,6 a 9,1 fps** de salida (captura 9,3 a 9,5 fps), compresión 105 a 107 ms por cuadro, y **edad del cuadro de unos 250 ms** hasta el fin
  de la compresión (en `/stats` con un cliente local: media 250 ms, peor 308 ms; el envío por el lazo local de la propia placa tarda 1 ms; el envío por WiFi lo mides tú).
- **Lo que limita ahora**: (1) el sensor GC0308 da unos 10 fps a 20 MHz (los fps de captura suben y bajan con el reloj: 5, 8 y 10 fps a 10, 16 y 20 MHz en
  QVGA) y (2) la compresión por software tarda ~107 ms por cuadro de QVGA, casi lo mismo que el periodo del sensor (~106 ms). Con 20 MHz, QVGA no pasa de ~9 a 10 fps.
- **No pudo hacerse mejor sin cambiar de resolución**: HQVGA 240x176 da 12,4 fps y 137 ms de edad (+41 % de fps, 46 % menos edad) pero tiene el 55 % de los píxeles y el
  detector de obstáculos (`nucleo_c/obstaculos.c`) espera 320x240. Se queda QVGA como valor por defecto; HQVGA se puede elegir con `/control?size=hqvga`.

## Cómo está hecho (H3)

- **Tubería**: `tarea_captura` (núcleo 0) pide cuadros a la cámara y los deja en UNA casilla; si llega uno nuevo y el anterior seguía ahí, el viejo se descarta
  (y se cuenta). `tarea_stream` (núcleo 1) toma el más reciente, lo comprime (`frame2jpg`) y lo envía. Si no hay nadie pidiendo cuadros, la captura se detiene
  y la casilla se vacía (un cuadro guardado envejece: antes el primer cuadro tras un rato salía con 7 a 9 segundos de edad; ya corregido).
- **Envío**: sin retardo de Nagle (`TCP_NODELAY`), búfer de envío de 2920 bytes pedido al sistema (puede que lwIP lo ignore: sin confirmar), tiempo máximo de envío 1,5 s
  (un cliente lento se corta en vez de acumular cuadros), y un solo envío por cuadro (cabecera + JPEG juntos, sin codificación "chunked"). Un cliente lento nunca hace
  que se acumulen cuadros: mientras se envía uno, la captura sigue y reemplaza el de la casilla.
- **Edad del cuadro** = ms desde que empezó la captura del cuadro (marca de tiempo del primer bloque DMA) hasta que termina su envío. Incluye la lectura del sensor (~100 ms).
- **Registros del sensor**: no se tocó ninguno (límite de exposición, fps automático...). No hay hoja de datos del GC0308 en el proyecto y escribir registros a ciegas no es seguro.
  Los fps no mostraron caídas por la luz durante la prueba.

## Lo medido: barrido sin cliente (100 cuadros por combinación, calidad 0 a 100)

Luz media al empezar y al terminar el barrido: Y = 127 y Y = 124 (de 0 a 255), es decir luz intermedia y estable; los fps no cambiaron entre bloques de 10 cuadros
(salvo la combinación de 20 MHz en QVGA: de 8,4 a 9,4 por la compresión). Las ms de compresión son por cuadro; "edad" es hasta el fin de la compresión; JPEG medio con
calidad 50 / calidad 70. Los tamaños de JPEG dependen de la escena.

- 10 MHz, RGB565, QVGA: 5,0 fps; JPEG 6238 / 8156 B; compresión 101 ms; edad 301 ms
- 10 MHz, RGB565, HQVGA: 8,0 fps; 3955 / 5128 B; 55 ms; edad 180 ms
- 10 MHz, RGB565, QQVGA: 5,0 fps; 2836 / 3589 B; 26 ms; edad 226 ms
- 10 MHz, YUV422, QVGA: 5,0 fps; 6660 / 8677 B; 119 ms; edad 320 ms
- 10 MHz, YUV422, HQVGA: 8,0 fps; 4233 / 5445 B; 64 ms; edad 189 ms
- 10 MHz, YUV422, QQVGA: 5,0 fps; 3019 / 3832 B; 30 ms; edad 230 ms
- 16 MHz, RGB565, QVGA: 8,0 fps; 5989 / 7790 B; 106 ms; edad 231 ms
- 16 MHz, RGB565, HQVGA: 12,8 fps; 3556 / 4611 B; 56 ms; edad 135 ms
- 16 MHz, RGB565, QQVGA: 8,0 fps; 2735 / 3417 B; 26 ms; edad 151 ms
- 16 MHz, YUV422, QVGA: 8,0 fps; 6562 / 8538 B; 124 ms; edad 250 a 270 ms
- 16 MHz, YUV422, HQVGA: 12,8 fps; 3824 / 4947 B; 66 ms; edad 144 ms
- 16 MHz, YUV422, QQVGA: 8,0 fps; 2989 / 3784 B; 30 ms; edad 156 ms
- 20 MHz, RGB565, QVGA: **8,8 fps (calidad 50) y 8,5 (calidad 70)**, captura 9,4 y 9,2; 6254 / 8138 B; 107 y 109 ms; 7 y 9 cuadros descartados; edad 252 y 250 ms
- 20 MHz, RGB565, HQVGA: 12,4 fps; 3986 / 5116 B; 56 ms; edad 137 ms
- 20 MHz, RGB565, QQVGA: 10,0 fps; 2844 / 3575 B; 26 ms; edad 126 ms
- 20 MHz, YUV422, QVGA: 7,1 fps (captura 8,6; 20 descartados); 6650 / 8629 B; 125 ms; edad 260 ms
- 20 MHz, YUV422, HQVGA: 12,4 fps; 4195 / 5349 B; 66 ms; edad 147 ms
- 20 MHz, YUV422, QQVGA: 10,0 fps; 3026 / 3809 B; 30 ms; edad 130 ms

Conclusiones del barrido:

- Los fps de captura salen del reloj y del tamaño: 10 MHz es la mitad de rápido que 20 MHz. A 16 MHz, HQVGA da el mismo fps que a 20 MHz (12,4 a 12,8).
- **RGB565 comprime más rápido que YUV422** (107 contra 125 ms en QVGA) y da JPEG algo más pequeños: se queda RGB565.
- La calidad 50 contra 70 casi no cambia el tiempo (107 y 109 ms), pero baja el JPEG ~23 % (por ejemplo 6254 contra 8138 B): se queda 50. Para el detector de color de piso
  da igual, porque promedia celdas de 10x10 píxeles (y el JPEG solo cambia los bordes finos).
- QVGA no mejora con 16 MHz (8,0 fps y edad parecida); 20 MHz es lo máximo que se probó.
- **Elegido (por defecto)**: 20 MHz, RGB565, QVGA 320x240, calidad 50.
- Con la configuración elegida, en el arranque: 8,6 a 9,1 fps, 105 a 107 ms de compresión, edad 252 a 255 ms, 5 a 8 cuadros descartados de 100 (la compresión es apenas
  más lenta que el sensor).

## Autoprueba local (la placa se pide a sí misma las rutas, por 127.0.0.1)

Sirve para comprobar que el servidor responde sin conectar nada a la red de la placa; no mide el WiFi, y el cliente local comparte el procesador con la placa (resultados pesimistas).

- `/stats`: HTTP 200 (498 bytes); `/capture`: HTTP 200 (un JPEG); `/control?size=qvga`: HTTP 200; `/stream`: HTTP 200, 49 cuadros en 6,0 s (8,2 fps).
- Cada petición queda anotada por serie, por ejemplo `[http] GET /stats desde 127.0.0.1`.
- `/stats` justo después del stream: captura 7,9 fps, envío 6,3 fps, compresión 109 ms, envío 1,0 ms (por el lazo local), edad media 250 ms y peor 308 ms.
- **No probado**: el WiFi real (envío por radio, retraso en el teléfono).

## Para ver video y /stats en el teléfono (pasos cortos)

1. DevKit encendida (red del carro) y la cámara conectada a la corriente. **Espera 40 segundos** después de encender la cámara (al arrancar mide la cámara ~12 s y se conecta).
2. En el teléfono conéctate a **CarroFSI** (si Android avisa "sin Internet", mantén la conexión).
3. Abre **http://192.168.4.2/**. Arriba del video, en letra pequeña, verás: fps de captura y de envío, edad del cuadro (ms), ms de compresión y de envío, cuadros descartados,
   RSSI y la configuración. Se actualiza cada 2 s.
4. Abre también **http://192.168.4.2/stats** (JSON con los mismos números y la memoria libre). Botones QVGA / HQVGA / QQVGA / VGA en la página.
5. Si no carga: el teléfono pudo recibir la misma IP .2 de la DevKit; ponle una IP manual (192.168.4.50, máscara 255.255.255.0, puerta 192.168.4.1).

Qué anotar: nitidez (¿se distingue el piso y un objeto a 20 y 50 cm?), retraso que sientes, fluidez, si se corta (cuándo, cuánto tarda en volver), y de la página: `fps de envío`,
`edad`, `descartados` y `RSSI`, con QVGA y luego con HQVGA. Si el teléfono muestra menos fps que los de captura, la causa es el WiFi o el navegador.

## Cómo repetir la prueba del cronómetro (retraso total hasta la pantalla)

1. Pon un cronómetro en marcha en otra pantalla (computadora u otro teléfono, con milisegundos o centésimas) y apunta la cámara a él.
2. En el teléfono abre `http://192.168.4.2/` y acerca el teléfono para ver a la vez el cronómetro real y el video del cronómetro.
3. Toma una captura de pantalla del teléfono o una foto del conjunto con otro dispositivo: debe verse el cronómetro real y el del video en la misma imagen.
4. Retraso = lectura del cronómetro real menos la lectura del cronómetro en el video. Repite 5 veces y anota promedio, mínimo y máximo.
5. Anota junto con cada medida la `edad` y los `fps de envío` que muestra la página, y la configuración (QVGA/HQVGA). La diferencia entre tu retraso y la `edad` es lo que añaden el WiFi,
   el navegador y la pantalla. (Antes: unos 530 ms en una sola muestra.)

## Primera parte (H2): mapa de pines y sensor

- **Placa**: identificada por la PSRAM (Freenove, 4 MB; MAC f0:16:1d:8b:cf:e8), chip USB-serie CH340, microSD vacía.
- **Mapa de pines**: `CAMERA_MODEL_WROVER_KIT`, el mismo que usa el código oficial de Freenove (repositorio `Freenove/Freenove_ESP32_WROVER_Board`,
  `Sketch_06.1_CameraWebServer/camera_pins.h`, idéntico al del core de ESP32): XCLK 21, SIOD 26, SIOC 27, datos Y2..Y9 = 4, 5, 18, 19, 36, 39, 34, 35, VSYNC 25, HREF 23, PCLK 22, sin PWDN ni RESET.
  `AI_THINKER` falló con `0x106 ESP_ERR_NOT_SUPPORTED`.
- **Sensor GC0308** (PID 0x9B, 0,3 megapíxeles): no hace JPEG (con JPEG da `0x106` "JPEG format is not supported on this sensor"), por eso la cámara entrega RGB565/YUV422 y la ESP32 comprime.
- **WiFi**: red `CarroFSI`, IP fija 192.168.4.2, RSSI de -19 a -44 dBm con las placas juntas. Si la red no está o se cae, reintenta con espera creciente (1, 2, 4... 10 s) y a los 60 s sin conectar se reinicia (ver «Robustez del WiFi y de la cámara» más abajo).
- **Memoria** con el servidor listo: heap libre 130 a 145 KB; PSRAM libre 3,7 MB.
- Los errores `gpio_install_isr_service ... already installed` al reiniciar la cámara son inofensivos.

## Por qué esto importa para el carrito

Con QVGA y este sensor se esperan unos 9 fps y ~250 ms de edad antes de contar el WiFi; HQVGA da 12 fps y ~137 ms. El presupuesto de 300 ms del proyecto queda justo en QVGA.
Con una cámara con JPEG por hardware (por ejemplo OV2640) la compresión dejaría de ser el límite; si la ESP32-CAM definitiva trae uno, hay que repetir el barrido
(`#define MODO_BARRIDO 1` en el programa y `.\scripts\cargar.ps1 camara -MonitorSegundos 540`).

## Robustez del WiFi y de la cámara (16 oct)

**Qué pasó:** la cámara (IP fija 192.168.4.2) dejó de responder en la red `CarroFSI` y solo volvió al desconectarla y conectarla. No se sabe si fue un cuelgue de la placa o un choque de IP
(el DHCP del DevKit repartía desde la .2, la misma de la cámara, y un teléfono que se conecta antes se la queda). Se cubrieron las dos causas.

### 1. DevKit: el DHCP ya no reparte la .2
- `firmware_carro.ino` llama a `WiFi.softAPConfig(192.168.4.1, 192.168.4.1, 255.255.255.0, 192.168.4.10)` antes de crear la red. En Arduino-ESP32 3.3.12 el cuarto parámetro es dónde EMPIEZA el DHCP y el core
  reparte esa dirección y las 10 siguientes (en `NetworkInterface.cpp`: `lease.end_ip = lease.start_ip + 10`): **.10 a .20**. La .2 (cámara) y la .1 (DevKit) quedan fuera; el core además rechaza (devuelve `false`) un rango que pise la IP del carro o la puerta de enlace.
- Los números están en `config.h` (`DHCP_INICIO_OCTETO`, `DHCP_FIN_OCTETO`, `IP_CAMARA_OCTETO`...) y `pruebas_firmware/pruebas_carro.c` comprueba en PC que el rango es .10 a .20 y no contiene la .2 ni la .1 (7 casos).
- **Cómo verificarlo con la placa** (monitor serie del DevKit a 115200, `.\scripts\cargar.ps1 nuevo` ya lo abre 15 s):
  1. Al arrancar debe salir `DHCP: configurado (reparte .10 a .20; la .2 queda para la camara)`. Si dice `ERROR al configurar`, el core no aceptó el rango.
  2. Cada vez que un teléfono se conecta sale `DHCP: entrego 192.168.4.1x`. Debe ser un número de .10 a .20; si alguna vez sale la .2 aparece `<-- ERROR: es la de la camara`.
  3. En el teléfono (Ajustes → WiFi → CarroFSI → dirección IP) debe verse 192.168.4.10 a .20. Se puede conectar el teléfono ANTES que la cámara: la cámara debe seguir entrando con la .2.

### 2. Cámara: recuperación y vigilante
Las decisiones están en `camara_prueba/recuperacion.c/.h` (C puro, probado en PC con `pruebas_firmware/pruebas_camara.c`, 34 casos, incluido el cruce de la vuelta de `millis()` a los 49 días); `camara_prueba.ino` solo actúa.

| Situación | Qué hace |
|---|---|
| Se pierde la asociación WiFi | Vuelve a llamar a `WiFi.begin` (con la misma IP fija y el canal de la red) esperando 1, 2, 4, 8 y luego 10 s entre intentos. En el arranque el primer intento tiene 5 s antes del primer reintento. |
| 60 s sin conectar (en el arranque o después de una caída) | `ESP.restart()` con motivo `sin_wifi_60s`. |
| El servidor HTTP no atiende | La placa se pide `/salud` a sí misma (127.0.0.1) cada 10 s; 3 fallos seguidos (30 s) → reinicio con motivo `servidor_no_atiende`. `/salud` responde `ok` sin escribir en el serial. |
| Alguien pide cuadros y la captura lleva más de 5 s sin entregar uno | Reinicio con motivo `cuadro_viejo_5s`. Sin demanda de video no cuenta (la captura no pide cuadros). |
| El bucle principal (`loop`) no avanza más de 8 s | Reinicio con motivo `bucle_sin_senal`. |

El vigilante es una tarea (núcleo 0, prioridad 5) que empieza cuando `setup()` termina. Se mantiene IP fija 192.168.4.2, canal de la red (1, lo fija el DevKit), `setSleep(false)` y todo el resto del comportamiento.
El motivo de un reinicio propio se guarda en memoria RTC (`RTC_NOINIT_ATTR`) y sobrevive al `ESP.restart()`.

### `/stats` (campos nuevos; los de antes no cambian)
- `uptime_s`: segundos encendida (ya existía).
- `reconexiones`: veces que el WiFi se desconectó (ya existía); `reintentos_wifi`: veces que se volvió a llamar a `WiFi.begin`.
- `ultima_desconexion`: `{"razon": 200, "texto": "BEACON_TIMEOUT", "hace_s": 35}` (`razon` 0, `texto` "ninguna" y `hace_s` -1 si no se ha desconectado desde que arrancó). Los códigos son los de ESP-IDF: 201 = no encuentra la red, 200 = se perdió la señal del DevKit, 15 = fallo del apretón de manos...
- `ultimo_reinicio`: `{"causa": "brownout", "motivo": "", "propios_seguidos": 0}`. `causa` puede ser `encendido`, `pin_externo`, `software`, `panic`, `watchdog_tareas`, `watchdog_interrupciones`, `watchdog_otro`, `brownout` o, si el reinicio lo pidió el vigilante, `reinicio_propio` con el `motivo` de la tabla de arriba. `propios_seguidos` cuenta reinicios propios seguidos (vuelve a 0 con cualquier otra causa, como un apagón).
- También sale por el serial al arrancar: `[sistema] ultimo reinicio: ...`, y cada reinicio propio escribe `[recuperacion] REINICIO por: <motivo>`.
- Cómo leerlo: `http://192.168.4.2/stats` desde el teléfono en la red CarroFSI (o `Invoke-WebRequest` desde la PC conectada a CarroFSI). Si la cámara se vuelve a quedar sin respuesta, lo primero es mirar `ultimo_reinicio.causa` y `ultima_desconexion` en cuanto vuelva.

### Qué NO cubre (para la siguiente vez)
- Si la cámara no consigue iniciar el sensor al arrancar (`DETENIDO: la camara no inicio con ningun mapa de pines`) sigue sin reiniciarse sola (suele ser el cable de la cámara, no un fallo pasajero).
- Si la radio se queda colgada pero `WiFi.status()` sigue en «conectado» y el servidor local contesta, la placa no lo nota (no se mide el alcance hacia el DevKit). Si pasa, `/stats` no estará disponible y bastará compararlo con el último `reconexiones`.

### App
Si la cámara no responde, las pantallas (Inicio, Manual, Visión, Ejecutar ruta, Prueba final) dicen **«La cámara no responde»** con la IP intentada («Se intentó con 192.168.4.2 y se reintenta sola cada 3 s…»); la sesión de video reabre sola cada **3 s** (`SES_REINTENTO_MS`, antes 5 s; el control del carro usa el mismo plazo). El chip de Inicio dice «No responde» y el hueco del video de Manual «La cámara no responde».

### Cargar (no se cargó nada en este trabajo)
- DevKit (**COM6**, solo esa placa por USB): `.\scripts\cargar.ps1 nuevo -Puerto COM6`
- Cámara Freenove (**COM8**, solo esa placa por USB): `.\scripts\cargar.ps1 camara -Puerto COM8`
- Solo compilar: `.\scripts\cargar.ps1 nuevo -SoloCompilar` y `.\scripts\cargar.ps1 camara -SoloCompilar` (compilan: DevKit 74 %, cámara 83 % de la flash).
- Después de cargar ambas, reiniciar la cámara y el DevKit en cualquier orden: la cámara debe entrar sola con la .2 y el teléfono debe recibir una dirección de .10 a .20.

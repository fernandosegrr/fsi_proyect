# Prueba de la cámara Freenove ESP32-WROVER (v3.0)

Programa: `camara_prueba/camara_prueba.ino`. Se carga con `.\scripts\cargar.ps1 camara` (ver `docs/hardware_cargar.md`).
La cámara se conecta como cliente a la red del carrito (la que emite la DevKit) y sirve video por HTTP.

## Lo medido (13/10/2026, placa en COM8, chip USB-serie CH340)

- **Placa**: la script la identificó por la PSRAM (con PSRAM = Freenove, 4 MB; MAC f0:16:1d:8b:cf:e8). Se probó con la ranura microSD vacía.
- **Mapa de pines que funcionó**: `CAMERA_MODEL_WROVER_KIT`. Es el que usa el código oficial de Freenove para esta placa
  (repositorio `Freenove/Freenove_ESP32_WROVER_Board`, `Sketch_06.1_CameraWebServer/camera_pins.h`, idéntico al del core de ESP32):
  XCLK 21, SIOD 26, SIOC 27, datos Y2..Y9 = 4, 5, 18, 19, 36, 39, 34, 35, VSYNC 25, HREF 23, PCLK 22, sin PWDN ni RESET.
- **AI_THINKER** (XCLK 0, PWDN 32...): falló con `0x106 ESP_ERR_NOT_SUPPORTED` ("Detected camera not supported"); era de esperar, no es esta placa.
- **El sensor es un GC0308** (PID 0x9B, 0.3 megapíxeles). **No sabe hacer JPEG**: con JPEG el arranque da `0x106 (ESP_ERR_NOT_SUPPORTED)`
  ("JPEG format is not supported on this sensor"). Por eso el programa lo inicia en RGB565 y **comprime cada cuadro a JPEG por software**
  en la ESP32 (calidad 80 de 100; la "calidad 12" del sensor no existe aquí y esa equivalencia es aproximada, sin medir).
  Con RGB565 hay que reiniciar la cámara para pasar de QVGA a VGA.
- **Medida sin ningún cliente conectado**, 100 cuadros por tamaño (descartando los 5 primeros), 2 búferes en PSRAM, modo "latest":
  - QVGA 320x240: captura sola 9.9 fps; captura + JPEG **6.7 fps**; JPEG medio **9570 bytes**.
  - VGA 640x480: captura sola 10.0 fps; captura + JPEG **2.1 fps**; JPEG medio **24193 bytes**.
  - La captura sola se queda en unos 10 fps en los dos tamaños (probablemente la limita el sensor con el reloj de 20 MHz que se usa; sin comprobar); lo que baja los fps con JPEG
    es la compresión por software. El tamaño del JPEG depende de lo que vea la cámara: son números de una escena cualquiera.
  - Cuentas mías: 6.7 fps son unos 150 ms por cuadro; 9570 B x 6.7 fps son unos 64 KB/s (unos 0.5 Mbit/s) por WiFi.
- **WiFi**: conectó a la red `Wall-E` (la DevKit original) en el primer intento, canal 1, **IP fija 192.168.4.2**, **RSSI de -33 a -44 dBm**
  (las placas estaban cerca). Mientras la DevKit estuvo desconectada, la cámara no encontró la red y reintentó 10 veces (cada 15 s, como se pidió) sin colgarse; al volver la DevKit conectó en el primer intento tras reiniciar.
- **Memoria** con el servidor listo: heap libre 154 KB, PSRAM libre 3.88 MB de 4 MB.
- **Errores en el serie**: solo `JPEG format is not supported on this sensor` (esperado, activa el JPEG por software) y avisos
  `gpio_install_isr_service ... already installed` al reiniciar la cámara (sin consecuencias).
- **NO probado**: el servidor HTTP con un cliente real (`/`, `/capture`, `/stream`, `/control`): para no cortar el Internet de la PC, esta no se
  conectó a la red de la placa. Los fps servidos, el retraso y la fluidez los mides tú con el teléfono. Tampoco se miraron ni guardaron imágenes.

## Para ver el video en el teléfono (pasos cortos)

1. Deja la DevKit encendida (red Wall-E) y la placa de la cámara conectada a la corriente (USB). **Espera 90 segundos** después de encenderla:
   al arrancar mide la cámara (unos 65 s) y luego se conecta.
2. En el teléfono conéctate a la red **Wall-E**. Si Android avisa "sin Internet", elige **mantener la conexión**.
3. Abre en el navegador **http://192.168.4.2/**.
4. Prueba los botones **QVGA** y **VGA** de la página. (Otras direcciones: `/capture` da un solo cuadro, `/stream` el video, `/control?size=vga`.)
5. Si el video no carga: puede que el teléfono haya recibido la misma IP 192.168.4.2 de la DevKit. Fija en el teléfono una IP manual
   (por ejemplo 192.168.4.50, máscara 255.255.255.0, puerta 192.168.4.1) o apaga y enciende el WiFi del teléfono con la cámara ya conectada.

Qué anotar: nitidez (¿se distingue el piso y un objeto a 20 y 50 cm?), retraso que sientes al mover la mano, fluidez (¿saltos?), y si se
corta (cuándo y cuánto tarda en volver). Con QVGA y VGA por separado. Cuéntame también los fps aproximados que ves, y si ves el aviso del
navegador de que la conexión no tiene Internet.

## Por qué esto importa para el carrito

Con este sensor, en QVGA se esperan unos 6 a 7 cuadros por segundo con JPEG por software y un retraso de la cámara de unos 150 ms antes de
contar el WiFi; en VGA solo unos 2. El presupuesto de 300 ms del proyecto queda justo en QVGA. Con una cámara con JPEG por hardware
(por ejemplo OV2640) esos números mejorarían mucho; si la ESP32-CAM definitiva trae uno, hay que repetir esta medición.

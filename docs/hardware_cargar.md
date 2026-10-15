# Cargar el firmware a las placas por comandos (`scripts/cargar.ps1`)

Todo se hace por USB desde PowerShell. El script no usa ni cambia el WiFi de la PC (no te quedas sin
Internet): solo habla con la placa por el puerto serie.

## Las dos placas

- **ESP32 DevKit** (sin cámara, sin PSRAM): es la del carrito. Recibe `baseline` y `nuevo`.
- **Freenove ESP32-WROVER** (con cámara y PSRAM): recibe `camara` (el programa de prueba de la cámara
  se crea en el trabajo H2; mientras no exista, `camara` avisa y no hace nada).

El script distingue una de otra por la PSRAM (esptool no la puede ver). La primera vez que ve una
placa (la reconoce por su dirección MAC) carga un programa mínimo, `scripts/sonda_placa`, y lee lo que
imprime. **Eso sobrescribe una sola vez lo que tuviera esa placa.** La MAC y su tipo se guardan en
`scripts/placas.local.json` (está en `.gitignore`; no se sube). Las veces siguientes no hay sonda.
Con `-Puerto COMx` se salta toda la detección (no comprueba qué placa es).

## Uso

Desde la carpeta del proyecto:

```powershell
.\scripts\cargar.ps1 baseline        # firmware ORIGINAL (copia temporal de original_backup) -> DevKit
.\scripts\cargar.ps1 nuevo           # firmware_carro/ (necesita firmware_carro\secretos.h) -> DevKit
.\scripts\cargar.ps1 camara          # camara_prueba/ -> Freenove (existirá tras H2)
```

Opciones:

- `-SoloCompilar`: compila y se detiene (no busca placa ni sube nada).
- `-Puerto COM6`: usa ese puerto sin detectar nada.
- `-MonitorSegundos 30`: cuánto escucha el puerto serie después de subir (15 por defecto; 0 = no escucha).
- `-EsperarBoot`: último intento con cuenta regresiva para mantener presionado BOOT (ver abajo).

Qué hace, en orden: compila (en una carpeta temporal; el repositorio no se ensucia y `original_backup/`
solo se lee), busca la placa, sube y escucha el puerto serie a 115200. Si la subida falla reintenta a
921600, 512000 y 115200 baudios (el core de ESP32 no ofrece 460800 en su menú). Nunca imprime la
contraseña del WiFi: la oculta de cualquier salida. Solo muestra el nombre de la red y la IP.

Mensajes de error y su significado: el script termina con código 2 si no hay ningún puerto, 3 si no hay
una placa del tipo pedido o hay dos iguales, y 4 si no pudo subir.

## Si no aparece ningún puerto COM

- Usa un cable USB **de datos** (algunos cables solo cargan) y prueba otro puerto USB de la PC.
- En el Administrador de dispositivos, un dispositivo "CP2102 USB to UART Bridge Controller" o "USB-SERIAL
  CH340" con signo de error significa que **falta el driver**. El script lo dice cuando lo ve.
- Para el chip **CP210x** (Silicon Labs): driver "CP210x Universal Windows Driver" de la página oficial
  <https://www.silabs.com/developer-tools/usb-to-uart-bridge-vcp-drivers>. Se instala con
  `pnputil /add-driver silabser.inf /install` en una consola de administrador.
- Para el chip **CH340/CH9102** (WCH): driver de la página oficial del fabricante, <https://www.wch-ic.com>
  (busca CH341SER). No hizo falta en esta PC todavía.
- Descarga siempre del sitio del fabricante y comprueba la firma digital antes de instalar.

## Si la placa no entra en modo de carga

El script ya reintenta solo a menor velocidad. Si aun así sale `No se pudo subir el firmware`:

1. Deja la placa conectada por USB y ejecuta el mismo comando agregando `-EsperarBoot`.
2. Cuando el script diga **MANTEN PRESIONADO el boton BOOT**, presiona y no sueltes BOOT (el botón del
   lado de IO0, a veces rotulado BOOT o IO0).
3. Suelta BOOT en cuanto el script muestre que ya está escribiendo.
4. Si no entra: otro cable, otro puerto USB, desconecta lo que tengas en los pines (sobre todo en GPIO 2 y
   GPIO 0, que si están a 3.3 V al arrancar impiden entrar en modo de carga) y repite.

## Qué se probó (DevKit, 13/10/2026)

- La DevKit quedó en **COM6**: chip USB-serie **CP2102** (CP210x de Silicon Labs, VID 10C4, PID EA60); chip
  **ESP32-D0WD-V3 (revisión v3.1)**, flash de 4 MB, MAC 68:09:47:4b:fb:24. Windows no tenía el driver:
  se instaló el oficial de Silicon Labs (versión 11.6.0.420, firmado por Silicon Laboratories y Microsoft).
- El script detectó que esa placa **no tiene PSRAM** (DevKit), subió el firmware original (`baseline`) a
  921600 baudios sin reintentos y mostró el arranque.
- La Freenove **no estaba conectada** en esa prueba; todo lo de `camara` y su detección queda sin probar.
- Lo que imprime el firmware original: `CPU: 160 MHz`, un error `E (19) task_wdt: esp_task_wdt_init(517):
  TWDT already initialized` (el core ya inicia ese vigilante; el programa sigue y funciona; el firmware
  nuevo ya NO da ese error: reconfigura el vigilante en vez de iniciarlo otra vez y escribe `Watchdog de hardware: 5000 ms`;
  compilado, falta comprobarlo en la placa), `AP Iniciado. IP: 192.168.4.1` y `Servidor listo.`. Red: **CarroFSI** (el nombre sale de `secretos.h`).
- `nuevo` y `baseline` compilan sin errores para la DevKit (969402 y 969538 bytes, 73 % del programa).

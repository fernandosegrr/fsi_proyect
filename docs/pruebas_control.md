# Pruebas de la pantalla de control (con el servidor falso, sin el carro)

Fecha: 2026-10-10. Equipo: Huawei ELS-NX9 (Android 10) conectado por USB a este PC. Se probó
la pantalla `ControlActivity` de la app contra `pruebas_firmware/servidor_falso.py`, que imita
al carro. El carro y la red WiFi real (192.168.4.1) NO intervinieron.

## Cómo correr el servidor falso

Una sola vez, desde la carpeta `pruebas_firmware` (el entorno virtual es local y no se sube a git):

```powershell
python -m venv .venv
```

```powershell
.venv\Scripts\python.exe -m pip install -r requirements.txt
```

Cada vez que se quiera usar:

```powershell
.venv\Scripts\python.exe servidor_falso.py
```

- Escucha en `0.0.0.0:8080`, ruta `/ws` (`--puerto N` para otro puerto).
- Acepta solo `FORWARD`, `REVERSE`, `LEFT`, `RIGHT`, `STOP`, `EBRAKE` y `PING`. Responde `PONG` a `PING`.
- Imprime cada mensaje con la hora y los milisegundos desde el anterior. Cualquier otro texto se imprime como `DESCONOCIDO (ignorado)`.
- Watchdog de 1000 ms como el firmware: si hay motores en marcha y no llega un comando real en 1000 ms, imprime `WATCHDOG: motores detenidos`. El `PING` no renueva el reloj.
- Un solo cliente: si llega otro, cierra al anterior.
- Prueba automática del propio servidor (9 casos): `.venv\Scripts\python.exe probar_servidor_falso.py`.

## Cómo conectar el teléfono (adb reverse)

El teléfono no está en la misma red que el PC, así que se usa un túnel por el cable USB: el
`localhost:8080` del teléfono llega al puerto 8080 del PC.

```powershell
& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" reverse tcp:8080 tcp:8080
```

Después, en la app: **Control del carro**, escribir `ws://localhost:8080/ws` en la dirección y
pulsar **Conectar**. Para abrirla ya con esa dirección:

```powershell
& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" shell am start -n com.fsi.carro/.ControlActivity --es direccion ws://localhost:8080/ws
```

Para quitar el túnel: `adb reverse --remove tcp:8080`. Los mensajes de la app salen en
`adb logcat -s CONTROL:I`.

## Resultados reales (medidos en el Huawei)

Los toques se simularon con `adb shell input swipe x y x y <ms>` (un toque largo en el mismo
punto = botón mantenido) y `adb shell input tap`. Durante la primera conexión además hubo
toques manuales sobre la pantalla que también funcionaron.

- **Conexión**: la app abre el WebSocket, pasa a "Conectado" y empieza a enviar `PING`. En pantalla se ve "Conectado · latencia PING/PONG: 7 ms".
- **Botón mantenido**: al presionar ▲ el servidor recibió `FORWARD` al instante y luego cada 250 ms (15 intervalos medidos: media 251 ms, mínimo 248 ms, máximo 254 ms). Con ◀, ▶ y ▼ pasó igual con `LEFT`, `RIGHT` y `REVERSE`.
- **Soltar**: al soltar cada botón llegó un `STOP` (sin más comandos de movimiento después). El botón STOP central envía `STOP` al tocarlo.
- **PING**: cada 802 ms de media (13 intervalos: 799 a 807 ms).
- **Latencia PING a PONG** (75 mediciones en 60 s, con la app quieta): media 7,6 ms, mediana 7 ms, mínimo 5 ms, máximo 12 ms, p95 9 ms. Ojo: es por cable USB con `adb reverse`; NO representa el WiFi del carro.
- **Reconexión**: se detuvo el servidor mientras la app estaba conectada. La app pasó a "Reconectando" en el mismo instante y reintentó cada 2 s después de cada fallo (cada intento tarda unos 2 s en fallar con el túnel de adb, por eso se ven cada ~4 s). Al volver a levantar el servidor se reconectó sola en el primer reintento posterior (el servidor registró la conexión 0,3 s después de quedar escuchando) y siguió con los PING.
- **Segundo plano (HOME)**: con la app conectada y quieta, al mandarla a segundo plano envió `STOP` y cerró el socket (el servidor imprimió `STOP` y luego `DESCONEXION`). Al volver a la app se reconectó sola.
- **Segundo plano con un botón mantenido**: con ▲ presionado se mandó la app a segundo plano a mitad. Hubo `FORWARD` cada 250 ms hasta ese momento; después `STOP`, `DESCONEXION` y ningún `FORWARD` más.
- Los relojes del teléfono y del PC difieren unos 2 s; por eso las horas del logcat y del servidor no coinciden exactamente.

Fragmento real del servidor al mantener ▲ y mandar la app a segundo plano:

```
13:45:40.150  +   765 ms  FORWARD
13:45:40.403  +   218 ms  FORWARD
13:45:40.652  +   250 ms  FORWARD
...
13:45:41.656  +   250 ms  FORWARD
13:45:41.823  +    31 ms  STOP
13:45:41.862  DESCONEXION: motores detenidos
```

## Qué NO se probó (pendiente con el carro)

- **`bindProcessToNetwork`**: hay un comentario `PENDIENTE HARDWARE` en `ControlActivity.onResume`. Sin esto, Android puede mandar el tráfico por datos móviles aunque el teléfono esté conectado al WiFi del carro, que no tiene Internet.
- **Wi-Fi+ / cambio automático de red del Huawei**: puede sacar al teléfono de la red del carro por no tener Internet. Hay que desactivarlo (WLAN+ / "Cambio automático a datos móviles") y comprobar que no se desconecta durante la prueba.
- **`ws://` real a 192.168.4.1**: la conexión al ESP32 (puerto 80, ruta `/ws`) no se probó; solo `localhost` por USB. Tampoco el tráfico sin cifrar contra esa dirección (la configuración de red permite `ws://` en general y sí funcionó con localhost).
- **El carro real y el `PONG`**: el firmware original nunca contestaba. El firmware nuevo (`firmware_carro`) ya contesta `PONG` a `PING` (probado en PC con `pruebas_firmware`; sin probar en la placa), así que con él la latencia por WiFi debería verse en pantalla.
- **Silencio del carro**: la app desactiva el tiempo de espera de lectura de OkHttp para que no cierre la conexión cuando el carro no manda nada. Con el servidor falso siempre hay tráfico (el PONG), así que no se comprobó que aguante más de 10 s sin ningún mensaje del carro.
- **Latencia y estabilidad por WiFi**: el 7,6 ms medido es del cable USB. Falta medir con el WiFi del ESP32 (alcance, interferencia, pérdida de paquetes) y con el carro en movimiento.
- Un solo cliente: el firmware cierra al cliente anterior al conectar uno nuevo; no se probó con el firmware.
- Presionar dos botones a la vez (multitoque) y toques reales prolongados con la pantalla en uso; solo se simularon toques con `adb`.
- Pantalla apagada (bloqueo del teléfono) en vez de HOME, y teléfonos con Android 11 o más nuevo.

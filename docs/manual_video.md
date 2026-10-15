# Manual con video: sesión compartida y medida del PONG

## 1. La sesión de video compartida

La cámara del carro atiende **un solo cliente de video** a la vez. Por eso la app abre **un solo `VideoCarro`** (`SesionVideo.kt`) y lo comparten las pantallas que lo usan:

| Usuario | Pantalla | Qué recibe |
|---|---|---|
| `USUARIO_INICIO` (0) | Inicio | Solo el estado (cuadros por segundo y red) |
| `USUARIO_MANUAL` (1) | Manual | Los JPEG, que decodifica `VisorJpeg` para mostrarlos |
| `USUARIO_VISION` (2) | Visión y diagnóstico | Todo (la cadena de 3 hilos del detector) |
| `USUARIO_RUTA` (3) | Ejecutar ruta | Todo |
| `USUARIO_DETECCION` (8) | «Ver detección» de Manual | Todo (el detector que dibuja los números) |

La decisión de abrir, compartir, cerrar y reintentar vive en C: `nucleo_c/sesion_compartida.c` (con `pruebas_sesion_compartida`, 43 casos). Reglas:

- **Abrir:** entra el primer usuario. **Compartir:** los demás no abren otra vez (entrar dos veces con el mismo número cuenta una).
- **Cerrar:** al salir el último usuario se espera **1.5 s** de gracia (cambiar de pantalla o girar el teléfono no corta el video) y entonces se cierra.
- **Fallo:** si el video falla, la sesión pasa a FALLO, las pantallas muestran un aviso en banda con «Reintentar» y, además, se reabre sola cada **5 s**.
- La misma lógica (otra instancia) comparte la **conexión de control** con el carro (`SesionCarro.kt`): el video y el manejo son sesiones distintas, así que **el manejo no depende del video**.

Comprobado en el teléfono (registro `SESION_VIDEO`): al ir Inicio → Manual → Inicio → Manual → Rutas, el video se abre **una sola vez**, se queda abierto (estado ABIERTA) mientras alguna pantalla lo usa y se cierra 1.5 s después de entrar a Rutas, que no lo usa.

## 2. Si el video falla en Manual

Manual sigue manejando igual. Aparece un aviso en banda: **«No llega el video de la cámara»** / «Puedes seguir manejando sin video. Revisa que la cámara esté encendida y toca Reintentar», con el botón **Reintentar** (`SesionVideo.reintentar()`: cierra y vuelve a abrir). El interruptor **Video** apaga el video por completo (se recuerda en Ajustes).

## 3. ¿El video empeora la respuesta del control? (registro `MANUAL_VIDEO`)

El video y el WebSocket de control comparten el WiFi del carro. Si el video estorba, el PONG del control tarda más. La pantalla Manual escribe cada 5 s una línea:

```
MANUAL_VIDEO video=on sesion=ABIERTA fps=10.8 modo=jpeg deteccion=off pong_medio_ms=17.7 pong_max_ms=31 pongs=7 conexion=CONECTADO
```

**Regla del proyecto** (`nucleo_c/latencia_video.c`, `pruebas_latencia_video`, 12 casos): si el PONG medio **con video** es **más de 30 ms mayor** que **sin video** (con al menos 5 medidas de cada lado), el video de Manual queda **apagado por defecto** como interruptor. Con 30 ms o menos, se deja encendido.

### Medida en el teléfono con la cámara y el carro FALSOS (conexión por el cable USB)

Solo comprueba que el registro y la regla funcionan; **no representa el WiFi del carro** (el carro real y la cámara comparten el aire, el simulador no).

| Video | `pong_medio_ms` (ventanas de 5 s) | `pong_max_ms` |
|---|---|---|
| Encendido (10.3 a 10.8 fps) | 17.7, 14.1, 14.7, 19.5 | 31, 20, 17, 25 |
| Apagado | 15.3, 15.5, 14.8, 15.0 | 24, 18, 19, 17 |

Diferencia de promedios: unos 1 a 2 ms (muy por debajo de los 30 ms).

### PENDIENTE HARDWARE: medir con el carro y la cámara reales

1. Carro y cámara encendidos; el teléfono en la red `CarroFSI`.
2. Pantalla **Manual**, 1 minuto con el interruptor **Video** encendido y 1 minuto apagado (mismas condiciones, el carro quieto). Leer: `adb logcat -d -s MANUAL_VIDEO`.
3. Promediar los `pong_medio_ms` de cada lado. Si con video son **más de 30 ms** mayores: en Más → Ajustes dejar «Video en Manual» apagado (queda como interruptor en Manual).
4. Anotar aquí los dos promedios y la decisión.

| Condición | PONG medio (ms) | PONG máximo (ms) |
|---|---|---|
| Sin video | _pendiente_ | _pendiente_ |
| Con video | _pendiente_ | _pendiente_ |
| Decisión | _pendiente_ | |

## 4. «Ver detección»

El interruptor abre además la detección de obstáculos sobre el mismo video (usuario `DETECCION`) y dibuja encima **solo los números**: una píldora por zona (IZQ, CEN, DER) con la cercanía (0 a 3) y, si el corredor está encendido, «fuera» cuando el obstáculo cae fuera de la huella del carro. La imagen no se modifica (los números van en otra vista transparente, `VistaNumerosDeteccion`). En Manual la detección **no frena** el carro: manda el usuario.

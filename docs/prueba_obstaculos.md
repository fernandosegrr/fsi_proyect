# Prueba "Obstáculos: cámara" con el carro

Mide cuánto tarda el carro en frenar cuando la cámara de SU cuerpo ve un objeto adelante, y cuántos cm avanza mientras tanto.
Las cuentas están en `nucleo_c/prueba_obstaculo.c` (probadas en PC); la medición, en `ModoSemiautonomo.kt`.

## Preparación

1. Cargar los dos firmwares nuevos, uno por uno (un solo USB conectado a la vez): `scripts\cargar.ps1 nuevo` (DevKit) y
   `scripts\cargar.ps1 camara` (Freenove). Traen canal WiFi fijo y sin ahorro de energía.
2. Encender el carro con la powerbank (primero el DevKit, luego la cámara), conectar el teléfono a la red del carro e instalar la app
   (ya instalada si se hizo `adb install`).
3. En la app: **Visión** → «Fuente del video» en **cámara del carro (WiFi)**; «Modelo de objetos» en **NO** (viene apagado). Revisa que
   debajo diga «Video del carro … red=WiFi del carro» y que NO salga el aviso ⚠.
4. **Control** → Conectar → modo **Semiautónomo** → «Obstáculos: **cámara**» → «Ruta: **Prueba de obstáculo (1 m)**».

## Las pasadas (hacer 5 y, si se puede, 5 más con otro objeto)

- **Objeto:** una caja de cartón o libro grande (al menos 20 cm de ancho y 15 de alto) de un color distinto al piso, o una silla. Algo
  que se vea bien: no un objeto transparente, no uno del color del piso.
- **Colocación:** pon el carro en una marca de salida mirando a una zona libre de 1.2 m. Pon el objeto **a 80 cm de frente**, centrado.
- **Cada pasada:** toca **Iniciar**. El carro avanza, frena solo al ver el objeto y la app **cancela la ruta**: queda detenido. Mide con la
  cinta cuántos cm quedó del objeto (de la parte delantera del carro al objeto) y anótalo. Lleva el carro a la marca y toca **Iniciar** otra vez.
- Si frena demasiado pronto o no frena (choca), anótalo y sigue con la pasada siguiente.
- Al terminar la serie, toca **Cancelar** si algo sigue activo; el resumen se escribe al terminar cada ruta.

## Dónde leer los números

- **En la pantalla de control** (panel semiautónomo): «Última pasada: detectó en … ms, STOP enviado a los … ms, respuesta del carro … ms,
  video … ms → retraso … ms ≈ … cm» y «Pasadas: … | retraso promedio … máximo … | cm promedio … máximo …».
- **En el log** (para que yo los lea): `adb logcat -d -v time OBST_PRUEBA:D VIDEO_STATS:D VIDEO:D *:S`
  (antes de empezar: `adb logcat -G 16M` y `adb logcat -c`). Líneas `OBST_PRUEBA marca_cuadro_ms=… retraso_total_ms=… cm_recorridos=…` y la
  línea `RESUMEN`.
- **Qué comparar:** `cm_recorridos` de la app contra los cm que mediste con la cinta (la distancia que falta hasta el objeto): la distancia
  recorrida desde que el objeto aparece hasta que el carro se detiene debería ser ~ (80 − cm que quedaron al objeto).
- **Video:** en la pantalla de visión (y en el log `VIDEO_STATS` cada 5 s): fps reales, hueco máximo, huecos de más de 500 ms.

## Si el video se corta a mitad de la ruta

El carro frena y queda en ERROR «DETENIDO: se perdió el video» y NO reanuda solo cuando el video vuelve: hay que iniciar otra ruta.

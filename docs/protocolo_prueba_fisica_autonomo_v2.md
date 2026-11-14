# Protocolo de prueba física — autónomo v2

Rama `feat/autonomo-v2`. **Nada de esto se ha ejecutado.** Todo lo que la rama afirma está SIMULADO ([diseño](autonomo_v2_diseno.md)); este protocolo es para convertirlo en VALIDADO EN EL CARRO o en un hallazgo. Va de menor a mayor riesgo: **no pases a un bloque si el anterior no cumplió su criterio.**

Hoja de registro: [pruebas_fisicas_plantilla.csv](pruebas_fisicas_plantilla.csv). Una fila por corrida.

## 0. Reglas de la sesión

- Una mano cerca del carro en todas las corridas. Si va a tocar algo, levántalo o pulsa STOP.
- **Abortar la sesión** (no solo la corrida) si: el carro avanza sin que la app lo mande; no frena con STOP; gira más de una vuelta; el láser no frena a ~12 cm de una pared en la prueba 1.3; la app se cierra dos veces.
- No se desinstala la app ni se borran sus datos. No se usa `erase_flash`. Una placa por USB a la vez. La PC no se une a la WiFi `CarroFSI`.
- No se guardan imágenes de la cámara del carro. Las fotos del resultado se toman con otro teléfono, desde arriba.

## 1. Preparación

| Paso | Qué | Criterio |
|---|---|---|
| 1.1 | 4 pilas AA nuevas en el carro; powerbanks cargadas; teléfono con más de 50 % | Anotar el voltaje de las pilas si hay multímetro |
| 1.2 | Instalar la APK conservando datos (sección 6) | La app abre y conserva rutas y calibración |
| 1.3 | Pantalla Sensores: giroscopio listo, láser vivo. Carro en perfil AUTO frente a una pared: avanzar | Frena solo a ~12 cm (`TOF_PARO`). **Si no frena: abortar la sesión** |
| 1.4 | Piso despejado, luz estable (anotar: ventana, focos, hora). «Aprender piso» | Aprendizaje aceptado. Si lo rechaza por «muy variado», cambiar de lugar |
| 1.5 | Visión: umbral 40, celdas 7, fila mínima 44 %, detector de objetos encendido, autoridad del láser encendida | Anotar los valores reales en la hoja |

Con el firmware actual del carro (v1.1) todo el protocolo es válido. Los bloques marcados **[SEG=2]** y **[cam-v2]** solo aplican si autorizaste cargar el firmware nuevo (sección 7).

## 2. Bloque A — sin obstáculos (riesgo bajo)

| Prueba | Ruta | Repeticiones | Aceptación | Abortar si |
|---|---|---|---|---|
| A1 | Recta de 150 cm, piso limpio | 3 | Llega; sin paradas; error lateral ≤ 5 cm | Se detiene o esquiva sin nada delante 2 de 3 |
| A2 | Recta de 150 cm con una pared a 60 cm más allá del destino | 3 | Llega y dice Terminado sin «esquivar» la pared | Intenta rodear la pared |

## 3. Bloque B — un obstáculo (riesgo medio)

Obstáculo centrado en la línea, a 80–100 cm de la salida, con 1 m libre a cada lado y 1.5 m libres después.

| Prueba | Obstáculo | Repeticiones | Aceptación | Abortar si |
|---|---|---|---|---|
| B1 | Caja de 20 × 20 cm, recta de 150 cm | 5 | 5 de 5 sin tocar; llega; error lateral ≤ 8 cm | Toca la caja 2 veces |
| B2 | La misma caja corrida 15 cm a un lado | 3 | 3 de 3 sin tocar. Anotar si hizo el vistazo (tuerce ~12° y vuelve) | Toca la caja 2 veces |
| B3 | Banquito 35 × 45 cm con patas | 3 | Sin tocar una pata. Anotar: rodeó, pasó por debajo o pidió ayuda | Toca una pata 2 veces |
| B4 | Objeto largo (90 cm a lo largo de la ruta), recta de 250 cm | 3 | Sin tocar; no vuelve a la línea junto al objeto | Gira hacia el objeto estando a su lado |
| B5 | Pared o mueble que cierra el paso | 2 | Mide un lado, mide el otro y pide ayuda con «Causa 16» sin acercarse | Avanza de lado hacia la pared |

En B1, mirar tres cosas nuevas: (a) el carro se aparta, pasa y **avanza bastante más allá de la caja** (unos 60 cm) antes de volver; (b) antes de entrar a la línea **se queda mirando medio segundo**; (c) si retrocede, es solo por donde vino.

## 4. Bloque C — falsos positivos provocados (riesgo bajo)

Recta de 150 cm sin obstáculos.

| Prueba | Qué | Repeticiones | Aceptación |
|---|---|---|---|
| C1 | Tu sombra cruzando la ruta a 50 cm del carro | 3 | No esquiva ni pide ayuda. Anotar si se detiene un momento |
| C2 | Mancha de sol o linterna sobre el piso, a un lado de la línea | 3 | Igual. Anotar si hace el vistazo |
| C3 | Mancha de sol o linterna sobre la línea | 3 | Igual |
| C4 | «Aprender piso» con la luz encendida y correr con la luz apagada (o al revés) | 2 | Anotar qué pasa. **Esta es la prueba que más informa**: de aquí sale si hace falta fijar la exposición |

Después de cada corrida de este bloque, **guarda la caja negra** (sección 8): sus líneas `VTEL` dicen, sin imágenes, si el histograma entero se corrió (exposición), si subieron unas pocas celdas (mancha) o si la cámara cambió sus medias de color (`CAB camara_stats`, con cam-v2).

## 5. Bloque D — fallas y rutas con giros (riesgo medio–alto)

| Prueba | Qué | Aceptación | Abortar si |
|---|---|---|---|
| D1 | Recta de 200 cm: a media ruta, apagar la WiFi del teléfono | El carro se detiene en ~1 s (watchdog) | Sigue avanzando más de 2 s |
| D2 | Recta de 200 cm: tapar la cámara con la mano 2 s | Se detiene por pérdida de video y lo dice | Sigue a ciegas |
| D3 | Recta con el detector de objetos apagado en Visión | Al iniciar avisa «Percepción incompleta» | — |
| D4 | Ruta en L (150 + 100 cm), sin obstáculos | Llega; error ≤ 10 cm | — |
| D5 | Cuadrado de 100 cm, sin obstáculos | Llega; error ≤ 15 cm | — |
| D6 | Dos cajas seguidas a 80 cm una de otra, recta de 300 cm | Sin tocar ninguna | Toca una caja 2 veces |
| D7 **[SEG=2]** | Con el carro frente a una pared a 10 cm, mandar adelante desde la cruceta en AUTO | Los motores no dan ni un pulso | Avanza |

## 6. Compilar e instalar la APK conservando datos

No ejecutado por el asistente. En PowerShell, desde la raíz del repositorio:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/compilar_app.ps1
```

```powershell
& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r app_android/app/build/outputs/apk/debug/app-debug.apk
```

Si `install -r` falla por firma distinta (`INSTALL_FAILED_UPDATE_INCOMPATIBLE`): **detente. No desinstales** (se pierden rutas, calibración y el piso). Avisa y se resuelve con la misma llave de depuración de la instalación anterior.

## 7. Firmware nuevo (solo con tu autorización)

No ejecutado por el asistente. Ninguno es obligatorio para probar la rama.

- **Carro `SEG=2`**: cierra el fallo de los comandos desconocidos y bloquea `FORWARD` contra un obstáculo. Riesgo: bajo (559 pruebas en PC); es el recomendado.
- **Cámara `cam-v2`**: añade números de luz a `/stats` y apaga la autoprueba. Riesgo: bajo, pero no se pudo probar con el sensor real.

Procedimiento propuesto, una placa por USB a la vez, sin `erase_flash`:

```powershell
.\scripts\cargar.ps1 nuevo -SoloCompilar
```

```powershell
.\scripts\cargar.ps1 nuevo -Puerto COM6
```

```powershell
.\scripts\cargar.ps1 camara -Puerto COM8
```

Después de cargar el carro: pantalla Sensores → la línea CAPS debe traer `SEG=2,TUSO=1`. Repetir 1.3 antes de cualquier ruta. Para volver atrás: cargar el firmware del tag `v1.1`.

## 8. Qué registrar

**A mano, en la hoja:** distancia final al destino con cinta (a lo largo y de lado, en cm), contactos (sí/no y con qué), si pidió ayuda y el número de causa, duración, y una foto del resultado desde arriba con otro teléfono.

**Lo saca la caja negra** (un archivo por corrida; anota su nombre en la hoja):

```powershell
& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" pull /sdcard/Android/data/com.fsi.carro/files/cajas_negras cajas_negras
```

- cabecera con la configuración real con la que salió la ruta;
- por tick: estado, comando, pose estimada, láser, fuentes por zona, vistazos, causa;
- por cuadro: histograma de distancias al piso, brillo, celdas por zona y fila;
- cada decisión del navegador: ancho medido en el barrido, miradas, pasos extra, retrocesos;
- `TOF_PARO`, watchdog y el resultado final.

El nombre y la carpeta también aparecen en Ejecutar ruta → Avanzado.

## 9. Criterio global

- **Se puede seguir hacia v1.2** si: bloque A y B1 completos sin contacto, C1–C3 sin esquives ni ayudas, D1 y D2 detienen.
- **Vuelve al escritorio** si: algún contacto en B1; el carro gira hacia un objeto que tiene al lado; un falso positivo hace esquivar; la caja negra queda vacía.
- Banquito, silla y objeto bajo son los casos donde el simulador ya dice que no se llega al 99 %: lo que pase ahí es información, no un fracaso de la sesión.

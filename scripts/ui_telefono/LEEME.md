# Pruebas de la interfaz de la app en el teléfono (carro y cámara FALSOS)

Programas que abren la app en un teléfono conectado por USB y miden o fotografían su interfaz. **El carro no se mueve**: la app habla con el servidor falso del PC y el video es la cámara falsa (imágenes sintéticas). Nacieron en la revisión de parpadeos del 24-nov-2026 ([docs/revision_ui_android.md](../../docs/revision_ui_android.md)), donde una de estas pruebas encontró un cierre de la app que la lectura del código no había visto.

## Antes de correrlos

1. Teléfono con depuración USB y la app instalada (`scripts/instalar_app_adb.py`). Si hay más de un dispositivo: `$env:ANDROID_SERIAL = "<serie>"` en PowerShell (con uno solo no hace falta).
2. El carro falso y la cámara falsa corriendo en el PC, y los puertos pasados al teléfono:

```powershell
pruebas_firmware\.venv\Scripts\python.exe pruebas_firmware\servidor_falso.py --sensores
python scripts\camara_falsa.py --puerto 8085
adb reverse tcp:8080 tcp:8080
adb reverse tcp:8085 tcp:8085
```

3. El teléfono desbloqueado y con la pantalla encendida (los programas no meten ningún PIN).

## Qué hace cada uno

| Programa | Qué hace |
|---|---|
| `prueba_telefono.py medir <etiqueta> <carpeta>` | Manual con el video bien, Manual con el video fallando, una ruta corta contra el carro falso (esta escena es frágil: ver abajo) y el brillo del punto de conexión. Por escena: cuadros dibujados, cuadros lentos y cuántos cuadros hacen una pasada de medir y acomodar (`dumpsys gfxinfo`), y dónde queda cada botón de manejo (`dumpsys activity top`). Deja un `<etiqueta>_medidas.json` |
| `prueba_telefono.py galeria <etiqueta> <carpeta>` | Abre cada pantalla en vertical, en horizontal y (algunas) en tema claro y guarda una captura de cada una. **Si una pantalla no se abre (la app se cerró), lo dice**: es la forma más barata de encontrar un cierre por un layout que existe en dos versiones |
| `extra_telefono.py <etiqueta> <carpeta>` | Manual en horizontal con el video bien y fallando, y el aviso global con el carro apagado (cuánto empuja a lo de abajo) |
| `ciclo_aviso.py <etiqueta>` | Durante ~22 s mide si el aviso «La cámara no responde» aparece y desaparece con cada reintento de la sesión de video (cámara rechazada y cámara apagada) |
| `ruta_corta.py <carpeta>` | Corre **una** ruta corta (la «Ruta en L», unos 6 s) contra el carro falso y guarda capturas de la ruta en marcha (cabecera con el PARO) y del resumen «Terminado». Espera el momento justo del ciclo de la cámara falsa para que la comprobación previa no se niegue; hasta 3 intentos |
| `comparar.py antes_medidas.json despues_medidas.json` | Pone lado a lado dos corridas de `medir` |
| `hoja.py salida.png ancho columnas recorte archivos…` | Junta capturas en una hoja para revisarlas de un vistazo (recorta la barra de estado) |

## Qué NO miden

- No miden nada con el carro ni la cámara reales: el WiFi, la latencia de verdad y el video de la placa pueden comportarse distinto.
- `dumpsys gfxinfo framestats` entrega solo los últimos 10 cuadros por lectura en este teléfono (Huawei, Android 10): la columna «con acomodar > N ms» es una **muestra**, no la cuenta completa. Los números de `gfxinfo` (cuadros lentos, hilo principal lento) sí cuentan todos los cuadros de la ventana.
- Un cuadro «lento» depende de la frecuencia de la pantalla (60 o 90 Hz) y de cuánto trabajo tenga el teléfono en ese momento: comparar corridas hechas con el mismo teléfono, el mismo cable y sin otras apps abiertas.
- La escena de la ruta de `medir` **no es fiable**: antes de correr, la app comprueba el piso con la cámara y se niega («Hay algo frente al carro») si ve la «caja» que `camara_falsa.py` dibuja a partir del segundo 5 de cada ciclo de 12 s, y entonces la cifra mide otra pantalla (en una corrida fue el editor de rutas). Mira siempre la captura de esa escena antes de usar su cifra, o salta la escena con `--sin-ruta` y usa `ruta_corta.py`.
- La batería y el modo de ahorro de energía cambian cuántos cuadros se pierden: la corrida de la app anterior se hizo con 10 a 12 % de batería y las de la nueva con 14 a 36 % (cargando por el cable). Para comparar de verdad, carga el teléfono por igual en las dos y repite cada corrida más de una vez.
- Las capturas se hacen solo si la app está al frente, y usan la cámara falsa. No guardes capturas con la cámara real.

# Imágenes reales y capturas del carro FSI

Esta carpeta está reservada para las **fotografías físicas del carro, su electrónica y las capturas de pantalla de la aplicación Android**.

---

## 1. Convención de nombres

Para que el generador del manual en PDF (`docs/manual/generar_manual.py`) asocie automáticamente cada fotografía a su lugar en el documento, se utiliza la convención definida en `docs/lista_de_fotos.md`:

```text
foto_NN_<slug>.jpg (o .png)
```

Ejemplos:
- `foto_01_carro_frontal.jpg`
- `foto_02_carro_lateral.jpg`
- `foto_04_electronica_rotulada.jpg`
- `foto_09_app_conexion.png`

---

## 2. Inclusión automática en el manual

1. El script `docs/manual/generar_manual.py` consulta el archivo `docs/lista_de_fotos.md`.
2. Si el archivo de la foto existe en esta carpeta (`docs/imagenes/`), el generador la inserta directamente con su pie correspondiente.
3. Si la foto **no** existe todavía, el generador dibuja un recuadro visible de **«FOTO PENDIENTE»** indicando el identificador, el encuadre requerido y el nombre de archivo esperado. De este modo, basta con tomar la foto, guardarla aquí con el nombre correspondiente y volver a ejecutar el generador del manual.

---

## 3. Pautas para las fotografías

- **Fondo limpio y neutro:** Tomar las fotos sobre mesa clara o fondo despejado.
- **Iluminación uniforme:** Evitar sombras duras y reflejos que tapen los números de pin o rótulos de las placas.
- **Privacidad y seguridad:** NO fotografiar notas adhesivas, credenciales de WiFi, PINs o cuentas personales.
- **Resolución:** Fotos nítidas a resolución media (150–200 dpi recomendado para mantener el PDF final en menos de 15 MB).

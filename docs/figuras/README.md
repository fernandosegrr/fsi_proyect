# Figuras y láminas del Manual Técnico (Versión final 1.1)

Todas las figuras y láminas del manual se generan de manera reproducible mediante `docs/figuras/generar_figuras.py` (que delega en `docs/figuras/generar_figuras_v2.py`). Ninguna imagen se edita a mano: cualquier cambio se realiza en el script generador y se vuelve a ejecutar.

## Salidas generadas

- **`docs/figuras/v2/*.png` y `*.pdf` (`220 dpi`):** las 12 figuras y láminas empleadas directamente por `docs/manual/generar_manual.py`.
- **`docs/figuras/fig_*.png` y `*.svg`:** copias con los nombres históricos en la raíz de `docs/figuras/` para compatibilidad con los enlaces de la documentación en Markdown.

| Figura en el manual | Archivo en `docs/figuras/v2/` | Copia compatible en `docs/figuras/` | Capítulo | Contenido técnico |
|---|---|---|---|---|
| **Figura 1** | `arquitectura.png` | `fig_01_sistema.png` / `.svg` | Cap. 2 | Topología de tres nodos (Android, ESP32 DevKit, Freenove), enlaces de red y tres fuentes de energía. |
| **Figura 2** | `alimentacion.png` | `fig_10_alimentacion.png` / `.svg` | Cap. 2 | Tres suministros independientes (Powerbank #1, portapilas 4×AA, Powerbank #2 exclusiva de cámara) y tierra común `GND`. |
| **Figura 3** | `seguridad.png` | `fig_02_capas_firmware.png` / `.svg` | Cap. 2 | Defensa en profundidad de 4 capas (operador, supervisión Android J16, watchdog 1000 ms y paro láser a 120 mm). |
| **Figura 4** | `lamina_carro.png` | *(Lámina fotográfica 2×2)* | Cap. 3 | Cuatro vistas reales del prototipo físico ensamblado (`docs/imagenes/foto_01..04`). |
| **Figura 5** | `comandos.png` | `fig_03_flujo_comando.png` / `fig_11_protocolo.png` | Cap. 4 | Cola de 32 órdenes, bucle `loop()` en C99, lazos `RUMBO`/`GIRO` y retorno de telemetría `SENS`. |
| **Figura 6** | `camara_modos.png` | `fig_09_camara_modos.png` / `.svg` | Cap. 5 | Tres modos de entrega de la cámara Freenove (`MJPEG`, `RGB565` crudo y celdas `UDP :4210`). |
| **Figura 7** | `navegacion.png` | `fig_04_estados_navegador.png` / `fig_05_pose_j15.png` / `fig_12_calibracion.png` | Cap. 7 | Planificación en cuadrícula de 20 cm, guiado `RUMBO`, giro cerrado `GIRO` e integración de pose en décimas de grado. |
| **Figura 8** | `esquive.png` | `fig_06_esquive.png` / `.svg` | Cap. 7 | Geometría del esquive en 3 tramos con margen de 150 mm y estado `VALIDADO EN EL CARRO` (B6, 6 corridas). |
| **Figura 9** | `percepcion.png` | `fig_07_corredor.png` / `.svg` | Cap. 8 | Tubería multimodal de percepción (YCbCr, EfficientDet-Lite0, láser VL53L0X, corredor y mutex `candado_vision`). |
| **Figura 10** | `pantallas.png` | `fig_08_pantallas_app.png` / `.svg` | Cap. 9 | Mapa de las 4 pestañas principales, las 5 actividades completas y las sesiones compartidas en Android. |
| **Figura 11** | `lamina_app.png` | *(Lámina de capturas 1×3)* | Cap. 9 | Tres estados operativos reales de la app (`Conectado`, `Reconectando` y `Sin conexión`). |
| **Figura 12** | `desarrollo.png` | *(Diagrama de evolución)* | Cap. 14 | Evolución técnica del sistema en 5 etapas hasta el cierre de precisión J16. |

## Cómo regenerar

```powershell
python docs/figuras/generar_figuras.py
```

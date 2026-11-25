# Construcción del Manual Técnico (v1.2 — base v3; autónomo v3 pendiente de validar en el carro)

El manual técnico del proyecto **LIDIA (*Carro FSI*)** se redacta de forma modular en `docs/manual/capitulos/` y se compila de manera reproducible en un solo comando mediante `docs/manual/generar_manual.py`. El PDF resultante es `docs/manual/Manual_Proyecto_Carro_FSI.pdf`; la guía para exponer está en [../guia_exposicion.md](../guia_exposicion.md).

## Estructura del manual v1.2

| Archivos en `capitulos/` | Qué contienen |
|---|---|
| `01_introduccion.md` | Propósito, alcance y convenciones de evidencia (`VALIDADO EN EL CARRO`, `SIMULADO`, `NO MEDIDO`, `PENDIENTE`) |
| `01a_mapa_general.md`, `01b_glosario_general.md`, `01c_si_te_preguntan.md` | Parte para exponer: mapa de una página, glosario general alfabético e índice «si te preguntan X, ve a la página Y» |
| `01d_` a `01l_` (subsistemas A a I) | Una explicación por subsistema (hardware, firmware, cámara, app, núcleo en C, visión, seguridad, simulador y pruebas, proceso), siempre con las mismas siete secciones: qué es, cómo funciona, cómo lo hicimos, cómo lo probamos, límites, términos clave y preguntas probables |
| `02_` a `17_` | Manual técnico de referencia (base v1.1 con las revisiones de v2 y v3) |

Los capítulos A a I resumen y remiten a los capítulos de referencia («Más detalle: capítulo N»). Todo lo que no estaba en el código ni en los documentos del repositorio quedó marcado en el texto como `CONFIRMAR CON EL EQUIPO: <pregunta>` y se reúne en [../pendientes_confirmar.md](../pendientes_confirmar.md).

## Cadena de herramientas local

- **ReportLab (`SimpleDocTemplate` + `TableOfContents` en varias pasadas):** maquetación tipográfica con fuentes `DejaVu Sans` y `DejaVu Sans Mono` incrustadas, encabezados jerárquicos, tablas con repetición de cabecera, bloques de código que conservan los espacios (diagramas ASCII), bloques de fórmulas y marcadores interactivos del PDF (*bookmarks*).
- **Referencias de página `{{pag:CLAVE}}`:** en cualquier texto o tabla se sustituyen por el número real de página del encabezado y quedan como enlace dentro del PDF. Claves: `A` a `I` (subsistema), `A.3` (sección 3 del subsistema A), `c7` y `c7.2` (capítulo 7 y su sección 2), `mapa`, `glosario` e `indice`. El generador arma el PDF las veces necesarias hasta que los números de página dejan de cambiar y falla si una clave no existe.
- **Matplotlib (`docs/figuras/generar_figuras_v2.py`):** generación programática de los 10 diagramas vectoriales y las 2 láminas fotográficas compuestas en `docs/figuras/v2/` a `220 dpi`.
- **PyMuPDF (`pymupdf`):** limpieza de recursos tipográficos no utilizados (`/F1 Helvetica`) y verificación de metadatos y fuentes incrustadas.
- **`revisar_manual.py`:** `plantilla` comprueba que los nueve subsistemas tienen exactamente los siete encabezados, de 8 a 14 términos, de 3 a 5 preguntas y etiquetas de evidencia; `glosario` rehace `01b_glosario_general.md` con los términos clave de A a I; `pendientes` rehace `docs/pendientes_confirmar.md`.

## Cómo regenerar el PDF completo

Con el entorno de Python del repositorio (`docs/manual/requisitos.txt`), ejecutar desde la raíz:

```powershell
python docs/manual/revisar_manual.py plantilla
python docs/manual/revisar_manual.py glosario
python docs/manual/revisar_manual.py pendientes
python docs/manual/generar_manual.py --sin-figuras
python docs/verificar_enlaces.py
```

Sin `--sin-figuras`, el script ejecuta antes `docs/figuras/generar_figuras.py` y regenera las figuras (reescribe archivos de `docs/figuras/`). Con `--sin-figuras` reutiliza las existentes, que es lo habitual cuando solo cambió el texto.

## Diferencias conocidas entre documentos antiguos y el código (v1.2)

Los capítulos `02_` a `17_` y varios documentos de `docs/` vienen de la base v1.1. En la v1.2 se corrigió lo que el código desmentía y se pudo comprobar: PWM de `1500 Hz`, giroscopio a `100 Hz` con calibración de `250` muestras, NDK `30.0.16248370` y CMake `4.1.2`, los ocho estados del navegador, los nombres reales de los comandos y respuestas del protocolo (`REVERSE`, `GIRAR`, `IMU_CAL`, `ACK_*`, `GIRO_OK`; capítulo 6 y anexo 16), las banderas de `SENS` y la cadena `CAPS`, el controlador de giro (`GIRAR` frena por anticipación, no por una ventana de `±2.0°`), el nombre `TIEMPO_WATCHDOG_MS`, el puerto `80` y los núcleos de la cámara, el paquete `com.fsi.carro`, la caja negra (ya existe), las constantes del navegador (nota al inicio de `docs/navegador.md`) y las cifras del Monte Carlo contra `docs/precision/monte_carlo_1000.txt`.

Quedan sin corregir estas diferencias que reportó la revisión hecha al escribir los capítulos A a I (quien armó el release no las comprobó una por una). **Ante cualquier duda manda el código y, después, los capítulos A a I:**

- `docs/camara_prueba.md`: la resolución por defecto que cita no coincide con la del código de la cámara.
- `docs/latencia.md`: el hueco de video que usa el texto (`500 ms`) no coincide con el del código (`700 ms`).
- `capitulos/05_camara.md`: las constantes `RECUP_*` y varios tiempos de recuperación de la cámara.
- `capitulos/07_navegacion.md`: nombres de constantes que no existen en `nucleo_c/navegador.h`.
- `capitulos/09_aplicacion.md`: los botones de giro de `±90°` que describe no coinciden con la interfaz actual.
- `docs/codigo/puente.md`: describe otra API de `puente.c` y `3,467` líneas contra las `3,644` del código.
- `docs/fusion.md`: describe una fusión de percepción anterior a la de v3.

Las preguntas que el equipo debe contestar antes de exponer están en [../pendientes_confirmar.md](../pendientes_confirmar.md).

## Documentos de soporte en esta carpeta

- [Manual_Proyecto_Carro_FSI.pdf](Manual_Proyecto_Carro_FSI.pdf): PDF de la versión v1.2 (base v3). La portada dice «v1.2 (autónomo v3, pendiente de validar en el carro)».
- [diagnostico_v1.md](diagnostico_v1.md): diagnóstico visual página por página del borrador previo a la reescritura (histórico).
- [plan_final.md](plan_final.md): estructura de 16 capítulos, fuentes verificadas y catálogo de figuras y tablas de la versión 1.1 (histórico).
- [auditoria_afirmaciones.md](auditoria_afirmaciones.md): trazabilidad de las afirmaciones técnicas, cifras y etiquetas de evidencia de la versión 1.1.

El PDF de la versión anterior (v1.1) ya no está en esta carpeta: se conserva en la etiqueta `v1.1` del repositorio, en la misma ruta (`docs/manual/Manual_Proyecto_Carro_FSI.pdf`), solo como referencia. Una copia suelta llamada `Manual_Proyecto_Carro_FSI_v1.1.pdf` puede existir en el equipo de quien armó la entrega, pero no forma parte del repositorio (no está versionada). El manual vigente es el de esta carpeta.

# Diagnóstico visual y editorial del manual previo (`Manual_Proyecto_Carro_FSI.pdf`)

Fecha de revisión: `2026-11-10`  
Rama de trabajo: `docs/manual-final` (creada desde `main` en `cd90001`)  
Método: generación reproducible del PDF actual, renderizado de **todas** sus páginas a PNG a 150 dpi con PyMuPDF e inspección visual página por página con lector de imágenes, complementada con la auditoría del manual original de 79 páginas (`c6ee161`).

---

## 1. Síntesis ejecutiva

Se diagnosticaron dos estados previos del entregable PDF en el repositorio:

1. **Manual original v1 (79 páginas, `c6ee161` / `v1.0`):** construido concatenando directamente 16 archivos Markdown internos de `docs/` sin edición unificada. Presentaba bloques negros por diagramas ASCII con caracteres de dibujo de cajas, encabezados duplicados, metadatos de proceso interno, rutas y números de línea en el cuerpo, figuras con letra ilegible y texto `"Fuente: docs/..."` incrustado dentro de la imagen, numeración de figuras desordenada (`Figura 10` antes de `Figura 8`), capturas de la aplicación rotuladas como `"Fotografía"` con el nombre del archivo y sin explicar sus estados de conexión, contradicción sobre el número de baterías portátiles USB (*powerbanks*) y el banco completo de preguntas de examen del sínodo en el entregable.
2. **Borrador intermedio (67 páginas, `5d5c628`):** aunque eliminó los bloques ASCII y las preguntas del sínodo, **no incluye nada del cierre J16 ni la validación física del esquive B6 en el carro (10 de noviembre de 2026)**, sigue declarando el esquive como `NO MEDIDO` en texto y figuras, cita conteos de pruebas obsoletos (`1598` y `447` en lugar de `1655` y `540`), omite la institución (`UNIVERSIDAD DE GUANAJUATO`) y la etiqueta `"Versión final 1.1"` en la portada, **no inicia los capítulos en página nueva** (dejando títulos de capítulo a mitad o al pie de página), agrupa todas las figuras al final de cada capítulo lejos del texto que las explica, carece casi por completo de tablas técnicas y ejemplos numéricos, e infla las páginas 52 a 67 (Capítulo 17) repitiendo literalmente la misma frase genérica más de 90 veces.

Por ello, la **Versión final 1.1** requiere una reescritura integral de capítulos, figuras y generador tipográfico.

---

## 2. Defectos del manual v1 original (79 páginas)

| Categoría | Páginas afectadas (v1) | Descripción exacta del defecto | Corrección en la Versión final 1.1 |
|---|---|---|---|
| **Diagramas ASCII como cuadros negros** | p. 5, 7, 17, 45 | Los bloques ASCII con caracteres Unicode de cajas (`┌─┐│└┘`) carecían de glifos en la fuente monoespaciada base y se imprimían como hileras de rectángulos negros (`.notdef`). | Prohibido el arte ASCII. Se reemplaza por diagramas vectoriales/PNG a 220 dpi con fuente `DejaVu Sans` incrustada y tablas reales de ReportLab. |
| **Documentos pegados y títulos duplicados** | p. 4, 8, 12, 16, 20, 26, 30, 36, 40, 44, 51, 56, 61, 64, 69, 73 | Cada capítulo pegaba un archivo de `docs/` cuyo `# Título` interno chocaba con el cintillo `"CAPÍTULO N"` en estilo, tamaño y numeración. | Redacción original por capítulo con jerarquía única de 3 niveles (`1.`, `1.1`, `1.1.1`). |
| **Metadatos internos y restos del proceso** | p. 4, 8, 12, 16, 20, 26, 30, 36, 40, 44, 47, 51, 56, 61, 64, 69, 73 | Bloques iniciales con `"Estado: documento de referencia"`, `"Rama: docs/documentacion-completa"`, `"Fuentes principales:"`, `"Fecha de consolidación"`, y frases como `"por indicación del dueño"`, `"los defectos se anotan sin corregirlos"`, `"Prohibida su alteración sin verificación de código"`, además de citar el commit viejo `deae8b6`. | Eliminados por completo. El manual se redacta como documento técnico final autónomo. |
| **Rutas y números de línea en el cuerpo** | Todo el documento | Citas constantes como `firmware_carro/config.h:66-76` o `nucleo_c/navegador.c:69-85` interrumpiendo la prosa técnica. | En el cuerpo no van rutas ni líneas. Al final de los capítulos 1 a 15 va una sola línea `Dónde está en el código:` con nombres de archivo, y en el Anexo 16.3 la tabla completa de rutas del repositorio. |
| **Páginas casi vacías** | p. 11, 25, 29, 39, 55, 68 | Saltos forzados antes de figuras grandes dejaban páginas con 3 a 6 líneas y más del 75 % en blanco. | Los capítulos inician en página nueva (`PageBreak`), pero dentro de cada capítulo las figuras se dimensionan e integran sin dejar páginas a medio llenar. |
| **Figuras con texto minúsculo y metadatos internos** | p. 5, 9, 13, 18, 22, 27, 32, 37, 41, 45, 52 | Las figuras `fig_01`..`fig_11` incluían un cintillo superior y un pie `"Fuente: docs/... · ..."` dentro del propio PNG, con tipografía menor a 6 pt al escalar al ancho de página. | Se regeneran las 12 figuras en `docs/figuras/generar_figuras.py` sin texto `"Fuente: docs/..."` ni nombres de archivo internos, con tipografía mínima equivalente a $\ge 8.5$ pt impresos. |
| **Pies de figura con asterisco literal** | p. 5, 9, 13, 18, 22, 27, 32, 37, 41, 45, 52 | Los pies `*Figura N. ...*` se procesaban sin cursiva y mostraban los asteriscos `*` literales. | Pies de figura formateados con estilo tipográfico propio y numeración secuencial automática. |
| **Numeración de figuras desordenada** | p. 32 vs p. 41 y p. 45 | La `Figura 10` aparecía en el Capítulo 8 (p. 32), antes de la `Figura 8` (p. 41) y la `Figura 9` (p. 45). | Numeración estricta por orden de aparición (`Figura 1` a `Figura 12`) verificada automáticamente. |
| **Capturas de la app rotuladas como "Fotografía" y sin contexto** | p. 15, 23, 33, 38, 42, 46, 54 | Las capturas de pantalla de Android se titulaban `"Fotografía — ... (foto_10_app_inicio.png)"` y mezclaban estados (`Conectado`, `Reconectando`, `Sin conexión`) sin explicar por qué. | Se presenta una lámina comparativa de 3 capturas (`Figura 11`) que explica explícitamente los tres estados de conexión de la interfaz. |
| **Contradicción sobre las baterías USB (*powerbanks*)** | p. 6 vs p. 10 y p. 14 | Una sección afirmaba que una sola *powerbank* alimentaba la ESP32 y la cámara, y otra decía que se desconocía. | Corregido y unificado en todo el manual y diagramas: la cámara Freenove tiene su **propia** *powerbank* USB, la ESP32 DevKit usa **otra** *powerbank* USB, y los motores usan **4 pilas AA** con tierra común (`GND`) hacia el driver TC1508A y la DevKit. |
| **Banco de preguntas del sínodo en el entregable** | p. 73–79 (Cap. 16) | Incluía el guion de defensa oral y 15 preguntas de examen con respuestas sugeridas. | Eliminado del manual PDF (permanece únicamente en `docs/exposicion.md`). |

---

## 3. Diagnóstico visual página por página del borrador de 67 páginas (`5d5c628` / `cd90001`)

Al renderizar e inspeccionar las 67 páginas actuales (`page_01.png` a `page_67.png`), se identificaron los siguientes defectos estructurales, visuales y de contenido:

1. **Portada incompleta (`page_01.png`):**
   - Omite el nombre de la institución (**UNIVERSIDAD DE GUANAJUATO — División de Ingenierías · Campus Irapuato-Salamanca**).
   - Omite la identificación **"Versión final 1.1"**.
   - Muestra `"Entrega: 1 de diciembre de 2026"` en lugar de la fecha real de cierre (**10 de noviembre de 2026**) y refiere al commit anterior al merge de J16.
2. **Ausencia total del cierre J16 y desactualización grave del estado de validación (`page_07.png`, `page_08.png`, `page_16.png`..`page_29.png`, `page_37.png`..`page_45.png`, `page_49.png`):**
   - Afirma repetidamente que la maniobra de esquive y reincorporación está `NO MEDIDA` en el carro (por ejemplo, p. 8: *"La corrección de pose y reincorporación del cierre J15 cuenta con resultados de simulación... su efecto en el carro real permanece NO MEDIDO"*; p. 26: `Figura 7` con el rótulo `"J15: NO MEDIDO en el carro"`; p. 38–39: sección `"12.3 Reincorporación J15: simulada vs no medida"`; p. 45: `Figura 11` con `"La validación física de la corrección de pose sigue pendiente"`).
   - No incluye las **6 corridas físicas en el carro del esquive B6** realizadas el 10 de noviembre de 2026 (`FW:6d1b213-20261110`, `APP:0.4-6d1b213`, 0 toques de caja en 6 corridas, 5 corridas autónomas completas con error lateral aproximado entre `~0` y `~10 cm` [`~10, ~10, ~5, ~0, ~0 cm`] y 1 corrida detenida en `AYUDA` en el punto `4/10` sin contacto y sin causa identificada).
   - Omite todos los mecanismos de ingeniería de **J16**:
     - Control proporcional-integral de rumbo en rectas (`firmware_carro/rumbo.c`, `RUMBO:ON`, `RUMBO:OFF`, `RUMBO:<décimas>`, `RUMBO?`, `aprendido_mil`).
     - Factor de escala del giroscopio en partes por millón persistido en NVS (`IMU_ESCALA:<ppm>`, `IMU_ESCALA?`, campo `IMUS=<ppm>` en `CAPS`, `calgiro_escala` en el núcleo C y calibración guiada de 3 pasos en `SensoresActivity`).
     - Aviso explícito de vencimiento del *watchdog* de comunicación (`WD:<n>`) y supervisión en la aplicación Android (`ModoSemiautonomo.kt`: paro por `WD`, paro por silencio de `SENS` > 1.5 s y paro por `TOF_PARO` no atendido; `ControlLogica.kt`: corte del latido `FORWARD` ante `TOF_PARO`).
     - Integración de pose en décimas de grado (`rumbo_x10`) sin truncamiento por paquete, margen longitudinal de esquive ampliado a `150 mm` (`NAV_PASO_MARGEN_MM`), corrección de `override_largo_mm` en `Recta 150 cm`, bandera `descartar_dt` al caer a giro por tiempo, candado mutex `candado_vision` en `puente.c` y validación de calibración guardada al abrir la pestaña `Rutas`.
     - Simulación Monte Carlo de 1,000 corridas por escenario y 5 configuraciones (50,000 corridas en total) documentada en `docs/precision/monte_carlo_1000.txt`.
     - Conteos reales de pruebas automatizadas (`22` programas y `1,655` casos en `nucleo_c`, `5` programas y `540` casos en `pruebas_firmware`, `54` en servidor falso y `18` en verificador de versión) y los artefactos validados (`carro_fsi_0.4-6d1b213.apk` y `FW:6d1b213-20261110` con sus hashes SHA-256).
3. **Los capítulos no inician en página nueva (`page_06.png`, `page_11.png`, `page_14.png`, `page_20.png`, `page_23.png`, `page_30.png`, `page_33.png`, `page_35.png`, `page_37.png`, `page_42.png`, `page_43.png`, `page_46.png`, `page_48.png`, `page_52.png`):**
   - `generar_manual.py` concatena todos los capítulos en un solo flujo sin insertar `PageBreak()` antes de cada `# Capítulo`.
   - Como consecuencia, el **Capítulo 1** empieza a mitad de la página 6 debajo del índice; el **Capítulo 6** (`page_20.png`) y el **Capítulo 12** (`page_37.png`) arrancan al final de la página con apenas 3 líneas sueltas; y los capítulos 13, 14, 15, 16 y 17 comienzan a mitad de página.
4. **Ubicación defectuosa de figuras y de la línea `"Dónde está en el código"` (`page_06.png`, `page_10.png`, `page_13.png`, `page_16.png`, `page_25.png`, `page_26.png`, `page_29.png`, `page_32.png`):**
   - En `01_introduccion.md`, la línea `Dónde está en el código:` estaba escrita tras la sección 1.1.
   - `generar_manual.py` extrae todas las llamadas `[FIGURA:...]` de su posición original y las acumula juntas al final del capítulo con una frase robótica idéntica (`"La Figura X presenta: ..."`), desconectándolas de la sección donde se explican.
5. **Figuras excesivamente esquemáticas y con textos obsoletos (`docs/figuras/v2/`):**
   - Los diagramas (`arquitectura.png`, `alimentacion.png`, `seguridad.png`, `protocolo_flujo.png`, `camara_modos.png`, `navegacion.png`, `esquive.png`, `vision_tuberia.png`, `app_mapa.png`, `desarrollo.png`) son cajas genéricas de 4 bloques que omiten direcciones IP (`192.168.4.1`, `192.168.4.2`), puertos (`80`, `81`, `4210`), pines (`GPIO 21/22`, `27/26/25/33`), tamaños de paquete (`1,244 B`), umbrales (`120 mm`, `250 ms`, `1000 ms`, `150 mm`) y muestran rótulos falsos hoy (`"J15: NO MEDIDO en el carro"`, `"Avanzar por tiempo"` sin `RUMBO`).
6. **Falta de tablas técnicas y ejemplos numéricos en los capítulos 1 a 15:**
   - En las primeras 48 páginas no hay **ninguna** tabla; solo existen 2 tablas pequeñas en la página 49 (Capítulo 16). Faltan tablas fundamentales: especificaciones de hardware, mapa de pines GPIO, perfiles PWM y constantes de control, modos y benchmark de cámara, catálogo de comandos del protocolo (incluyendo J16), corridas físicas B6, simulación Monte Carlo de 1,000 corridas y guía rápida de diagnóstico de fallas.
7. **Relleno repetitivo artificial en el Capítulo 17 (`page_52.png` a `page_67.png`, 16 páginas):**
   - Para alargar el documento, `17_referencia_archivos.md` repite palabra por palabra las mismas oraciones genéricas:
     - *"Para comprender su comportamiento completo, revise sus llamadas a las sesiones, vistas y módulos que utiliza; la pantalla por sí sola no verifica la respuesta del carro."* aparece **45 veces seguidas** (páginas 57 a 61).
     - *"Declara la interfaz, las estructuras y los límites del módulo... Es el primer lugar para comprobar qué entradas y salidas utiliza quien lo llama."* aparece **25 veces** (páginas 52 a 56).
     - *"Sus resultados corresponden a los datos y condiciones de la prueba de software, no a una medición física nueva."* aparece **21 veces** (páginas 64 a 65).
   - Además, omite los archivos nuevos de J16 (`firmware_carro/rumbo.c`, `firmware_carro/rumbo.h`, `nucleo_c/pruebas_precision_j16.c`, `pruebas_firmware/pruebas_rumbo.c`).
   - En la **Versión final 1.1**, esta correspondencia se integra de forma concisa, completa y útil en el **Anexo 16.3**, eliminando el relleno repetitivo y dejando el manual dentro del rango objetivo de **40 a 60 páginas** con contenido sustantivo en cada página.

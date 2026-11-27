# 1. Introducción y cómo leer el manual

Este capítulo presenta el propósito del sistema robótico móvil **LIDIA** (*Carro FSI*), define el alcance de la **versión 1.2.2** (autónomo v3 con `FW:b396390` y las correcciones físicas del 26-nov-2026; validación física parcial y 1 fallo conocido en `pruebas_sim_ruta`) y de la **versión 1.1** (cierre J16 del 10-nov-2026) y establece las convenciones de lectura que rigen todo el documento. Su objetivo es que cualquier integrante del equipo, docente evaluador o desarrollador futuro pueda distinguir con exactitud qué funciones están comprobadas en el vehículo físico, cuáles provienen de modelos de simulación y cuáles permanecen como mediciones abiertas.

## 1.1 Propósito del proyecto y arquitectura del sistema

El proyecto **LIDIA** desarrolla un vehículo terrestre a escala con tracción omnidireccional de cuatro ruedas, diseñado para desplazarse bajo control manual o semiautónomo sobre superficies interiores planas, detectar obstáculos frente a su trayectoria y ejecutar maniobras de esquive y reincorporación. El sistema se construyó como proyecto integrador de la materia **Fundamentos de los Sistemas de Información (FSI)** en la División de Ingenierías del Campus Irapuato-Salamanca de la Universidad de Guanajuato.

En lugar de concentrar la adquisición de video, la visión por computadora, la planificación geométrica y el control de potencia en un único microcontrolador de recursos limitados, el diseño reparte el trabajo en tres nodos cooperativos conectados mediante una red inalámbrica local (`CarroFSI`, banda de 2.4 GHz):

1. **Placa de control y sensores en el carro (`ESP32 DevKit`, `192.168.4.1`):** actúa como punto de acceso WiFi, gobierna los cuatro motores mediante un puente H doble, integra la velocidad angular de un giroscopio `MPU6050`, supervisa la distancia frontal con un sensor láser de tiempo de vuelo `VL53L0X` y ejecuta los lazos locales de seguridad y mantenimiento de rumbo.
2. **Placa de cámara independiente (`Freenove ESP32-WROVER` con sensor `GC0308`, `192.168.4.2`):** captura cuadros en resolución `HQVGA` (`240×176` píxeles) y los entrega directamente al teléfono en formato comprimido (`MJPEG`), crudo (`RGB565`) o resumido en celdas de color por `UDP`.
3. **Teléfono Android con la aplicación `LIDIA` (`APP:0.4-6d1b213`):** ofrece la interfaz de operación y ejecuta de forma nativa —a través de `JNI` (*Java Native Interface*, puente entre Kotlin y código compilado en C)— el núcleo matemático de navegación, calibración y percepción visual en estándar `C99`.

Esta partición permite que el mismo código en `C99` que toma decisiones dentro del teléfono se compile y verifique en una computadora personal mediante miles de pruebas automatizadas antes de mover el vehículo físico.

## 1.2 Alcance de la Versión final 1.1 (cierre J16), la validada en el carro

La **Versión final 1.1** consolida el trabajo acumulado desde la primera entrega etiquetada (`v1.0`, commit `eb88151`) hasta el cierre de precisión e integración **J16** fusionado en `main`. Los artefactos validados que corresponden a esta edición son:

- **Aplicación Android:** `carro_fsi_0.4-6d1b213.apk` (versión `APP:0.4-6d1b213`, `14,881,402` B, SHA-256 `C5B36FEC74BFEDE9798B4BE7C992D7FEA8369F456DA4D4A10D61A4BBB25ED02F`).
- **Firmware del carro:** `FW:6d1b213-20261110` (`1,034,112` B, SHA-256 `71D671A4E64E0F234015E8685D4E741B87471534E3E50869EE023C5A657B79FC`; solo código fuente y hash, sin publicar el `.bin` por seguridad WiFi).
- **Suites de verificación en PC:** `22` programas y `1,655` casos en `nucleo_c`, `5` programas y `540` casos en `pruebas_firmware`, `54` en el servidor falso y `18` en el verificador de versiones (`0` fallos).

## 1.3 Convención de etiquetas de evidencia

Para evitar ambigüedades entre lo que el código calcula en un modelo matemático y lo que el vehículo ejecuta sobre el piso real, este manual utiliza una clasificación estricta de cuatro niveles de evidencia. Esta convención se define aquí **una sola vez** y se aplica de manera uniforme en todos los capítulos, tablas y pies de figura:

| Etiqueta | Significado técnico en este manual | Criterio de aplicación y alcance en la versión 1.1 |
|---|---|---|
| **VALIDADO EN EL CARRO** *(o VERIFICADO)* | Comportamiento, parámetro o resultado comprobado experimentalmente en el vehículo físico o mediante ejecución determinista de las suites de software del repositorio. | En pruebas físicas de ruta del cierre J16 en el carro, **solo el esquive con reincorporación B6** (6 corridas el 10-nov-2026: 0 toques, 5 completas autónomas con error lateral aproximado de `~0` a `~10 cm` y 1 en `AYUDA` sin causa identificada). En software, las `2,267` aserciones automatizadas en PC. |
| **SIMULADO** | Resultado obtenido en el simulador físico-matemático del núcleo en C (`simulador_navegador.c`) bajo perturbaciones estocásticas con semilla fija. | Las `50,000` corridas Monte Carlo (`1,000` por escenario en 5 configuraciones) de recta de 150 cm, ruta en L, cuadrado de 60 cm y esquives sintéticos. No sustituye pruebas en piso. |
| **NO MEDIDO** | Función implementada en el código y verificada en pruebas lógicas, pero sin corridas físicas registradas en el carro en este cierre. | Rectas físicas con y sin `RUMBO`, ruta en L, cuadrado de 60 cm, corte de WiFi en marcha, freno autónomo aislado por láser, caja ancha de 40×20 cm, calibración en piso de `IMU_ESCALA`, circuito `GPIO 2` y consumo eléctrico en amperios. |
| **PENDIENTE** | Mejora de hardware o software identificada como trabajo futuro que aún no forma parte de la implementación validada. | Incorporación de odometría por codificadores (*encoders*), compensación de deslizamiento lateral de ruedas mecanum y medición instrumentada de corriente. |

### Regla de lectura sobre mediciones físicas

Cada vez que se citan los resultados de la validación física del escenario **B6** en el carro, debe tenerse presente que las desviaciones laterales finales respecto a la línea recta original (`~10 cm`, `~10 cm`, `~5 cm`, `~0 cm` y `~0 cm` en las cinco corridas autónomas completadas) son **medidas aproximadas** observadas en pista al concluir la maniobra. Asimismo, ninguna afirmación de este manual debe interpretarse como que las rectas con control de rumbo, la ruta en L o el cuadrado están validados físicamente en el carro: esos tres escenarios son `SIMULADOS` en computadora y permanecen `NO MEDIDOS` en el vehículo real.

## 1.4 Estructura del documento y convención de referencias al código

El manual está organizado en dieciséis capítulos agrupados en cuatro bloques lógicos:

- **Arquitectura y plataforma física (Capítulos 2 a 5):** describe la topología de tres nodos, el esquema eléctrico con una *powerbank* compartida por el DevKit y la cámara más el portapilas de tracción (`4×AA`), la electrónica del chasis, el firmware de control en la `ESP32 DevKit` y el servidor de visión en la cámara `Freenove`.
- **Comunicación, algoritmos e interfaz (Capítulos 6 a 9):** detalla el protocolo de texto sobre `WebSocket`, el núcleo matemático de navegación en `C99`, la tubería multimodal de percepción visual y el diseño de la aplicación Android.
- **Ingeniería de entrega, operación y evidencia (Capítulos 10 a 13):** documenta el flujo de compilación y verificación criptográfica de versiones, los procedimientos seguros de operación, los resultados experimentales y simulados, y los límites físicos conocidos.
- **Evolución, diagnóstico y consulta rápida (Capítulos 14 a 16):** sintetiza la evolución técnica del proyecto, ofrece una guía sistemática de solución de problemas e incluye los anexos de glosario, comandos, pines, correspondencia completa de archivos del repositorio y lista de tareas abiertas.

Para mantener una lectura fluida, el cuerpo de los capítulos no intercala rutas de carpetas ni números de línea. Al final de cada capítulo del 1 al 15 se incluye una sola línea titulada **Dónde está en el código** con los nombres de los archivos fuente principales, mientras que el **Anexo 16.3** presenta la tabla completa con las rutas exactas dentro del repositorio.

## 1.5 Qué trae la v1.2.2, decisión para la exposición y reparto del equipo

La **v1.2.2** parte de la v1.2.1 (autónomo v3 + revisión de interfaz Android + correcciones físicas del 26-nov en `navegador.c` y `ManualFragment.kt` + resultados físicos agregados del 25–26 nov) y documenta el **fallo conocido** en `pruebas_sim_ruta` (objetos laterales fuera del haz estrecho del láser `VL53L0X`, ~25°, que no disparan esquive solo con visión cuando el ToF está fresco; **PENDIENTE** en el carro, solo **SIMULADO**).

- **Decisión para la exposición (Opción 1 con condiciones):** exponer v3 autónomo con el firmware del carro `FW:b396390` (`SEG=3`, `TOFP=150`, `TOF_BLOQUEO=180`), la cámara `cam-v2` y la app `0.4-b396390` más las dos correcciones del 26-nov. **Plan B:** el mismo firmware v3 con el autónomo apagado (modo manual o semiautónomo). **v1.1 no es respaldo seguro**: su paro a 120 mm deja solo 1.5 a 3.5 cm de margen y puede golpear el sensor.
- **Condiciones operativas obligatorias:** pilas AA alcalinas nuevas y un juego de repuesto; arrancar cada ruta con al menos 100 cm de piso libre delante (límite conocido de `aprenderPiso`); encender primero la *powerbank* con el carro quieto 3 s para calibrar el giroscopio y luego conectar las pilas; revisar cables antes de salir.

### Quién explica qué en la exposición

El principio rector es que **todos deben entender todo y cada quien se especializa en lo suyo** (presentación general: todos):

- **Jaime Fernando Guerrero Segoviano:** hardware, conexiones y firmware del carro (subsistemas A y B), navegación autónoma y visión (E y F), simulador y pruebas (H) y proceso de desarrollo (I); opera el carro en la demostración (y lo operó en las pruebas físicas; armó e integró el sistema base del carro y su lógica).
- **Vitto Eduardo Rangel Fortuna:** aplicación Android (subsistema D).
- **Melanie Contreras Solis:** módulo de cámara (subsistema C) y red propia `CarroFSI`.
- **Azul Abril Cadena Campos:** seguridad del sistema (subsistema G).

### Lo que todos deben saber (10 puntos)

1. El sistema tiene tres nodos en red local propia (`CarroFSI`, 2.4 GHz): carro (`ESP32 DevKit`), cámara (`ESP32-WROVER`) y teléfono Android.
2. La cámara comparte la *powerbank* del DevKit; los cuatro motores TT van aparte con 4 pilas AA Energizer a través del driver `TC1508A` (`INT1` a `INT4`).
3. El carro no tiene *encoders*: estima distancia por tiempo (`ms_por_cm`) y mide giros y rumbo con el giroscopio `MPU6050`.
4. El láser `VL53L0X` (a 3.5 cm del suelo, sobresale 3.5 cm al frente) frena en el propio carro a 150 mm (`FW:b396390`, `SEG=3`) y bloquea arranque bajo 180 mm.
5. El núcleo en `C99` corre igual en PC (pruebas y simulador) y en el teléfono (vía JNI), devolviendo un comando cada 50 ms.
6. La visión combina color del piso (`obstaculos.c`) y `EfficientDet-Lite0`; con las correcciones del 26-nov, cuando el ToF está fresco manda el láser real en el centro.
7. Si no puede esquivar con seguridad, el navegador pasa a `AYUDA`, detiene el carro y cede la cruceta al operador.
8. El watchdog de comandos corta motores a los 1,000 ms sin latido (250 ms) y el de hardware reinicia a los 5,000 ms.
9. Exponemos v3 con `FW:b396390` y las correcciones del 26-nov (Plan B: mismo firmware en manual); v1.1 no es respaldo seguro.
10. Toda cifra lleva etiqueta de evidencia (`VALIDADO EN EL CARRO`, `SIMULADO`, `NO MEDIDO`, `PENDIENTE`); las corridas buenas tras el 26-nov tienen `n=3` en los mismos escenarios donde ocurrió la falla.

### Su especialidad (3 preguntas más probables por integrante)

- **Jaime (A, B, E, F, H, I):** (1) ¿Por qué cambió la autoridad del láser el 26-nov y qué corrige en `PARALELO`? (2) ¿Por qué se usa v3 con `SEG=3` (150/180 mm) y no v1.1 (120 mm) ni v4–v6? (3) ¿Qué está validado físicamente (M1, M3, M5, M6, M8 AUTO, mochila, bajos) y qué sigue pendiente (M2, `RUMBO:ON` Manual, C y D)?
- **Vitto (D · App Android):** (1) ¿Qué decide Kotlin frente al núcleo en C por JNI? (2) ¿Qué vigila la app (`SENS`, video, `WD`, `TOF_PARO`) y qué hace `ManualFragment` con `RUMBO`? (3) ¿Qué cambió en la revisión de interfaz (de 19.2 % a ≤ 3.3 % cuadros lentos en simulado) y qué guarda la caja negra?
- **Melanie (C · Cámara y red `CarroFSI`):** (1) ¿Por qué una placa aparte para la cámara y por qué RGB565 en HQVGA con `frame2jpg`? (2) ¿Cómo funciona la red `CarroFSI` (canal 1, IP `.1` y `.2`, DHCP `.10–.20`) y cómo evita acumular retraso (~320 ms)? (3) ¿Qué hace el vigilante de `recuperacion.c` cuando cae WiFi, servidor o captura?
- **Azul (G · Seguridad):** (1) ¿En qué orden actúan las capas de paro desde el láser (decenas de ms) hasta el portapilas? (2) ¿Por qué el freno láser solo actúa en AUTO y cómo funciona `TOF_BLOQUEO=180`? (3) ¿Qué pasa si se cae el WiFi, se congela la app o se toca «Continuar» tras `AYUDA` sin mover el carro?

Esta edición se puede leer de dos maneras:

- **Para exponer:** el mapa de una página (página {{pag:mapa}}), el glosario general (página {{pag:glosario}}), el índice «si te preguntan X, ve a la página Y» (página {{pag:indice}}) y los nueve subsistemas A a I (el A empieza en la página {{pag:A}} y el I en la {{pag:I}}). Cada subsistema repite siempre siete secciones: qué es, cómo funciona, cómo lo hicimos, cómo lo probamos, límites, términos clave y preguntas probables del profesor.
- **Para consultar a fondo:** los capítulos 2 a 16 y la bibliografía, con el detalle técnico.

La guía para la exposición está en `docs/guia_exposicion.md` y el registro de temas abiertos en `docs/pendientes_confirmar.md`.

Dónde está en el código: `README.md`, `entrega.md`, `estado_20261110.md`, `limites_conocidos.md`.

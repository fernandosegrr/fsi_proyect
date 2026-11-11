# 1. Introducción y cómo leer el manual

Este capítulo presenta el propósito del sistema robótico móvil **LIDIA** (*Carro FSI*), define el alcance funcional de su **Versión final 1.1** y establece las convenciones de lectura que rigen todo el documento. Su objetivo es que cualquier integrante del equipo, docente evaluador o desarrollador futuro pueda distinguir con exactitud qué funciones están comprobadas en el vehículo físico, cuáles provienen de modelos de simulación y cuáles permanecen como mediciones abiertas.

## 1.1 Propósito del proyecto y arquitectura del sistema

El proyecto **LIDIA** desarrolla un vehículo terrestre a escala con tracción omnidireccional de cuatro ruedas, diseñado para desplazarse bajo control manual o semiautónomo sobre superficies interiores planas, detectar obstáculos frente a su trayectoria y ejecutar maniobras de esquive y reincorporación. El sistema se construyó como proyecto integrador de la materia **Fundamentos de los Sistemas de Información (FSI)** en la División de Ingenierías del Campus Irapuato-Salamanca de la Universidad de Guanajuato.

En lugar de concentrar la adquisición de video, la visión por computadora, la planificación geométrica y el control de potencia en un único microcontrolador de recursos limitados, el diseño reparte el trabajo en tres nodos cooperativos conectados mediante una red inalámbrica local (`CarroFSI`, banda de 2.4 GHz):

1. **Placa de control y sensores en el carro (`ESP32 DevKit`, `192.168.4.1`):** actúa como punto de acceso WiFi, gobierna los cuatro motores mediante un puente H doble, integra la velocidad angular de un giroscopio `MPU6050`, supervisa la distancia frontal con un sensor láser de tiempo de vuelo `VL53L0X` y ejecuta los lazos locales de seguridad y mantenimiento de rumbo.
2. **Placa de cámara independiente (`Freenove ESP32-WROVER` con sensor `GC0308`, `192.168.4.2`):** captura cuadros en resolución `HQVGA` (`240×176` píxeles) y los entrega directamente al teléfono en formato comprimido (`MJPEG`), crudo (`RGB565`) o resumido en celdas de color por `UDP`.
3. **Teléfono Android con la aplicación `LIDIA` (`APP:0.4-6d1b213`):** ofrece la interfaz de operación y ejecuta de forma nativa —a través de `JNI` (*Java Native Interface*, puente entre Kotlin y código compilado en C)— el núcleo matemático de navegación, calibración y percepción visual en estándar `C99`.

Esta partición permite que el mismo código en `C99` que toma decisiones dentro del teléfono se compile y verifique en una computadora personal mediante miles de pruebas automatizadas antes de mover el vehículo físico.

## 1.2 Alcance de la Versión final 1.1 (cierre J16)

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

- **Arquitectura y plataforma física (Capítulos 2 a 5):** describe la topología de tres nodos, el esquema eléctrico con dos baterías USB independientes más el portapilas de tracción, la electrónica del chasis, el firmware de control en la `ESP32 DevKit` y el servidor de visión en la cámara `Freenove`.
- **Comunicación, algoritmos e interfaz (Capítulos 6 a 9):** detalla el protocolo de texto sobre `WebSocket`, el núcleo matemático de navegación en `C99`, la tubería multimodal de percepción visual y el diseño de la aplicación Android.
- **Ingeniería de entrega, operación y evidencia (Capítulos 10 a 13):** documenta el flujo de compilación y verificación criptográfica de versiones, los procedimientos seguros de operación, los resultados experimentales y simulados, y los límites físicos conocidos.
- **Evolución, diagnóstico y consulta rápida (Capítulos 14 a 16):** sintetiza la evolución técnica del proyecto, ofrece una guía sistemática de solución de problemas e incluye los anexos de glosario, comandos, pines, correspondencia completa de archivos del repositorio y lista de tareas abiertas.

Para mantener una lectura fluida, el cuerpo de los capítulos no intercala rutas de carpetas ni números de línea. Al final de cada capítulo del 1 al 15 se incluye una sola línea titulada **Dónde está en el código** con los nombres de los archivos fuente principales, mientras que el **Anexo 16.3** presenta la tabla completa con las rutas exactas dentro del repositorio.

Dónde está en el código: `README.md`, `entrega.md`, `estado_20261110.md`, `limites_conocidos.md`.

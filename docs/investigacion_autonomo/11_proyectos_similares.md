# 11. Proyectos similares: ideas de diseño

Investigación técnica para el carro FSI (rama `feat/autonomo-v3`). Fecha de consulta de todas las fuentes externas: **2026-11-16**.
**Alcance:** solo ideas de diseño, parafraseadas. No se copió código ni texto largo de ningún proyecto. Las patentes se usan únicamente como fuente de ideas (su vigencia no se verificó). La licencia de cada proyecto (Duckietown, Donkey Car, ROS/Nav2) no se verificó y no se reutilizó nada de su código.
Convenciones: **[n]** remite a la sección 5. Confianza: **alta** = documentación oficial, manual oficial o artículo revisado; **media** = informe técnico no revisado, tutorial o página de un proyecto; **baja** = inferencia mía (**INFERENCIA**) o fuente poco fiable. **HECHO (proyecto)** = leído en este repositorio. Lo no medido en el carro es **NO MEDIDO** o **SIMULADO**.

---

## 1. Cómo funciona (qué hace cada proyecto)

### 1.1 Duckietown (conducción por carril con cámara; Duckiebots)
- **Percepción de obstáculos sin aprendizaje.** El proyecto de Nubert et al. (ETH, 2018) detecta obstáculos con una sola cámara RGB frontal en una Raspberry Pi: segmenta por color en HSV, proyecta con *inverse perspective mapping* al plano del piso y distingue patos amarillos y conos naranjas por propiedades de la forma (valores propios del tensor de inercia, invariantes a la rotación). Eligieron no usar aprendizaje por el límite de cómputo [1].
- **Consenso temporal.** Si la figura segmentada es pequeña o cambia de tamaño más de 50 % entre cuadros consecutivos, se exige haberla seguido al menos 3 cuadros seguidos antes de llamarla obstáculo [1].
- **Reacción.** El obstáculo se pasa al marco del robot y se compara con un cuadro rectangular delante del robot (el carril); en la versión evaluada **para cualquier obstáculo dentro del cuadro se detiene** (no genera trayectorias de esquive), y esto vuelve estable el comportamiento [1].
- **ToF más cámara** (documentación oficial del Duckiebot): en la página de "Lane following with obstacles" el sensor ToF va en el parachoques frontal y un umbral de distancia en un archivo de configuración dispara el evento de obstáculo, que pasa el estado de seguir carril a PARAR; al desaparecer el obstáculo vuelve a seguir carril. **No pude abrir esa página** (404 en las dos rutas que probé); lo anterior viene de un resultado de búsqueda y queda como **NO CONSULTADO** (confianza baja) [16].
- **Fusión de un trabajo de curso**: usar la distancia menor cuando ToF y cámara dan valor (conservador); el ToF perdía lecturas en las mismas condiciones donde la cámara era más útil [3] (confianza baja).
- **Esquive dinámico** (ETH, 2024): detecta Duckiebots con LED y patos con máscara amarilla y modifica la tubería de carril para planificar rodeos; reportan falsos positivos por luces y objetos brillantes, líneas amarillas confundidas con patos, inestabilidad al adelantar y que cambiar la velocidad al adelantar desestabiliza el seguimiento de carril [2].

### 1.2 Donkey Car (coche RC con Raspberry Pi y cámara)
Plataforma de código abierto en Python para coches RC; cada plantilla es una tubería de "partes" que corren en orden en cada vuelta del bucle; soporta pilotos de aprendizaje profundo (TensorFlow, TensorFlow Lite, PyTorch), GPS y visión clásica, y guarda imágenes y entradas de dirección y acelerador como datos [4]. La página del lidar dice que sirve para evitar obstáculos o para seguir pistas con paredes, que solo se admiten RPLidar y que **ni la plantilla de aprendizaje profundo ni la de seguir trayectoria usan aún datos del lidar** (tema abierto) [4]. Lección: un sensor montado no es un sensor fusionado.

### 1.3 TurtleBot3 y ROS
- **Sensado:** LDS-02 de 360° (en el modelo actual), IMU de tres ejes y, en el Waffle Pi, cámara; la página de características no da alcance ni frecuencia [5].
- **Ejemplo de evitación básica:** un nodo lee `/scan`, y si hay algo a menos de 0.5 m en los 180° frontales (de -90° a +90°) detiene el robot; con camino libre avanza a 0.2 m/s; **no rodea, solo para y espera** [5].
- **Navegación (Nav2): recuperación escalonada.** El árbol de comportamiento reintenta toda la tarea hasta 6 veces; replanifica a 1 Hz; cada etapa tiene una recuperación contextual propia con un reintento (si falla planificar, limpia el mapa de costos global; si falla seguir el camino, el local); solo si eso no resuelve entra al subárbol de recuperación del sistema, que recorre en turnos: limpiar mapas, girar en sitio (1.57 rad), esperar 5 s y retroceder 0.30 m a 0.15 m/s, y tras cada acción reintenta la navegación [6]. Una nueva meta sale de la recuperación de inmediato [6].
- **No pude leer** la página de `move_base` en la wiki de ROS (barrera antibots; no la eludí) [17].

### 1.4 Robots aspiradores (iRobot Roomba)
Fuentes: la patente US 6,809,490 (Jones y Mass; presentada 12-jun-2002, concedida 26-oct-2004) y el manual del *Open Interface* de Roomba/Create 2 [7][8]. El artículo de Jones (2006, *Robots at the tipping point*) **NO lo pude abrir** [9].
- **Sensores de contacto y proximidad (patente):** dos parachoques frontales (el ángulo del contacto se estima por la diferencia de tiempo entre uno y otro), cuatro sensores infrarrojos de precipicio, un sensor infrarrojo de pared en el lado dominante, un sensor de rueda levantada, y encoders de rueda (la patente critica su error por deslizamiento) más monitoreo del atasco del cepillo para inferir que está atorado [7]. El manual de Create 2 confirma cuatro señales de precipicio (izquierda, frente-izquierda, frente-derecha, derecha) con valor digital y analógico, bumpers, ruedas levantadas y seis "light bumper" de proximidad [8].
- **Modos de cobertura (patente):** *espiral* (para cobertura puntual; sale por distancia máxima o al primer contacto), *seguir pared* (mantiene distancia con el sensor de pared durante una distancia aleatoria entre un mínimo y un máximo), *rebote* (avanza recto hasta el contacto y elige un rumbo nuevo al azar) y *recto* como comportamiento por omisión; el modo preferido alterna rebote y pared, y si la distancia media entre contactos (promedio móvil ponderado 3/4 y 1/4) sube, vuelve a la espiral [7].
- **Escapes (patente):** *giro* aleatorio (45-90° con "poco pánico", 90-270° con "mucho pánico"), *borde* (seguir el borde de un obstáculo con el parachoques para salir de espacios cerrados; termina tras 60° sin contacto o 170° acumulados), *rueda levantada* (retrocede un instante para evitar falsos positivos y reanuda si se corrige en 2 s) y *lento* [7].
- **Detección de atasco:** un registro de "tasa" sube mientras el cepillo se atora y baja si no; el escape se dispara cuando pasa de un umbral (~1 s de atasco continuo) y la pendiente es positiva; en atasco grave el cepillo se apaga, un contador de "nivel" sube, corren comportamientos de pánico a 1, 4 y 7 s y reinicia a los 13 s; el nivel decae cada segundo y si llega al máximo el robot se apaga [7].
- **Arbitraje:** comportamientos paralelos por capas con prioridades; los escapes ganan a la cobertura y el precipicio tiene prioridad inmediata [7]. En el *Open Interface*, en modo **Seguro** el robot se detiene y pasa a pasivo si detecta precipicio avanzando o rueda levantada; en modo **Completo** esas protecciones se apagan y la responsabilidad es del programa anfitrión [8].

### 1.5 Pololu Zumo y micromouse
- **Zumo 32U4:** cinco sensores de reflectancia hacia abajo (para detectar el borde del dohyo; funcionan como el QTR-1RC, midiendo el tiempo de decaimiento) y tres sensores de proximidad (izquierda, frente, derecha) que solo responden a infrarrojo modulado a 38 kHz y usan LED de la placa; la luz de los sensores de línea puede dar lecturas falsas de proximidad, por lo que la biblioteca apaga esos emisores antes de leer la proximidad [10].
- **Micromouse:** el concurso exige resolver un laberinto de 16x16 celdas de 18 cm, con ratón de hasta 25 cm, hasta cinco corridas; los algoritmos habituales son *flood-fill*, Dijkstra y A*; los ratones pueden pasar de 3 m/s [11]. En sensores, un artículo de la comunidad explica el truco de **leer cada sensor infrarrojo dos veces, con el emisor apagado y encendido, y restar** para quitar la luz ambiente, porque la luz fuerte sube el nivel base del fotodetector y puede dejarlo sin margen [12].

### 1.6 Evitación con cámara monocular de bajo costo (artículos)
- **Ulrich y Nourbakhsh (AAAI 2000).** Clasifican cada píxel como suelo u obstáculo por su apariencia de color: convierten a HSI, histograman una zona trapezoidal de referencia delante del robot (matiz e intensidad) y marcan como obstáculo el píxel cuyo histograma de matiz o de intensidad queda por debajo de un umbral. Suponen que (1) los obstáculos difieren del suelo en apariencia, (2) el suelo es relativamente plano y (3) no hay obstáculos colgantes. Para poder usar una zona de referencia profunda suponen que **el suelo que el robot ya recorrió estaba libre**: guardan una cola de las últimas diez zonas de referencia y las combinan con OR; esa suposición solo se mantiene si el robot no giró mucho en ese tramo. Modos: regular (histogramas aprendidos al conducir el robot), adaptativo (sigue aprendiendo el piso) y asistido [13]. Es, en esencia, el diseño del detector de color del carro.
- **Michels, Saxena y Ng (ICML 2005).** Coche RC a alta velocidad en terreno exterior sin estructura: aprendizaje supervisado estima la profundidad de obstáculos a partir de imágenes monoculares sueltas, con imágenes reales etiquetadas con distancias o gráficos sintéticos; después un aprendizaje por refuerzo con búsqueda de políticas en un simulador que dibuja escenas sintéticas aprende a elegir la dirección [14]. La página del proyecto dice que el coche condujo solo varios minutos por un bosque con maleza antes de chocar, y ofrece datos de entrenamiento emparejados con láser [14]. El artículo en sí **NO lo pude leer** (el servidor del PDF no respondió).

---

## 2. Parámetros y límites relevantes (con cifras y fuente)

| Dato | Valor | Fuente |
|---|---|---|
| Duckietown, consenso de cuadros | 3 seguidos si la figura es pequeña o cambia > 50 % de tamaño | [1] |
| Duckietown, detección en pruebas propias | ~97 % de patos y ~96 % de conos por cuadro; falsos positivos < 3 %; en estático < 1 % de falsos positivos; error > 5 % al decidir si estaba dentro o fuera del carril | [1] |
| Duckietown, límite de detección cercana | los patos más pequeños a menos de 4 cm de la cámara no se detectaban en estático; por eso paran antes de 4 cm | [1] |
| Duckietown, ritmo y costo en Raspberry Pi | una primera segmentación propia dejó el módulo en ~0.5 Hz (200 % de CPU); con una función de etiquetado eficiente llegó a hasta 3 Hz (~70 % de CPU); con la corrección de color continua (80 % de una CPU cada 10 s, más 74 % de otra para transformar la imagen a 4 Hz) el módulo de seguridad bajó a ~1.5 Hz "con mal comportamiento"; la solución fue calcular la corrección de color **una vez** y fijarla durante todo el recorrido (~3 Hz y hasta ~0.3 s de retraso adicional) | [1] |
| TurtleBot3, ejemplo de obstáculos | zona de parada 0.5 m, sector ±90°, velocidad 0.2 m/s | [5] |
| Nav2, recuperación | 6 reintentos de la tarea; 1 reintento contextual por etapa; girar 1.57 rad; esperar 5 s; retroceder 0.30 m a 0.15 m/s; replanificar a 1 Hz | [6] |
| Roomba, escapes | giro 45-90° (poco pánico) o 90-270° (mucho); borde: 60° sin contacto o 170° acumulados; rueda levantada: reanuda si se corrige en 2 s; avance recto ~0.306 m/s | [7] |
| Roomba, atasco | umbral de tasa ~1 s de atasco continuo; pánico a 1, 4 y 7 s; reinicio a 13 s; apagado si el nivel llega al máximo | [7] |
| Roomba, rebote | rumbo nuevo aleatorio; modo de 6 a 13 rebotes | [7] |
| Roomba OI | cuatro señales de precipicio, seis "light bumper", bumpers y ruedas levantadas; Seguro detiene ante precipicio y rueda levantada, Completo no | [8] |
| Zumo 32U4, proximidad | alcance máximo ~30 a 40 cm contra la hoja de acero de otro Zumo; sin distancia mínima; **zona muerta** entre las zonas frontal y lateral (el objeto puede desaparecer un momento al girar hacia él); LED azul de 20° para más alcance o transparente de 50° para más cobertura | [10] |
| Zumo, magnetómetro | corrientes de motor y metales cerca lo distorsionan; la orientación absoluta es muy difícil | [10] |
| Micromouse, laberinto | 16x16 celdas de 18 cm; ratón ≤ 25 cm | [11] |
| Micromouse, luz ambiente | en pruebas con un reflector de tungsteno a 100 mm, el TSL262 fue útil hasta ~3 500 lux y "completamente inutilizable a 4 500 lux"; el BPW85C y el SFH309FA se mantuvieron consistentes hasta 6 500 lux (el autor dice que fue un experimento tosco) | [12] |
| Ulrich y Nourbakhsh | imagen de 320x260 en ~200 ms en un Pentium II a 333 MHz; con solo la mitad inferior de la imagen alcanzarían 10 Hz con 2 m de alcance de mirada; cola de las 10 últimas referencias | [13] |
| Alcance del VL53L0X al aire libre (5 klx) | blanco típico 80 cm, gris típico 50 cm (contra 200 cm+ y 80 cm en interior) | [16] (Tabla 12) |

### Valor usado en el carro vs. lo que dice la fuente

| Concepto | Carro (HECHO salvo indicación) | Fuente | Comentario |
|---|---|---|---|
| Velocidad de crucero | 19.3 cm/s (perfil AUTO 210), medida en `docs/calibracion.md` | TurtleBot3 0.2 m/s [5]; Roomba ~0.306 m/s [7] | Misma escala |
| Distancia de reacción | láser fresco ≤ 400 mm confirma el centro; firmware para a 120 mm | TurtleBot3 0.5 m [5] | Comparable |
| Cuadros de consenso | 2 seguidos (1 en el centro con cercanía 3) | Duckietown 3 si la figura es chica o cambia mucho [1] | El carro es más permisivo; costo: SIMULADO 4-14 falsos por 1000 → 0 |
| Distancia mínima de detección vs. distancia de frenado | termo visible desde ~19 cm; frenado total ~8.5 cm a 210 (`docs/latencia.md`) | Duckietown: no detecta a menos de 4 cm y frena antes [1] | Margen de ~10 cm, **NO MEDIDO** de punta a punta |
| Recuperación | cambio de lado, paso extra, deshacer en reversa, retroceso con evidencia, y después AYUDA; 6 esquivadas por obstáculo y 18 por ruta | Nav2: 6 reintentos de la tarea, contextual primero, luego round-robin incluyendo **esperar** [6] | El carro no tiene "esperar y volver a mirar" |
| Detección de contacto | ninguna (no hay bumper, encoders ni corriente de motor; el contacto solo existe en el banco) | Roomba: bumpers, precipicio, rueda levantada, atasco del cepillo [7][8] | Brecha de diseño |
| Piso aprendido | «Aprender piso» y piso fijo durante la ruta; aprendizaje continuo (V2) quedó como diseño por riesgo de aprender un obstáculo | Ulrich: modo adaptativo con cola de referencias del suelo ya recorrido y sin giros grandes [13] | Hay una forma documentada de hacerlo con salvaguarda |
| Autoridad del láser | confirma, no veta | Duckietown: tomar la menor distancia entre ToF y cámara [3] (baja) | Equivalente a un OR conservador para frenar |
| Ambiente | luz de día, manchas de sol → falsos positivos de color y red (SIMULADO) | Ulrich: el modo regular da falsos positivos si la luz difiere del entrenamiento [13]; Duckietown: luz directa sobre líneas reflectantes [1] | Mismo tipo de falla en todos los sistemas basados en apariencia |

---

## 3. Errores típicos y modos de falla documentados

1. **Desenfoque de movimiento** produce falsos obstáculos: dos segmentos de línea del fondo se "mezclaron" en dos cuadros seguidos y se etiquetaron como obstáculo; además las oscilaciones del control por lag (carga de cómputo) aumentan el desenfoque y acortan el tiempo de reacción [1]. En el carro el pivote es rápido para ~12 cuadros por segundo con ~320 ms de retraso (`docs/progreso.md`).
2. **Obstáculo en una curva o esquina entra tarde en la zona de vigilancia** (el cuadro de seguridad es rectangular y no sigue la curva) y puede haber choque porque a muy poca distancia no se detecta [1]. Paralelo en el carro: SIMULADO, "caja antes de una esquina".
3. **Luz directa sobre superficies reflectantes** hace que las líneas amarillas parezcan blancas y falla la clasificación [1]; **luces y objetos brillantes** dan falsos positivos en el detector por LED [2]; los pisos brillantes, los bordes de alfombra y las sombras son falsos positivos clásicos de la detección por apariencia, y los límites débiles entre piso y objeto dan falsos negativos [13].
4. **La compensación de iluminación fue el mayor reto** de un detector por color: los autores de Duckietown escribieron que su enfoque "siempre tendría problemas" sin compensar el cambio de luz, usaron un módulo de corrección de color y, como la versión continua costaba demasiado cómputo, la fijaron una vez por recorrido [1]. Paralelo directo con «Aprender piso» y `piso_fijo` del carro (el proyecto no detecta cambios de iluminación a mitad de ruta, `docs/limites_conocidos.md`).
5. **Objetos del color del suelo** no se detectan (en su Figura 3e, una pata amarilla de color demasiado parecido al del piso) [13].
6. **Obstáculos colgantes** (mesas) violan la hipótesis del suelo plano sin voladizos y la distancia se **sobreestima**, lo que puede causar choques aunque los píxeles se clasifiquen bien; los autores proponen combinar con un sensor de rango, por ejemplo uno o dos ultrasonidos de ángulo ancho, y llaman "complementaria" la combinación de apariencia más rango [13]. Paralelo directo en el carro: asientos por encima del haz del láser (SIMULADO).
7. **Sensor de proximidad con zona muerta** entre sus campos frontal y lateral: el objeto desaparece al girar [10]. Paralelo: el "punto ciego" del carro a ~19 cm de la lente y > 28° del eje (`docs/autonomo_v2_diseno.md` §2.1, SUPUESTO).
8. **Interferencia entre sensores propios**: los emisores de los sensores de línea del Zumo dan lecturas falsas de proximidad [10].
9. **Luz ambiente que satura sensores infrarrojos** (micromouse) [12]; en el VL53L0X baja el alcance al aire libre (80 cm blanco, 50 cm gris, típicos) [16].
10. **Un sensor extra no se usa solo.** El lidar del Donkey Car no está integrado en las plantillas de conducción [4]; el ToF del Duckiebot de un curso perdía lecturas justo donde la cámara servía [3].
11. **Falla a largo plazo aunque funcione minutos**: el coche de Michels et al. condujo varios minutos por un bosque y terminó chocando [14].
12. **Evitar sin planificar es estable pero no avanza**: el TurtleBot3 solo para y espera; Duckietown para ante cualquier obstáculo en el carril porque así el comportamiento es estable [1][5].
13. **Seguridad desactivable**: en Roomba el modo Completo apaga el precipicio y la rueda levantada [8]. Paralelo: en el carro los frenos del firmware (TOF_PARO, watchdog, `SEG=2`) no deben depender de la app.

---

## 4. Buenas prácticas reconocidas (extraídas de las fuentes)

- **Capas con prioridad, la de seguridad arriba y siempre activa**: en Roomba los escapes ganan a la cobertura y el precipicio tiene precedencia inmediata [7]; en el *Open Interface* los reflejos de seguridad corren aunque el programa anfitrión falle (en modo Seguro) [8].
- **Combinar un sensor de apariencia con uno de rango** porque sus fallas son complementarias [13] y usar la regla conservadora (la menor distancia) cuando ambos dan valor [3].
- **Recuperación escalonada: primero lo local y barato, después lo drástico, y esperar es una opción**: reintento contextual, limpiar estado obsoleto, girar, esperar, retroceder [6]; escape aleatorio con "nivel de pánico" que crece con la persistencia del atasco y decae con el tiempo [7].
- **Contadores de atasco con decaimiento** (tasa, pendiente, nivel) en lugar de un solo umbral duro [7].
- **Exigir persistencia a las detecciones débiles** (3 cuadros si son pequeñas o inestables) [1].
- **Mantener la distancia de parada mayor que la distancia mínima de detección** [1].
- **Aprender el piso solo de lo ya recorrido y sin giros grandes**, con una cola de referencias recientes [13].
- **Documentar las zonas muertas y los supuestos** (suelo plano, sin voladizos, contraste con el piso) [13][10].
- **Compensar la luz ambiente en sensores activos** midiendo con y sin emisor [12].
- **Combinar sensores para entrenar o calibrar a otro** (el láser para supervisar la estimación de distancia de la cámara) [14] (la página del proyecto ofrece datos emparejados con láser).

---

## 5. Fuentes

(Fecha de consulta de todas: 2026-11-16.)

1. Julian Nubert, Niklas Funk, Fabio Meier, Fabrice Oehler. *Developing a Purely Visual Based Obstacle Detection using Inverse Perspective Mapping.* arXiv:1809.01268 (4-sep-2018). https://arxiv.org/abs/1809.01268 y PDF https://arxiv.org/pdf/1809.01268v1.pdf (leído completo, extraído en local). **Confianza: media-alta** (informe técnico de curso, con evaluación propia; no revisado por pares).
2. Nikolaj Witting, Fidel Esquivel Estay, Johannes Lienhart, Paula Wulkop. *Implementing vision based dynamic obstacle avoidance.* Duckietown, 6-jul-2024. https://duckietown.com/dynamic-obstacle-avoidance-in-duckietown/ (leído vía resumen automático; sin resultados cuantitativos). **Confianza: media.**
3. Página personal de estudiantes (el repositorio inicial enlazado sugiere el curso CMPUT 412; autoría sin verificar), "Don't crash! Tailing behaviour - Fourth lab", 2022-2023. https://my-webpage-658.pages.dev/duckietown/lab4 — fusión ToF + cámara con la distancia menor. **Confianza: baja.**
4. Donkey Car. Documentación oficial: https://docs.donkeycar.com/ (descripción general) y https://docs.donkeycar.com/parts/lidar/ (lidar). Leído vía resumen automático. **Confianza: alta** para lo citado (descripciones del propio proyecto).
5. ROBOTIS. *TurtleBot3 e-Manual*: https://emanual.robotis.com/docs/en/platform/turtlebot3/features/ (sensores) y https://emanual.robotis.com/docs/en/platform/turtlebot3/basic_examples/ (ejemplo de detección de obstáculos; solo leí los primeros 100 000 caracteres de la segunda). **Confianza: alta** (documentación del fabricante; el ejemplo está escrito para ROS Kinetic y la página lo advierte).
6. Nav2. *Navigate To Pose With Recovery* (árbol de comportamiento), versión Jazzy: https://docs.nav2.org/jazzy/getting_started/nav2_behavior_trees/trees/nav_to_pose_recovery/ — reintentos, recuperaciones contextuales, subárbol round-robin. **Confianza: alta.** (Las rutas `docs.nav2.org/behavior_trees/...` dieron 404.)
7. Joseph L. Jones, Philip R. Mass (iRobot). *Method and system for multi-mode coverage for an autonomous robot.* US 6,809,490 B2, presentada 12-jun-2002, concedida 26-oct-2004. https://patents.google.com/patent/US6809490 — leí los primeros 100 000 de ~312 000 caracteres (vía resumen automático). Solo como fuente de ideas; no se copia texto. **Confianza: alta** para el contenido de la patente; las cifras (600, 120 ciclos, 6.3 m, etc.) son de realizaciones preferidas, no de un producto concreto.
8. iRobot. *iRobot Create 2 Open Interface (OI) Specification, based on the iRobot Roomba 600* (8-sep-2015). https://anrg.usc.edu/ee579/spring2016/Roomba/iRobot_Roomba_600_Open_Interface_Spec.pdf — extraído a texto en local: modos Seguro y Completo, paquetes de bumpers, ruedas levantadas, precipicio y light bumper. **Confianza: alta** (manual oficial).
9. Joseph L. Jones. "Robots at the tipping point: the road to iRobot Roomba." *IEEE Robotics & Automation Magazine* 13(1):76-78, marzo 2006, doi:10.1109/MRA.2006.1598056. **NO CONSULTADO** (solo su ficha: ResearchGate dio 403).
10. Pololu. *Zumo 32U4 Robot User's Guide*, secciones 3.5 a 3.7. https://www.pololu.com/docs/0J63 (versión completa: https://www.pololu.com/docs/0J63/all; leí los primeros 100 000 de ~134 000 caracteres). **Confianza: alta** (documentación del fabricante).
11. Wikipedia, *Micromouse*. https://en.wikipedia.org/wiki/Micromouse — reglas del concurso, algoritmos, velocidad. La página no trata de sensores. **Confianza: media.**
12. Peter Harrison. "Sensor Alternatives to the TSL262." Micromouse Online, 11-dic-2011. https://micromouseonline.com/?p=1217 — resta con emisor apagado y encendido; pruebas con reflector. **Confianza: media** (artículo de comunidad con mediciones toscas que el autor declara).
13. Iwan Ulrich, Illah Nourbakhsh. "Appearance-Based Obstacle Detection with Monocular Color Vision." *AAAI-00*, pp. 866-871, 2000. https://cdn.aaai.org/AAAI/2000/AAAI00-133.pdf (PDF leído completo, extraído en local). **Confianza: alta** (artículo revisado).
14. Jeff Michels, Ashutosh Saxena, Andrew Y. Ng. "High Speed Obstacle Avoidance using Monocular Vision and Reinforcement Learning." *ICML 2005*, pp. 593-600, doi:10.1145/1102351.1102426. Resumen: https://mlanthology.org/icml/2005/michels2005icml-high ; página del proyecto: https://www.cs.cornell.edu/~asaxena/rccar . Leí solo el resumen y la página del proyecto. **Confianza: media** (el artículo **NO lo leí**).
15. Proyecto (HECHO): `docs/limites_conocidos.md`, `docs/autonomo_v2_diseno.md` §2.1-2.5, `docs/latencia.md`, `docs/progreso.md`, `docs/calibracion.md`, `nucleo_c/navegador.c/.h`, `nucleo_c/fusion.c`, `nucleo_c/mundo_v2.h`. **Confianza: alta** para lo leído.
16. STMicroelectronics, *VL53L0X Datasheet DS11555 Rev 6* (junio 2024), Tabla 12 (copia leída en https://www.makerguides.com/wp-content/uploads/2024/09/vl53l0x-datasheet.pdf; el sitio de ST no respondió). **Confianza: alta.** En esta lista también va el resultado de búsqueda sobre la página oficial de Duckietown de "Lane following with obstacles" (https://docs.duckietown.com/ente/opmanual-duckiebot/demos/supported/lane_following_pedestrians.html), que **no pude abrir** (404), citada en 1.1 con confianza baja.
17. ROS wiki, `move_base`. http://wiki.ros.org/move_base — **NO CONSULTADA** (barrera antibots).

**NO CONSULTADAS (resumen):** el *Duckiebook* sobre ToF y obstáculos (la página oficial dio 404), el artículo de Jones 2006, el artículo completo de Michels et al. 2005, la wiki de ROS `move_base`, el artículo de Brooks 1986 sobre arquitectura por capas (solo vi su ficha en un resultado de búsqueda; el PDF no respondió), la tesis de Tartu sobre Donkey Car con lidar (solo un fragmento de búsqueda), los informes de equipos de micromouse sobre sensores (solo fragmentos de búsqueda), UKMARS y las rutas profundas de la documentación de Donkey Car sobre IMU y límites.

---

## 6. Qué implica para este carro

El carro (cámara HQVGA con ~320 ms de retraso, láser VL53L0X de 25°, IMU, teléfono con detector) comparte con Duckietown y con Ulrich y Nourbakhsh el diseño de **detector por apariencia de color del piso**, con el láser de TurtleBot3/Roomba como **fuente de rango de poca cobertura**. Lo que le falta frente a los aspiradores es **sensado de contacto y de atasco**. Cada recomendación está marcada con su respaldo y cómo verificarla; las que no vienen de una fuente son **INFERENCIA**.

**R1. Cerrar la brecha de detección de contacto/atasco.** Roomba combina bumpers, precipicio, rueda levantada y monitoreo de atasco [7][8]; el carro no tiene ninguno (`docs/limites_conocidos.md`: "No hay sensor trasero", sin encoders; el contacto solo existe en el banco). Opciones de menor costo (**INFERENCIA**): (a) un detector de "no hay progreso" con el MPU6050 que ya está (el proyecto tiene la idea B del acelerómetro como diseño): si se manda avanzar y el giroscopio/acelerómetro indican golpe o inmovilidad, se detiene y registra; (b) un microinterruptor como parachoques (hardware nuevo, no estimé costo). Verificable: en banco, empujar el carro con la mano y medir la latencia de detección y los falsos con baches del piso.

**R2. Contador de "pánico" con decaimiento para la recuperación**, al estilo del atasco de Roomba [7], en vez de solo presupuestos fijos (6 esquivadas por obstáculo y 18 por ruta). Idea: un nivel que sube con cada intento fallido en la misma zona, decae con la distancia recorrida y decide qué recuperación toca (esperar, reversa corta, giro mayor, AYUDA). **INFERENCIA** de que reduce las ayudas innecesarias del banquito y la silla (SIMULADO: 58 y 64 ayudas por 1000). Verificable: en `pruebas_autonomo_v2.c`, comparar ayudas y contactos con presupuesto fijo contra nivel con decaimiento en los mismos 1000 casos.

**R3. Agregar "esperar y volver a mirar" antes de AYUDA** (el `Wait` de 5 s de Nav2 dentro de la recuperación en turnos [6]), para objetos que se mueven (una persona, una puerta). Hoy el carro, tras agotar las maniobras, pide ayuda (causas 1 a 3). Verificable: escenario "objeto que aparece y desaparece" del banco; la métrica es la fracción de AYUDA evitada sin aumentar los contactos.

**R4. Descontar los cuadros tomados durante un pivote o un cambio de rumbo rápido** como evidencia para el consenso: Duckietown atribuye la mayoría de sus errores dinámicos al desenfoque de movimiento y a las oscilaciones del control [1]. El proyecto ya espera "dos cuadros estables" en el regreso; falta una regla general. Verificable: con las cajas negras (campos `ang` y `fv`), medir si las coincidencias color + red aisladas ocurren con velocidad angular alta (**NO MEDIDO**; hoy hay solo 7 cajas negras).

**R5. Aplicar la regla "distancia de parada > distancia mínima de detección"** [1]: con el termo visible desde ~19 cm y un frenado total de ~8.5 cm a 210 (`docs/latencia.md`), el margen es ~10 cm y depende de la latencia medida una sola vez (320 ms). Verificable: repetir la prueba del termo con la caja negra completa y calcular, por pasada, la distancia de primera detección menos la distancia recorrida hasta parar.

**R6. Si algún día se hace «aprender piso» continuo**, usar el diseño de Ulrich y Nourbakhsh como guía de salvaguardas: aprender solo de lo recorrido, sin giros grandes y con una cola corta de referencias [13], y en el carro además solo mientras el láser confirme libre el frente (**INFERENCIA**, porque un objeto bajo que el láser no ve podría aprenderse como piso; ver `docs/autonomo_v2_diseno.md` §4, V2). Verificable en banco con el escenario "objeto bajo del color del piso".

**R7. Usar el láser para supervisar la relación fila de imagen → distancia de la cámara** (el trabajo de Michels et al. usó datos emparejados con láser [14]; el proyecto lo tiene como V5). Las cajas negras ya guardan `tof` y la franja `f` por fila; verificable ajustando una regresión por tramos y comparando el error contra las bandas 1/2/3 actuales (solo proximidad de imagen).

**R8. Documentar las zonas muertas del carro como lo hace Pololu con la zona entre sensores** [10]: tabla con (a) cono del láser ±89 mm a 400 mm [16], (b) punto ciego de cámara de ~19 cm, (c) sin sensor trasero, (d) láser a 55 mm de altura (SUPUESTO) frente a chasis de 130 mm. Verificable: cada fila lleva "medido o supuesto" y cómo comprobarlo.

**R9. Mantener los reflejos de seguridad fuera de la app** (TOF_PARO a 120 mm, watchdog, `SEG=2`), por el principio de que en Roomba esas protecciones son del robot y solo se desactivan por decisión explícita [8]. Verificable: pruebas de firmware existentes de `TOF_PARO` y comando desconocido.

**R10. Probar el truco de "con y sin" para detectar luz de ventana en el láser** (analogía con la resta ambiente de los sensores de micromouse [12]): el VL53L0X ya compensa la luz internamente [16], pero su alcance cae al aire libre, así que conviene repetir las pasadas con pared (filas 5 a 7 del CSV) con luz fuerte de ventana y comparar el porcentaje de ticks con eco válido contra el del 15-nov (casi continuo hasta ~1.0 m, ver `08_fusion_percepcion.md` §2.3). Verificable con las cajas negras (`tof=mm,valida,edad`).

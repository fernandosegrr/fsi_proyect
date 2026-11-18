# 04 — Motores, driver, cinemática y odometría sin encoders

Investigación del 16-nov-2026 (rama `feat/autonomo-v3`). Fecha de consulta de todas las fuentes externas: **2026-11-16**.

Convenciones. Confianza: **alta** = hoja de datos, manual oficial o artículo revisado; **media** = tutorial confiable, ficha de distribuidor o varias fuentes secundarias coherentes; **baja** = inferencia mía, siempre marcada **INFERENCIA**. Todo lo que no se midió en el carro es **NO MEDIDO** o **SIMULADO**. «NO CONSULTADA» = no pude abrirla y leerla; no cito nada de ella como si la hubiera leído.

Aviso previo sobre el chip. **No pude leer la hoja de datos del TC1508A.** Lo que leí de verdad: (a) la hoja de datos del **TC1508S** (hermano de la misma familia, documento S&CIC1061, 4 páginas, leída como imagen), y (b) dos fichas de distribuidor del TC1508A (LCSC y JLCPCB). Donde uso cifras del TC1508S lo digo; aplicarlas al TC1508A es una **INFERENCIA** (el TC1508S tiene otros límites de tensión: 5,5 V máximo absoluto contra 7,2 V recomendado del A según el listado).

---

## 1. Cómo funciona

### 1.1 El driver TC1508A: dos puentes H completos de MOSFET

- Es un driver de **dos canales** (cada canal es un puente H completo con MOSFET de potencia internos), encapsulado SOP-16, para juguetes. El listado de LCSC describe «adelante, atrás, parada y freno activo» [2] y el de JLCPCB lista «freno activo» [3]. El TC1508S dice lo mismo y añade «consumo en espera y en trabajo ultrabajos» [1].
- **Tabla de verdad (TC1508S, leída [1], pág. 2):**

| INA (IN1) | INB (IN2) | OUTA | OUTB | Modo |
|---|---|---|---|---|
| L | L | Hi-Z | Hi-Z | espera = **rueda libre** |
| H | L | H | L | adelante |
| L | H | L | H | atrás |
| H | H | L | L | **freno** (ambas salidas a masa) |

  Para el TC1508A se supone la misma tabla (**INFERENCIA**: misma familia y mismo listado de funciones; su hoja de datos no se leyó).
- **Cómo lo usa el proyecto.** `poner_motores()` (`firmware_carro/carro.c:108`) escribe PWM en un solo pin de cada motor y deja el otro en 0; un motor avanza con una patita (B en el izquierdo, A en el derecho; `config.h:67-72`). En la fase «apagada» del PWM los dos pines quedan en L/L, es decir **Hi-Z**. Según TI [6] eso es **fast decay** asíncrono: la corriente del motor se descarga por los diodos parásitos (la hoja del TC1508S da 1,7 V máx. de caída del diodo a 400 mA [1]). La alternativa «slow decay» (freno de lado bajo, HH en esta tabla) existe pero el firmware no la usa.
- **STOP y EBRAKE son lo mismo en el código: los cuatro PWM a 0**, o sea **rueda libre, no freno** (`carro_detener()`, `carro.c:727-737`; `docs/perfiles_velocidad.md:86` lo dice; la auditoría ya lo anotaba: `docs/auditoria_autonomo_v2.md:87`). El manual del proyecto (`docs/manual/capitulos/03_hardware.md:12`) describe EBRAKE como HIGH/HIGH (freno activo): **eso no coincide con el código**. Por tanto el «frenado» medido (1,4 cm a 210; 0,9 cm a 255; `docs/calibracion.md:319`) es el de rueda libre más la fricción de las reductoras.
- Frecuencia PWM: `config.h:81` fija **1500 Hz, 8 bits** (`firmware_carro.ino:270` y `firmware_carro/README.md:11` coinciden). El manual (`03_hardware.md:26,37-40`) dice **1000 Hz**. Discrepancia documental a corregir; el código manda.

### 1.2 Motores TT con reductora 1:48

- Motor DC de escobillas con caja plástica 1:48, 3 a 6 V nominales, **sin encoder** [4]. A tensión constante la velocidad en vacío es casi proporcional a la tensión: el vendedor mide 120 rpm a 3 V, 185 a 4,5 V y 250 a 6 V [4], es decir 40 a 42 rpm por voltio (cálculo mío con sus cifras). La velocidad «sigue» al voltaje promedio, o sea tensión de alimentación x ciclo de trabajo [8].
- La corriente es el punto crítico (sección 2): en vacío ~150 mA por motor, **atascado 1,1 A (3 V) a 1,5 A (6 V)** [4].
- Cada canal del driver alimenta **dos motores en paralelo** (`03_hardware.md:15`), así que la corriente por canal es la suma de ambos.

### 1.3 Ruedas mecanum y por qué un driver de dos canales no da desplazamiento lateral

- Rodillos a 45° respecto del plano de la rueda y del eje [9]. Con cuatro ruedas controladas por separado [9]: todas giran igual = avance (las fuerzas laterales se anulan); **lados opuestos en sentidos contrarios = giro en el sitio**; **los dos pares diagonales en sentidos opuestos = desplazamiento lateral**; combinando se obtiene cualquier dirección y rotación [9]. Existen modelos cinemáticos directo e inverso de cuatro ruedas mecanum [13] (solo vi el resumen; las ecuaciones **NO CONSULTADAS**).
- **Derivación propia (INFERENCIA con la regla de [9]).** Tres grados de libertad (vx, vy, ω) exigen al menos tres señales independientes; dos canales dan dos.
  - Cableado del proyecto: canal 1 = {delantera izq., trasera izq.}, canal 2 = {delantera der., trasera der.}. Las dos ruedas del mismo lado reciben la misma señal; como sus rodillos están en espejo, sus fuerzas laterales se anulan: el chasis solo recibe «avance de cada lado». Es cinemáticamente un **diferencial de tracción de 4 ruedas** (vL, vR). Lateral = imposible (pide pares diagonales en sentidos opuestos, y aquí cada par comparte señal). Esto coincide con `03_hardware.md:15`.
  - Si el cableado fuera por diagonales (canal 1 = {DI, TD}, canal 2 = {DD, TI}) saldrían avance y lateral pero **no giro** (el giro pide la rueda DI y la TI en el mismo sentido y en distinto canal). Dos canales nunca dan los tres movimientos.
- **Por qué el proyecto gira «sobre un costado» (pivote).** `calibracion.md:146-148` y `docs/bitacora.md:253` dicen que `LEFT`/`RIGHT` mueven **solo un lado** (el otro queda suelto, PWM 0; `carro.c:165-182`) y que el giro con lados opuestos «ya se probó y el carro no se mueve por falta de par». La causa de esa falta de par **no se diagnosticó** (hipótesis mías, **INFERENCIA**, sin medir: caída de tensión por la corriente conjunta de los cuatro motores, fricción de los rodillos al arrastrar lateralmente, reductoras plásticas). El manual (`03_hardware.md:11`) describe el giro como lados opuestos: **también contradice el código**.
- El resultado medido del pivote derecho a 90° (AUTO): el centro se desplaza 150 mm adelante y 165 mm a la derecha (cuerda de 22,3 cm; con R = 15,8 cm da 2R sen(45°) = 22,3 cm, cuadra) (`calibracion.md:146-160`, J14); equivale a girar sobre un punto a ~157 mm a la derecha del centro (`calibracion.md:347-349`). El pivote izquierdo midió R = 8,7 a 9 cm (`calibracion.md:365`), así que **izquierda y derecha no son simétricas** (nota: `limites_conocidos.md:80` y `autonomo_v2_diseno.md:204` aún dicen «espejo», en contradicción con eso).
- **INFERENCIA:** un pivote a 157 mm del centro cae **fuera** de un chasis de 200 mm de ancho (mitad = 100 mm; el ancho del chasis es SUPUESTO en `auditoria_autonomo_v2.md:105`). Por tanto ese «punto fijo» es un parámetro ajustado que incluye deslizamiento de los rodillos de las ruedas sin tracción, no un punto geométrico sobre las ruedas. Debe tratarse como estadístico (varía con piso y pilas), no como constante.

### 1.4 Odometría: errores sistemáticos y no sistemáticos (Borenstein y Feng)

Fuente principal leída completa: manuscrito del autor, «Measurement and Correction of Systematic Odometry Errors in Mobile Robots» [10] (IEEE Trans. Robotics and Automation, 1996; el manuscrito dice «Vol 12, No 5», el registro de IEEE «No 6, pp. 869-880»; verifíquese el número).

- **Sistemáticos** [10, sec. 2]: ruedas de diámetro distinto, diámetro medio distinto del nominal, desalineación, incertidumbre del ancho de vía efectivo, resolución y muestreo de encoder. **No sistemáticos**: suelo irregular, objetos en el piso, **deslizamiento** (suelo resbaladizo, sobreaceleración, giro rápido con patinaje, fuerzas externas, fuerzas internas, contacto no puntual). En suelo liso interior los sistemáticos pesan más; en suelo rugoso pueden dominar los no sistemáticos [10]. Los errores sistemáticos «se acumulan constantemente» [10].
- «Los errores de orientación son la principal preocupación porque, una vez cometidos, crecen sin límite como errores de posición lateral» [10, sec. 2] (parafraseado). Y errores típicos de odometría vuelven la posición «totalmente equivocada» tras ~10 m de recorrido [10, sec. 1.3, que cita a Gourley y Trivedi 1994 sin que yo la haya leído].
- **UMBmark** [10, sec. 3.2-3.3]: cuadrado de 4 x 4 m recorrido en **ambos sentidos (horario y antihorario)**, parando tras cada lado de 4 m, cuatro giros de 90° en el sitio, a velocidad baja; **5 corridas por sentido**; se compara la posición absoluta final medida (paredes de referencia en L; sonar con ±2 mm) con la calculada por odometría. Se toma el **centro de gravedad** de cada nube de errores y la medida de exactitud sistemática es E_max,syst = máx(|cg_horario|, |cg_antihorario|). Se usan ambos sentidos porque en un solo sentido los dos errores dominantes se pueden **compensar y esconderse** (ejemplo: 87° por giro por ancho de vía mal contra una trayectoria curva; con un solo sentido se «calibra» mal y se obtiene un resultado aparentemente excelente) [10, sec. 3.1].
- Error de **tipo A** (el giro total es menor o mayor en los dos sentidos) viene del ancho de vía (E_b); **tipo B** (menor en un sentido y mayor en el otro) viene del cociente de diámetros (E_d) [10, sec. 4]. Correcciones: E_b = 90°/(90° - alfa) y E_d = (R + b/2)/(R - b/2) con R = (L/2)/sen(beta/2) [10, ecs. 4.20-4.27]; la compensación de las ruedas conserva el diámetro medio [10, sec. 4.2].
- Resultados [10, sec. 5, tabla I]: ocho experimentos con un LabMate (encoders, suelo de concreto liso), E_max,syst de **232 a 423 mm** antes y **12 a 35 mm** después (mejora de **10 a 22 veces** en siete de los ocho; el experimento 7 dio 66 mm tras la primera compensación, 6,4 veces, y 20 mm tras una segunda, 21 veces); dispersión de las corridas ~25 mm; regla práctica: si E_max,syst < 3 x SEM (SEM = sigma/raiz(n)) es poco probable mejorar con otra compensación [10]. Calibrar periódicamente corrige carga y desgaste de las llantas [10, sec. 6].
- Hallazgo conceptual útil para este carro [10, sec. 2.3]: con encoders y giro **en el sitio**, ruedas de distinto diámetro **no** cambian el ángulo girado, solo desplazan el centro de giro. Este carro **no** gira en el sitio.
- **«Where am I?» (Borenstein, Everett, Feng, 1996, 282 pp.): NO CONSULTADA.** El PDF pesa más que el límite de mi herramienta de lectura (10 MB) y los demás espejos devolvieron error; no cito nada de él.
- Errores de odometría en robots omnidireccionales (mecanum/omni): un artículo de calibración revisado nombra diámetros desiguales, desalineación de juntas, **holgura (backlash)**, pulsos de encoder y **deslizamiento de ruedas** como fuentes, y mejora errores de posición en al menos 68 % en un robot de tres ruedas [11] (solo vi el resumen). Un resultado de búsqueda afirma que el error lateral de una plataforma mecanum es mayor que el longitudinal por rotación superpuesta y deslizamiento medio, pero **esa página dio 403 y no la abrí**: no lo uso como dato.

---

## 2. Parámetros y límites relevantes (con fuente)

### 2.1 Driver

| Parámetro | Valor | Fuente | Confianza |
|---|---|---|---|
| Tensión de alimentación recomendada TC1508A | 2,4 a 7,2 V | LCSC y JLCPCB [2][3] | media |
| Corriente continua por canal | 1,8 A | [2][3] (idéntico en TC1508S [1]) | media (A) / alta (S) |
| Corriente pico por canal | 2,5 A | [2][3]; en [1] «Iop» 2,5 A | media / alta (S) |
| Resistencia de encendido | 1,6 ohm (A, listado); TC1508S: típ. 1,0, máx. 1,6 ohm con Io = ±200 mA y VCC = 3 V | [2][3]; [1] | media / alta (S) |
| Caída del diodo de conducción (S) | máx. 1,7 V a 400 mA | [1] | alta (S) |
| Corriente en espera | máx. 10 uA | [1][2] | alta (S) |
| Temperatura de operación / unión | -20 a 85 C / 150 C | [1][2] | alta (S) |
| Disipación máx. / resistencia térmica (S) | 1,73 W / 75 C/W | [1] | alta (S) |
| Máximo absoluto de VCC (S) | **5,5 V** | [1] | alta (S) |
| **Frecuencia PWM admisible** | **NO ESPECIFICADA** en ninguna fuente que pude leer | — | — |
| **Protecciones** (térmica, sobrecorriente, subtensión) | **No aparecen** en la hoja del TC1508S ni en los listados; ausencia de mención no prueba ausencia, pero **no debe contarse con ellas** | [1][2][3] | media |

La hoja del TC1508S advierte (nota de uso 1 y 2, leída): los circuitos y parámetros son para «coches de juguete de control remoto comunes»; la capacidad de corriente continua depende del encapsulado, de VDD, de la dispersión entre chips y de la temperatura ambiente; las cifras son de referencia y hay que dejar margen; el chip es sensible a ESD [1]. Circuito de referencia: 10 uF/16 V y 0,1 uF en la alimentación y 0,1 uF entre las terminales de cada motor [1, pág. 4].

**Cálculo propio de caída de tensión (INFERENCIA, supuestos explícitos).** Supuestos: Ron = 1,0 a 1,6 ohm es la resistencia de toda la trayectoria de corriente por el puente (en la hoja no se aclara); las cuatro pilas AA son alcalinas frescas con 150 a 300 mohm cada una (hoja Energizer EN91 [5]: «Nominal IR 150 to 300 milliohms (fresh)»), es decir **0,6 a 1,2 ohm la serie**; tensión nominal 6,0 V. La química de las pilas del carro no consta en los documentos que leí (si son NiMH las cifras cambian).

| Corriente total del conjunto (A) | Caída en la serie de pilas | Caída en el driver (mitad de la corriente por canal) | Tensión en el motor |
|---|---|---|---|
| 0,6 (marcha ligera) | 0,4 a 0,7 V | 0,3 a 0,5 V | 4,8 a 5,3 V |
| 1,5 | 0,9 a 1,8 V | 0,75 a 1,2 V | 3,0 a 4,4 V |
| 3,0 (arranque con ambos canales) | 1,8 a 3,6 V | 1,5 a 2,4 V | 0 a 2,7 V |

Esto explica, de forma cualitativa, por qué el arranque necesita 255 durante 250 ms y por qué un perfil «210» (82 % del ciclo) no da 82 % de velocidad. **NO MEDIDO**: el manual dice expresamente que no hubo mediciones de corriente ni de tensión (`03_hardware.md:57`).

### 2.2 Motor TT [4] (ficha del vendedor Adafruit, valores medidos por él y valores nominales)

| Magnitud | 3 V | 4,5 V | 6 V |
|---|---|---|---|
| Velocidad en vacío (medida) | 120 rpm | 185 rpm | 250 rpm |
| Velocidad mínima de operación (nominal, +-10 %) | 90 rpm | — | 200 rpm |
| Corriente en vacío (medida) | 150 mA | 155 mA | 160 mA |
| Corriente atascado (medida) | 1,1 A | 1,2 A | 1,5 A |
| Par de bloqueo (nominal) | 0,4 kg*cm | — | 0,8 kg*cm |

La ficha avisa que hay variación de motor a motor y que no hay retroalimentación de posición [4]. **Comparación (INFERENCIA, el diámetro de rueda no está documentado en el proyecto)**: con una rueda supuesta de 65 mm, la velocidad en vacío sería v = pi*D*rpm/60 = 41, 63 y 85 cm/s a 3, 4,5 y 6 V; los 38 cm/s medidos a PWM 255 equivalen a la velocidad en vacío a ~3 V, coherente con las caídas de la tabla anterior. Medir D lo cierra.

**Corriente por canal frente al driver (INFERENCIA).** Atascados, dos motores en paralelo piden 2,2 A (3 V) a 3,0 A (6 V) por canal [4], por encima de los **1,8 A continuos** y alrededor o por encima de los **2,5 A de pico** del driver [2][3]. Un atasco prolongado con el PWM aplicado es un riesgo térmico: con I = 2,2 A y Ron 1,0 a 1,6 ohm la disipación sería 4,8 a 7,7 W, **~3 a 4,5 veces el 1,73 W** que da la hoja del TC1508S [1] (supuesto: Ron por toda la trayectoria; la constante térmica no se conoce). Ver la sección 6 y el área 09 (el navegador **no detecta atascos**).

### 2.3 PWM de motor de escobillas

- PWM con tiempo de encendido mínimo de ~5 tau (tau = L/R del motor) para respuesta lineal; a frecuencias altas la corriente tiene menos tiempo para subir, bajando corriente y par promedio y volviendo no lineal la respuesta; a baja frecuencia aparecen vibración y ruido [7] (ejemplo de un motor pequeño de 1,5 V/3,4 ohm/19 uH; no es el TT). El comentario de `config.h:78-80` («1500 Hz da más torque bajo carga que 5000 Hz, a costa de zumbido») es coherente con esto (**media**).
- La eficiencia del motor depende de la frecuencia sobre todo con ciclos menores de 30 % [8].
- Modo de recirculación: «fast decay» (rueda libre, corriente por diodos) frente a «slow decay» (freno de lado bajo) [6]. **INFERENCIA:** con el modo que usa el proyecto (fast decay), la relación ciclo-velocidad a ciclos bajos suele ser peor que con slow decay; no vi una fuente que lo cuantifique para este chip (un foro de Pololu aparecido en la búsqueda **no se abrió**), así que solo se puede probar midiendo.

### 2.4 Zona muerta, arranque, desgaste

- **No encontré** una fuente con la tensión mínima de arranque o la fricción estática del TT; depende de la reductora y de la carga y **hay que medirla** [8 no la da; 4 da solo «velocidad mínima de operación»].
- El proyecto mide el «arranque» como 0 ms con el método de reflejo humano (`calibracion.md:317`), con incertidumbre propia de ±50 ms (`calibracion.md:306-307`), pero el firmware necesita 250 ms a plena potencia para vencer la inercia (`config.h`, `carro.c`).
- Desgaste: [10, sec. 6] recomienda recalibrar periódicamente por «carga y desgaste de las llantas». Para las reductoras de plástico (holgura y fricción variables) la fuente del proyecto es `calibracion_guia.md:13` (afirmación del proyecto, sin medición).

### 2.5 Valores del proyecto y lo esperable

| Dato del proyecto | Valor | Estado | Lo esperable / comentario |
|---|---|---|---|
| Velocidad a 255 (MANUAL) | 26,3 ms/cm = 38 cm/s (13 oct) | MEDIDO | ver 2.2: ~3 V equivalentes con D = 65 mm (INFERENCIA) |
| Velocidad AUTO 210/207 | 51,7 ms/cm = **19,3 cm/s** (14 oct) | MEDIDO, **pero caduco** | los registros reales del 15-nov (caja negra `cajas_negras/cn_20261115_152451.txt`: `plan_ms=650` para 200 mm y 683 para 210 mm) implican una pendiente de **~33 ms/cm = ~30 cm/s** (derivación mía: 33 ms por 10 mm). **La misma consigna PWM cambió ~57 %** entre sesiones; `calibracion.md:141` ya dice que ese día se cambiaron pilas y contactos. **No usar 19,3 cm/s como constante.** |
| Relación entre 210 y 255 | 19,3/38 = 0,51 con ciclo 0,82 (solo 13/14 oct) | MEDIDO | **INFERENCIA** (2 puntos, dos días, pilas distintas): v = k*(d - d0) da d0 = 164, es decir ~64 % de ciclo como «zona muerta efectiva» bajo carga; solo una curva v(ciclo) medida lo confirma |
| Balance MANUAL | 14,5 cm de desvío a 150 cm con 255/255 -> 3,0 cm con 988 | MEDIDO | con la geometría del proyecto (ancho 12 cm, SUPUESTO) 14,5 cm a 150 cm es un desbalance de ~1,5 %; se aplicó 1,2 %; residual esperado 2,8 cm: **coincide con el 3,0 medido** (cálculo mío) |
| Balance AUTO | 6 cm a 38,7 cm con 986 | MEDIDO (15 oct) | por la misma fórmula del proyecto equivale a ~9,6 % de desbalance, **no** 1,5 %. **INFERENCIA:** una carrera tan corta la domina el **desfase de arranque** entre lados: si un lado arranca 100 ms tarde a 38 cm/s, recorre 3,8 cm menos y, con ancho 12 cm, gira 3,8/12 = 0,32 rad ~ 18°. Por eso la prueba pasó de 2 s a 4 s (`perfiles_velocidad.md:43`) y existe `ARRANQUE_ESCALONADO_MS` (apagado; pendiente) |
| Giro (pivote) | 7,4 a 8,2 ms/° (derecha) y 7,8 a 12,0 (izq.) = ~122 a 135 °/s | MEDIDO | la auditoría estima ~130 °/s a PWM 255 sin rampa, y 14 a 25° de cola de inercia al soltar (`auditoria_autonomo_v2.md:131`): coherente (130 °/s x 0,144 a 0,191 s = 19 a 25°) |
| Desplazamiento del centro en un giro de 90° derecho | 150 mm adelante + 165 mm a la derecha | MEDIDO (6 veces) | ver 1.3; el modelo lineal fallaba 8 %-41 % (`calibracion.md:161`); el de pivote reproduce la cuerda 2R sen(theta/2) |
| Rectas de 150 cm del 15-nov | error de largo -10, -5, -5, -10 cm (n = 4 con cinta); 1 medida lateral: 12 cm a la derecha | MEDIDO (`docs/pruebas_fisicas_20261114.csv`, filas 2, 3, 6, 7; archivo sin versionar) | largo medio -7,5 cm (5 %) con RUMBO activo. **El «ángulo final» de esa tabla viene de la pose del giroscopio (caja negra), no de una medición externa**: -5,4; -1,8; 10,2; 3,5; -5,7; 3,1; 0,1 grados |

---

## 3. Errores típicos y modos de falla conocidos

1. **Cambio de velocidad con las pilas** (sección 2.5: +57 % entre sesiones con el mismo PWM). Efecto: todo `ms_por_cm` guardado caduca; error de largo proporcional (hasta ±50 % con una calibración vieja). No se vigila la tensión (`auditoria_autonomo_v2.md:106`).
2. **Caída de tensión que hunde el arranque**: ver tabla 2.1; con ambos canales en arranque la tensión en el motor puede caer por debajo de 3 V (INFERENCIA). Un reinicio por «brownout» del ESP32 es una causa posible, registrada en la cámara como causa de reinicio (`auditoria_autonomo_v2.md:33`); en el carro no se lee la causa de reinicio (`:85`).
3. **Corriente de atasco por encima del driver** (2,2 a 3 A por canal contra 1,8 A continuos). Un obstáculo lateral o una rueda trabada con el PWM puesto no dispara ninguna protección conocida (sec. 2.1) ni el láser frontal.
4. **Rueda libre en lugar de freno**: STOP no frena activamente; la cola de inercia es 1,4 cm (avance, 210) y 14-25° (giro) (sec. 2.5). Con muñones plásticos y suelo distinto, ese valor cambia.
5. **Deslizamiento al arrancar y al girar** [10, sec. 2 «sobreaceleración, giro con patinaje»]: el firmware arranca a 255 sin rampa (`auditoria_autonomo_v2.md:68`). Es un error **no sistemático** que ningún calibrado compensa; solo se acota con lazo cerrado (giroscopio).
6. **Orientación mal -> posición mal**: un grado de error de rumbo a 150 cm da 2,6 cm lateral (tan 1° x 150); 5° dan 13 cm; 10°, 26 cm (aritmética mía). Por eso `RUMBO` (PI con giroscopio) pesa más que afinar `ms_por_cm`.
7. **Cancelación de errores que esconde un solo sentido de prueba** [10, sec. 3.1]: calibrar con un solo giro (por ejemplo solo la derecha) puede dar «excelente» y estar mal. Aquí la asimetría izquierda/derecha del pivote (R 15,8 contra 8,7 a 9 cm) es justamente ese caso.
8. **Giros pequeños y por tiempo**: un giro de 4° por tiempo giró 18° en el banco (`auditoria_autonomo_v2.md:131`); por eso los giros < 5° no se piden por tiempo.
9. **Contacto eléctrico intermitente** (una ruta de tres no giró; `calibracion.md:246`): fallo del carro, no del software; nada de la calibración lo cubre.
10. **Documentación que contradice el código**: EBRAKE (freno activo en el manual, rueda libre en el código), pivote (lados opuestos en el manual, un solo lado en el código), PWM 1000 Hz (manual) contra 1500 Hz (código), pivote izquierdo «espejo» (limites) contra R = 8,7 a 9 cm medido (calibración).

---

## 4. Buenas prácticas reconocidas

- **Calibrar sistemáticamente, no a ojo**: separar el error de recta (cociente de diámetros) del de giro (ancho de vía), probar en ambos sentidos y repetir 5 corridas por sentido, usando el centro de gravedad de cada nube [10]. Calibrar solo uno de los dos errores da resultados «excelentes» falsos [10, sec. 3.1].
- Calibrar con el **mismo piso, carga y pilas** de uso; recalibrar periódicamente [10, sec. 6]. (El proyecto ya lo avisa en pantalla: «Recalibra con las pilas que usarás en la demo».)
- Condensadores de desacoplo en la alimentación y en cada motor, como en el circuito de referencia del fabricante [1, pág. 4] (no sé si el carro los tiene: **NO VERIFICADO**).
- Dejar margen de corriente frente a la hoja de datos [1, nota 2].
- Usar medidas absolutas para acotar el error de odometría (el paper usa sonar con ±2 mm de exactitud) [10]; aquí el equivalente son el giroscopio (rumbo) y el láser (distancia a un obstáculo).
- Elegir el modo de recirculación (fast/slow decay) según linealidad y rizado [6]; probar ambos cuando importe la linealidad a velocidad baja.

---

## 5. Fuentes

Todas consultadas el 2026-11-16. «Leída» = abrí el documento y leí su contenido.

1. **TC1508S «双通道直流马达驱动器», documento S&CIC1061, 4 páginas** (la marca no aparece impresa en el documento leído; los buscadores lo asocian a Superchip/Fuman). URL: https://atta.szlcsc.com/upload/public/pdf/source/20161129/1480413685647.pdf. **Leída (las 4 páginas, como imagen).** Es el TC1508S, **no el TC1508A**. Confianza: **alta** para el TC1508S; **baja** al aplicarla al TC1508A.
2. LCSC, «FM TC1508A» (C5142992), ficha de distribuidor. https://www.lcsc.com/product-detail/C5142992.html. Leída. Confianza **media**. (El enlace de su hoja de datos, https://www.lcsc.com/datasheet/C5142992.pdf, devolvió solo el título: **hoja del TC1508A NO CONSULTADA**.)
3. JLCPCB, «TC1508A» de Shenzhen Fuman Elec (C5142992). https://jlcpcb.com/partdetail/Shenzhen_FumanElec-TC1508A/C5142992. Leída. Confianza **media**. (Su PDF, URL firmada de aliyuncs, dio 403: NO CONSULTADA. Las copias en datasheet4u y la de eeworld también fallaron o dieron página vacía.)
4. Adafruit, «DC Gearbox Motor - TT Motor - 200RPM - 3 to 6VDC» (producto 3777). https://www.adafruit.com/product/3777. Leída. Confianza **media-alta** (cifras medidas por el vendedor más nominales); es un motor genérico, no necesariamente el del carro.
5. Energizer, «EN91 AA, product datasheet» (copia en sparkydirect.com.au, impresión de 2018). https://www.sparkydirect.com.au/assets/brochures/EN91.pdf?1000832821. Leída con `pdftotext`. Confianza **alta** (hoja del fabricante, variante industrial); no se sabe qué pila usa el carro.
6. Murugavel Raju, «Current Recirculation and Decay Modes», Texas Instruments, SLVA321A, ago-2011 (rev. abr-2021). https://www.ti.com/lit/SLVA321. Leída (secciones 1 y 2.1-2.2). Confianza **alta**.
7. Precision Microdrives, «AB-022: PWM Frequency For Linear Motion Control» (sin fecha ni autor visibles). https://www.precisionmicrodrives.com/ab-022. Leída. Confianza **media** (nota de aplicación de un fabricante, motor pequeño distinto).
8. Adafruit Learn, «Improve Low Speed Performance of Brushed DC Motors: PWM and Brushed DC Motors». https://learn.adafruit.com/improve-low-speed-performance-of-brushed-dc-motors/pwm-and-brushed-dc-motors. Leída (solo la página indicada; no explica la fricción estática). Confianza **media**.
9. Wikipedia, «Mecanum wheel». https://en.wikipedia.org/wiki/Mecanum_wheel. Leída. Confianza **media** (fuente secundaria; sirve para la regla de movimiento, no trae cifras de deslizamiento).
10. **J. Borenstein y L. Feng, «Measurement and Correction of Systematic Odometry Errors in Mobile Robots», IEEE Trans. Robotics and Automation, 1996** (manuscrito del autor, 23 pp.). https://cs.au.dk/~ocaprani/legolab/DigitalControl.dir/NXT/Lesson10.dir/paper58.pdf. **Leída completa.** Confianza **alta**.
11. Y. Maddahi, A. Maddahi y N. Sepehri, «Calibration of omnidirectional wheeled mobile robots: method and experiments», Robotica 31(6), 969-980, 2013. https://www.cambridge.org/core/journals/robotica/article/abs/calibration-of-omnidirectional-wheeled-mobile-robots-method-and-experiments/5E30AECB1CBE6313A2227D9D2A2C7FDD. **Solo el resumen.** Confianza **media**.
12. J. Borenstein, H. R. Everett y L. Feng, «Where am I? Sensors and Methods for Mobile Robot Positioning», Univ. de Michigan, 1996. **NO CONSULTADA** (PDF de 13,5 MB sobre mi límite de lectura; espejo de Duke y DTIC fallaron).
13. H. Taheri, B. Qiao y N. Ghaeminezhad, «Kinematic Model of a Four Mecanum Wheeled Mobile Robot», Int. J. Computer Applications 113(3), 6-9, 2015. https://ijca.phdfocus.com/archives/volume113/issue3/19804-1586. **Solo el resumen** (no vi las ecuaciones). Confianza **media**.
14. Documentos y código del proyecto (confianza **alta** como registro; el estado MEDIDO/SIMULADO es el que ellos declaran): `docs/perfiles_velocidad.md`, `docs/calibracion.md`, `docs/calibracion_guia.md`, `nucleo_c/calibracion.c/.h`, `nucleo_c/trayectoria.c`, `docs/precision/` (`monte_carlo_1000.txt`, `plan_pruebas.md`, `simulador.md`), `docs/manual/capitulos/03_hardware.md`, `04_firmware.md`, `07_navegacion.md`, `firmware_carro/carro.c`, `config.h`, `docs/auditoria_autonomo_v2.md`, `docs/limites_conocidos.md`, `docs/pruebas_fisicas_20261114.csv` y `cajas_negras/cn_20261115_*.txt` (estos dos sin versionar al momento de leerlos).

---

## 6. Qué implica para este carro

Cada punto: acción, criterio de verificación y tipo (HECHO = verificado en documentos/código; INFERENCIA = mía).

1. **Confirmar qué chip es de verdad.** Leer el marcado del driver y bajar su hoja de datos (TC1508A o TC1508S). Si fuera el S, su máximo absoluto de 5,5 V [1] queda **por debajo** de 4 pilas alcalinas frescas (6 V nominales). Criterio: marcado leído y hoja archivada en `docs/`. (INFERENCIA sobre el riesgo; el dato de 5,5 V es HECHO para el S.)
2. **Medir tensión y corriente reales** con multímetro y pinza o resistencia shunt: tensión de pilas en reposo, en marcha recta en AUTO y en arranque; corriente por canal. Criterio: comparar con la tabla de 2.1 y con 1,8/2,5 A [2][3]. Si el pico de arranque pasa de 2,5 A, probar `ARRANQUE_ESCALONADO_MS` = 30 a 60 ms (ya implementado y apagado: `perfiles_velocidad.md:57-60`). (NO MEDIDO hoy.)
3. **Tratar «19,3 cm/s» como dato caducado.** Medir la velocidad AUTO al empezar cada sesión (3 corridas de 150 cm) y registrar la tensión de las pilas; actualizar `docs/investigacion_autonomo/00_mapa.md` (tabla 3.1 usa 19,3 cm/s) y el manual. Criterio: variación entre sesiones documentada. (HECHO: la discrepancia 19,3 contra ~30 cm/s sale de comparar `calibracion.md` con `cn_20261115_152451.txt`.)
4. **Medir la curva velocidad-ciclo**: ciclos 150, 180, 210, 240, 255 x 3 repeticiones de 150 cm con las mismas pilas. Criterio: ajuste lineal; si el intercepto da ~164/255 como predice mi cálculo, hay zona muerta efectiva grande y el perfil AUTO opera cerca de ella. (INFERENCIA a confirmar.)
5. **Medir el desfase de arranque entre lados** con el ángulo del giroscopio de los primeros 500 ms (SENS a 100 ms) en 10 arranques. Criterio: desvío angular del arranque y su dispersión; si es del orden de 10-20° en AUTO, explica el 9,6 % «equivalente» (INFERENCIA) y justifica el arranque escalonado o una rampa de ~100 ms (a probar, no implementar sin medir: la rampa puede agrandar el tiempo de arranque).
6. **UMBmark adaptado** (integra todo lo anterior): cuadrado de 100 cm de lado (hay escenario `cuadrado` en el banco), **5 corridas en cada sentido**, referencia en esquina de paredes en L, medir posición y ángulo final con cinta; calcular el centro de gravedad de cada nube y E_max,syst como en [10]. Criterios propuestos (INFERENCIA, míos): E_max,syst menor o igual al p95 simulado del cuadrado de J14 (125 mm, `monte_carlo_1000.txt`; SIMULADO) y reportar sigma/SEM; si E_max,syst > 3 x SEM, recalibrar los parámetros antes de repetir [10]. La **separación entre las nubes horaria y antihoraria** mide justo la asimetría izquierda/derecha del pivote. No usar las fórmulas de correccion E_b/E_d tal cual: suponen giro en el sitio [10, sec. 2.3], aquí el pivote desplaza el centro.
7. **Corregir la documentación** que contradice el código (sec. 3, punto 10) y decidir el comportamiento deseado de EBRAKE: hoy es rueda libre. Si se quisiera un freno corto, la tabla del TC1508S admite HH [1], pero el TC1508A no se verificó y el pico de corriente de frenado no está medido: probar primero sin carga. (HECHO para la contradicción; el freno activo es INFERENCIA.)
8. **Medir el pivote izquierdo por separado y repetirlo.** Aunque `calibracion.md:365` da R = 8,7 a 9 cm, varios documentos aún lo llaman «espejo». Criterio: 3 giros de 90° a cada lado, media y dispersión de (adelante, lateral); si la dispersión es mayor que 30 % del desplazamiento, el modelo de pivote debe llevar incertidumbre (hoy `cal_desplazamiento_promediar` solo exige repetibilidad de dos medidas).
9. **Qué esperar de navegar por tiempo calibrado (orden de magnitud; INFERENCIA salvo lo marcado):**
   - Largo: con calibración fresca, error de 3 a 7 % (MEDIDO, n = 4: -5 a -10 cm a 150 cm); con calibración vieja de otra sesión, hasta decenas de %.
   - Lateral por rumbo: lateral = distancia x tan(error de rumbo): 2,6 cm por grado a 150 cm. Con `RUMBO` activo ese error ya no crece con la distancia pero sigue acotado por la deriva del giroscopio (supuesta: +-0,05 °/s y escala +-1 %; `docs/precision/simulador.md:15`, SIMULADO).
   - Giros: la precisión de ángulo depende del lazo cerrado con giroscopio (+-3° «por construcción» en J14, 28 de 28, medido **por el mismo giroscopio**, no por un patrón externo).
   - Posición tras k esquives: el simulador da error final del esquive de 20 x 20 de 200 mm de media (p95 324 mm) con la configuración de J14 y **la medición real de J14 (400 mm) cayó en el percentil 99 de esa simulación** (`monte_carlo_1000.txt`), es decir la simulación subestima el error real; cada pivote suma 15 a 17 cm de desplazamiento del centro que el modelo debe seguir bien.
   - [10] recuerda que sin referencias absolutas el error de odometría crece sin límite; las referencias absolutas de este carro son el láser contra un obstáculo y el giroscopio para el rumbo, nada para la posición lateral.
10. **No confiar en el diseño de potencia sin una protección propia**: como el hardware no detecta atascos (sec. 2.2 y área 09), el firmware o la app deberían cortar el PWM si lleva más de N ms con orden de avance y el giroscopio/láser no indican movimiento (propuesta, INFERENCIA; N a elegir con las mediciones 2 y 4).

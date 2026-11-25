# Si te preguntan X, ve a la página Y

Índice de consulta rápida para la exposición. La columna «Página» es el número de página de este PDF y también es un enlace. Cada subsistema (A a I) tiene siempre las mismas siete secciones: **1** qué es, **2** cómo funciona, **3** cómo lo hicimos (decisiones y alternativas), **4** cómo lo probamos, **5** límites, **6** términos clave y **7** preguntas probables del profesor; por eso, cuando una pregunta no esté listada, busca en la sección que corresponda de la parte que toca.

## Visión general

| Si te preguntan… | Ve a | Página |
|---|---|---|
| ¿Qué es el proyecto y cómo se conectan carro, cámara, teléfono y núcleo? | Mapa general | {{pag:mapa}} |
| ¿Qué significan VALIDADO EN EL CARRO, SIMULADO, NO MEDIDO y PENDIENTE? | Capítulo 1, convenciones de evidencia | {{pag:c1}} |
| ¿Qué quiere decir tal término? | Glosario general | {{pag:glosario}} |
| ¿Qué se probó en el carro físico y qué solo en simulación? | H.4 Cómo lo probamos | {{pag:H.4}} |

## A · Hardware del carro

| Si te preguntan… | Ve a | Página |
|---|---|---|
| ¿Qué piezas tiene el carro? | A.1 Qué es y para qué sirve | {{pag:A.1}} |
| ¿Cómo se conectan los motores, el driver y los sensores? | A.2 Cómo funciona | {{pag:A.2}} |
| ¿Por qué hay tres fuentes de energía? | A.3 Cómo lo hicimos | {{pag:A.3}} |
| ¿Qué falló o qué limita al hardware? | A.5 Límites conocidos | {{pag:A.5}} |
| ¿Qué es una rueda mecanum, un puente H o el I2C? | A.6 Términos clave | {{pag:A.6}} |

## B · Firmware del carro

| Si te preguntan… | Ve a | Página |
|---|---|---|
| ¿Cómo se conecta el teléfono al carro? ¿Qué es la red CarroFSI? | B.2 Cómo funciona | {{pag:B.2}} |
| ¿Qué comandos entiende el carro y qué contesta (SENS, CAPS)? | B.2 Cómo funciona | {{pag:B.2}} |
| ¿Cómo mantiene el rumbo en línea recta (RUMBO) y cómo gira? | B.2 Cómo funciona | {{pag:B.2}} |
| ¿Por qué una red propia y no el WiFi de la escuela? | B.3 Cómo lo hicimos | {{pag:B.3}} |
| ¿Qué se probó del firmware y qué no? | B.4 Cómo lo probamos | {{pag:B.4}} |

## C · Módulo de cámara

| Si te preguntan… | Ve a | Página |
|---|---|---|
| ¿Qué transmite la cámara y a quién? | C.2 Cómo funciona | {{pag:C.2}} |
| ¿Por qué la cámara va en otra placa? | C.3 Cómo lo hicimos | {{pag:C.3}} |
| ¿Qué tan rápido llega el video y con qué retraso? | C.4 Cómo lo probamos | {{pag:C.4}} |
| ¿Qué le pasa al video si falla algo? | C.5 Límites conocidos | {{pag:C.5}} |

## D · App Android

| Si te preguntan… | Ve a | Página |
|---|---|---|
| ¿Qué pantallas y qué modos tiene la app? | D.2 Cómo funciona | {{pag:D.2}} |
| ¿Por qué el núcleo está en C y se llama por JNI? | D.3 Cómo lo hicimos | {{pag:D.3}} |
| ¿Qué se cambió en la interfaz para quitar parpadeos y cómo se probó? | D.4 Cómo lo probamos | {{pag:D.4}} |
| ¿Qué pasa si se cae la conexión con el carro o con la cámara? | D.2 Cómo funciona | {{pag:D.2}} |

## E · Núcleo de navegación en C

| Si te preguntan… | Ve a | Página |
|---|---|---|
| ¿Qué hace cada módulo de `nucleo_c`? | E.2 Cómo funciona | {{pag:E.2}} |
| ¿Cómo decide el navegador cuándo avanzar, esquivar o pedir ayuda? | E.2 Cómo funciona | {{pag:E.2}} |
| ¿Qué es el estado AYUDA? | E.6 Términos clave | {{pag:E.6}} |

## F · Visión

| Si te preguntan… | Ve a | Página |
|---|---|---|
| ¿Cómo distingue el piso de un obstáculo? | F.2 Cómo funciona | {{pag:F.2}} |
| ¿Qué es la red neuronal, quién la entrenó y para qué sirve aquí? | F.3 Cómo lo hicimos | {{pag:F.3}} |
| ¿Cómo se combinan láser y cámara y quién manda si no coinciden? | F.2 Cómo funciona | {{pag:F.2}} |
| ¿Qué objetos o condiciones no ve bien? | F.5 Límites conocidos | {{pag:F.5}} |

## G · Seguridad

| Si te preguntan… | Ve a | Página |
|---|---|---|
| ¿Qué detiene el carro y en qué orden? | G.2 Cómo funciona | {{pag:G.2}} |
| ¿Qué pasa si se cae el WiFi o se bloquea la app? | G.2 Cómo funciona | {{pag:G.2}} |
| ¿Cómo se apaga el modo autónomo y se pasa a manual? | G.2 Cómo funciona | {{pag:G.2}} |
| ¿Qué de la seguridad se midió en el carro? | G.4 Cómo lo probamos | {{pag:G.4}} |

## H · Simulador y pruebas

| Si te preguntan… | Ve a | Página |
|---|---|---|
| ¿Cómo saben que funciona si no probaron en el piso? | H.2 Cómo funciona | {{pag:H.2}} |
| ¿Qué semilla usa el Monte Carlo y por qué? | H.3 Cómo lo hicimos | {{pag:H.3}} |
| ¿Qué pruebas automáticas hay y cuántos casos pasan? | H.4 Cómo lo probamos | {{pag:H.4}} |
| ¿Qué falta medir en el carro? | H.5 Límites conocidos | {{pag:H.5}} |

## I · Proceso de desarrollo

| Si te preguntan… | Ve a | Página |
|---|---|---|
| ¿Cómo lo hicieron? ¿Qué versiones hubo? | I.2 Cómo funciona | {{pag:I.2}} |
| ¿Qué se probó y se descartó (v4, v5, v6) y por qué? | I.3 Cómo lo hicimos | {{pag:I.3}} |
| ¿Qué quedó pendiente? | I.5 Límites conocidos | {{pag:I.5}} |

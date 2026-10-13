MENSAJE 2 de 7 - PARTE 1. Guárdalo en docs/trabajos/parte_1.md y responde "recibido 2/7". No ejecutes hasta EMPIEZA.

Objetivo: adaptar todo a la cámara real en HQVGA 240x176.

a) camara_prueba: tamaño por defecto HQVGA, calidad 50, XCLK 20 MHz, RGB565. QVGA y QQVGA siguen disponibles con /control?size=.

b) obstaculos.c: acepta ancho y alto del cuadro en cada llamada, sin suponer 320x240. Celdas de 10x10 píxeles; filas y columnas se calculan. fila_minima pasa a ser proporción del alto (0.0 a 1.0; el valor anterior 12 de 24 equivale a 0.5) y se convierte a fila al ejecutar. La tira de referencia del piso (abajo al centro) también se calcula en proporción. Valida tamaños raros: rejilla menor de 3x3 celdas da error claro, sin cuelgue.

c) Pruebas: repite las del detector para 320x240 y 240x176, y añade 160x120 (documenta si da error o funciona con aviso). Las 71 pruebas anteriores deben seguir pasando.

d) navegador.c: antirrebote en milisegundos, no en cuadros. Parámetro MS_CONFIRMAR_LEJOS (por defecto 150). Confirmación inmediata con 1 solo cuadro si la cercanía es 3, o si es 2 y el obstáculo está en la zona central. Usa el reloj que ya recibe el navegador. Pruebas con cuadros cada 80 ms y cada 120 ms.

e) docs/latencia.md: cadena cámara (0.32 s medido) + detección + STOP, y cuántos cm recorre el carro hasta frenar a 20 cm/s de ejemplo.

f) VisionActivity: acepta el tamaño del cuadro que llegue, sin valores fijos.

Entrega: commit y resumen corto.

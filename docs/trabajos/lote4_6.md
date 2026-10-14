MENSAJE 6 de 7 - MODOS DE VIDEO RAW Y CELDAS. Guárdalo tal cual en docs/trabajos/lote4_6.md y responde solo "recibido 6/7". No ejecutes hasta EMPIEZA. Sigue docs/trabajos/reglas.md.

TRABAJO I. Probar dos formas de bajar la latencia del video: A) imagen cruda y B) resumen por celdas por UDP. El modo JPEG queda como predeterminado y no debe empeorar. Interruptor en la cámara: /control?modo=jpeg|raw|celdas (al reiniciar vuelve a jpeg); la app lo elige en "Fuente del video".

A) RAW: ruta /raw en camara_prueba, HQVGA RGB565 sin fmt2jpg. Cabecera por cuadro: ancho, alto, número y ms entre captura y envío. VideoCarro.kt lo convierte a bitmap y lo mete a la misma cadena de análisis. Cola de 1 en la cámara: descarta cuadros viejos.

B) CELDAS por UDP: en la cámara, antes de comprimir, calcula el promedio RGB de cada celda de 10x10 con el MISMO código C del detector (compila nucleo_c en el firmware, no lo reescribas). Un paquete UDP por cuadro con número, ms desde captura y las celdas. El teléfono manda UDP "HOLA" al puerto 4210 y la cámara responde a esa dirección (sin IP fija). El JPEG para ver sigue en paralelo con menor prioridad. En obstaculos.c agrega entrada por celdas ya promediadas; prueba en PC: entrada por cuadro y por celdas dan el mismo resultado. El fail-safe de 500 ms y video_stats cuentan los paquetes de celdas.

Medición sin imágenes: /stats por modo (fps, bytes/s, ms captura-envío, ida y vuelta de red/2 con eco del teléfono); la app registra MODO_STATS cada 5 s (fps de llegada, hueco máximo, huecos de más de 300 ms, ms de llegada a decisión); mide que el JPEG paralelo no baje de 8 fps en modo celdas; OBST_PRUEBA anota el modo.

Seguridad: no cambies el protocolo de motores. Si un modo falla, la app vuelve a jpeg y avisa.
Entrega: pruebas en PC, docs/latencia_modos.md con tabla vacía. Commit y resumen corto con los pasos de la comparación: 3 modos x 3 pasadas con una caja a ~80 cm (mismo obstáculo, distancia y luz).

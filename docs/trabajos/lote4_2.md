MENSAJE 2 de 7 - ESQUIVE DE OBJETOS GRANDES Y LATENCIA. Guárdalo tal cual en docs/trabajos/lote4_2.md y responde solo "recibido 2/7". No ejecutes hasta EMPIEZA. Sigue docs/trabajos/reglas.md. Antes de cada trabajo revisa si ya existe algo en el repo y complétalo en vez de repetirlo.

TRABAJO C - ESQUIVE.
1. Estudia en navegador.c por qué termina en ERROR con un objeto grande (mochila) tras girar; documenta en docs/navegador.md.
2. Reintento: si tras el giro sigue muy cerca, retrocede por tiempo (máx 15 cm; retroceso ciego, documenta el riesgo), gira del mismo lado con más ángulo (45, luego 90 grados) y mira otra vez. Máx 2 reintentos; si no, ERROR y STOP.
3. CalibracionCarro: ms por cm hacia atrás y su prueba en CalibracionActivity (sin calibrar, usa el del avance con aviso).
4. Gira hacia el lado con menos celdas de obstáculo; si empatan, comportamiento actual.
5. Valores por defecto en el código: fila mínima ~440, celdas mínimas 7, umbral de color 40, modelo de objetos NO, frenar desde lejos en el centro con el video del carro. Documenta unidades en docs/obstaculos.md.
6. Pruebas en PC con el simulador (objeto que ocupa todo el frente, pared, esquina, obstáculo que desaparece, reintentos agotados). PENDIENTE HARDWARE: ruta con mochila a mitad de camino, 5 corridas.

TRABAJO D - LATENCIA DEL VIDEO. Mide antes y después de cada paso; si no gana 5 ms, descártalo.
1. Compresión en la cámara: mide fmt2jpg en HQVGA (calidad 50) y compárala con un compresor para microcontroladores (por ejemplo JPEGENC de bitbank2; anota licencia) y con submuestreo 4:2:0. Cambia solo si la app decodifica bien y la ganancia es clara. Registro JPEG_MS.
2. Confirma cuadro más reciente (cola de 1) sin acumulación en el socket; mide la edad del cuadro en /stats.
3. TCP_NODELAY en el servidor de video y en el WebSocket; confirma setSleep(false) y canal fijo.
4. VideoCarro.kt: hilos separados para lectura, decodificación y análisis con cola de 1 (descarta el viejo) y bitmaps reutilizados. Mide ms de llegada a decisión (MODO_STATS).
5. Antes y después en docs/latencia.md.

Un commit por trabajo y resumen corto.

MENSAJE 1 de 7 - PERFILES DE VELOCIDAD Y CORREDOR DE TRAYECTORIA. Guárdalo tal cual en docs/trabajos/lote4_1.md y responde solo "recibido 1/7". No ejecutes hasta EMPIEZA. Sigue docs/trabajos/reglas.md.

TRABAJO A - PERFILES. Manual a velocidad máxima (255) y semiautomático limitado (el 210 actual).
1. config.h del DevKit (comentarios en español): PERFIL_MANUAL (255, ajuste recto validado a 255: VELOCIDAD_IZQ 252, ARRANQUE_PLENO_MS 250) y PERFIL_AUTO (210 actual con su ajuste), cada uno con sus propios VELOCIDAD_IZQ, VELOCIDAD_DER y ARRANQUE_PLENO_MS.
2. Comandos sin parámetros MODO_MANUAL y MODO_AUTO, con ACK_MANUAL / ACK_AUTO. Si el carro se mueve al cambiar, primero STOP.
3. Al arrancar, reconectar el WebSocket o tras el watchdog el perfil es AUTO. La app pide MODO_MANUAL explícitamente. Heartbeat y STOP por pérdida de señal igual.
4. App: ControlActivity envía MODO_MANUAL al abrir y reconectar y muestra el perfil. Rutas y prueba de obstáculo envían MODO_AUTO y no arrancan sin confirmación (1 s, si no ERROR claro). CalibracionActivity siempre en AUTO. En manual la detección no frena por defecto (documéntalo); EBRAKE disponible.
5. Pruebas en PC (servidor falso y firmware). PENDIENTE HARDWARE: ambos perfiles rectos en 150 cm, anotando el desvío.

TRABAJO B - CORREDOR DE TRAYECTORIA. Hoy la decisión usa zonas izquierda/centro/derecha. Agrega un corredor: la huella del carro (ANCHO_CARRO_CM, ALTURA_CAMARA_CM, INCLINACION_GRADOS y FOV configurables) proyectada en la imagen por perspectiva, estrechándose con la distancia. Un obstáculo frena o se esquiva solo si está dentro del corredor; el que pasa por un lado solo avisa. Margen lateral configurable; en la duda dentro del margen, frena. Interfaz común en C para detector, modelo de objetos y flujo. Pruebas en PC con obstáculos dentro, en el borde y fuera del corredor, a varias distancias. Apagado por defecto. Documenta en docs/obstaculos.md.

Un commit por trabajo y resumen corto.

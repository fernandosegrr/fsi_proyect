MENSAJE 4 de 7 - PARTE 3. Guárdalo en docs/trabajos/parte_3.md y responde "recibido 4/7". No ejecutes hasta EMPIEZA.

Objetivo: que el detector no dependa de un piso de un solo color. Habrá baldosas, vetas, alfombras, varios tonos.

a) Modelo del piso con varias referencias: hasta K=4 grupos de color (k-medias corto de 3 iteraciones sobre las celdas de la tira inferior). Una celda es piso si está cerca de cualquier grupo (con normalización de brillo como ahora).

b) Aprendizaje continuo seguro: el modelo solo se actualiza cuando no hay obstáculo y el carro avanza, con promedio móvil lento, y nunca con celdas marcadas como obstáculo (para no aprender el obstáculo como piso).

c) Cambio de piso: si todas las celdas quedan lejos de todos los grupos durante 1 s, reaprende desde la tira inferior y avisa.

d) Objetos del color del piso (una libreta): añade el borde (gradiente simple entre celdas vecinas) como segunda señal. Déjalo activo solo si las pruebas muestran mejora sin más falsos positivos; si no, déjalo apagado y documenta por qué.

e) Pruebas en PC con imágenes sintéticas: baldosas de 2 tonos, vetas, sombra suave (no debe detectar), mano o libreta sobre el piso (debe detectar), escalera lejana (no debe disparar STOP si está lejos).

f) ConfigObstaculos gana K y velocidad de aprendizaje. Las 71 pruebas anteriores siguen pasando.

g) Actualiza docs/obstaculos.md. C simple con comentarios en español. Commit y resumen corto.

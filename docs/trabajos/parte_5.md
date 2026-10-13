MENSAJE 6 de 7 - PARTE 5. Guárdalo en docs/trabajos/parte_5.md y responde "recibido 6/7". No ejecutes hasta EMPIEZA.

Objetivo: medir cuánto gira el carro con la cámara, para corregir el error del giro por tiempo (no hay IMU).

a) movimiento_visual.c (C puro): estima el corrimiento horizontal entre dos cuadros consecutivos por proyección de columnas (suma de brillo por columna) y búsqueda del mejor corrimiento, con subpíxel simple. Usa solo la mitad superior del cuadro (el horizonte se mueve más limpio que el piso).

b) Con ~10 a 12 fps y giro rápido, el corrimiento entre cuadros puede ser grande: el rango de búsqueda debe ser al menos un tercio del ancho. Si la mejor coincidencia es débil, devuelve "sin confianza" y no suma.

c) Corrimiento a grados con el campo de visión horizontal (parámetro FOV_GRADOS, inicial 60, calibrable en CalibracionCarro con un giro de 90 grados medido).

d) Acumulador de ángulo que se reinicia al empezar el giro. El navegador corta el giro al alcanzar el ángulo pedido, con el tiempo como respaldo. Sin confianza, sigue por tiempo.

e) Ignora los cuadros durante el retardo de arranque medido en calibración.

f) Pruebas en PC con imágenes sintéticas desplazadas 1, 10, 40 y 80 píxeles, con ruido, y con escena casi uniforme (debe dar "sin confianza").

g) PENDIENTE HARDWARE: validar con el carro real. Añade en CalibracionActivity una prueba que gire 90 grados y compare con el ángulo visual.

h) docs/giro_visual.md. Commit y resumen corto.

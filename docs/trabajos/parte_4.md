MENSAJE 5 de 7 - PARTE 4. Guárdalo en docs/trabajos/parte_4.md y responde "recibido 5/7". No ejecutes hasta EMPIEZA.

Objetivo: combinar el detector de piso (~7 ms) con EfficientDet-Lite0 (~45 ms, CPU 1 hilo) en la app, para reconocer objetos como tales y no solo por color.

a) fusion.c (C puro). Entrada: resultado del detector del cuadro actual + últimas detecciones del modelo con marca de tiempo y zona. Salida: severidad y zona final.

b) Reglas: si el detector dice obstáculo cercano, STOP sin esperar al modelo (el detector manda en seguridad). Si el modelo ve un objeto en el camino y el detector duda, sube la severidad. Si el modelo ve un objeto lejano que el detector no ve, solo avisa y no frena.

c) El modelo corre en un hilo aparte, máximo ~10 veces por segundo, sin bloquear al detector. Su resultado caduca a 300 ms; si es más viejo se ignora. Ajustable en ConfigFusion.

d) Convierte la caja del modelo a zona izquierda/centro/derecha y a cercanía aproximada por la posición vertical del borde inferior y el tamaño de la caja.

e) Clases de interés configurables (persona, mascota, silla, mesa, botella...); cualquier otra clase cuenta igual si está en el camino.

f) VisionActivity muestra qué fuente decidió (detector, modelo o ambos) y los ms de cada etapa.

g) Pruebas en PC con entradas simuladas: modelo caducado, contradicciones, STOP inmediato por detector. Las mediciones en el Huawei van como PENDIENTE HARDWARE.

h) docs/fusion.md. Commit y resumen corto.

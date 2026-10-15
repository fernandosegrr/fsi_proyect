MENSAJE 1 de 4 - DIRECCIÓN VISUAL Y SISTEMA DE DISEÑO. Guárdalo tal cual en docs/trabajos/rediseno_1.md y responde solo "recibido 1/4". No ejecutes hasta EMPIEZA REDISEÑO. Sigue docs/trabajos/reglas.md. Reemplaza al antiguo TRABAJO 14 y a L1.

Objetivo: una app que se vea y se sienta profesional y clara, sin cambiar la lógica que funciona. Mantén Views y Material Components (no migres a Compose ni reescribas módulos). Cambia la capa visual y la navegación, con pruebas.

1. Dirección visual: sensación de panel de control moderno, calmado y legible, no de herramienta de depuración. Tema oscuro por defecto y claro opcional (sigue al sistema). Superficies gris azulado muy oscuro, UN color de acento (cian o verde azulado) para acciones, y colores con significado fijo: verde = bien, ámbar = aviso, rojo = paro o peligro. Define los tokens en docs/diseno.md (valores hexadecimales, contraste mínimo 4.5:1 comprobado con un script) y en res/values.
2. Tipografía: una familia sans de licencia libre incluida en la app (por ejemplo Inter, OFL; anota la licencia) en 4 estilos: título, subtítulo, cuerpo y etiqueta; cifras tabulares para latencia, fps, cm y tiempos. Una sola cosa grande por pantalla.
3. Forma y espacio: cuadrícula de 8 dp, esquinas de 16 dp en tarjetas y 28 dp en botones principales, bordes finos o sombras sutiles, márgenes generosos. Botones de manejo de al menos 56 dp; el resto, 48 dp.
4. Componentes reutilizables, documentados en diseno.md: tarjeta de estado, chip de estado (conectado, aviso, error), botón primario y secundario, botón de PARO, cabecera, estado vacío, diálogo de confirmación y aviso en banda (qué pasó y qué hacer).
5. Movimiento: transiciones de 150 a 250 ms, respuesta al tocar, vibración corta en acciones importantes y en obstáculo, indicador de conexión que "respira" suavemente. Nada recargado.
6. Iconos: un solo conjunto coherente (Material Symbols, Apache 2.0), mismo grosor.
7. Quitar WALL-E: busca sin distinguir mayúsculas "wall", "walle" y "wall-e" en código, recursos, cadenas, comentarios, documentación, firmware (incluida firmware_carro/pagina.h y su prueba de hash: actualízala) y scripts. NO toques original_backup/. Cambia el SSID por defecto en secretos.ejemplo.h por un nombre neutro (CarroFSI); la contraseña real queda solo en secretos.h. Cámara y DevKit deben usar el mismo SSID: se recargan las dos al final, una por USB. PENDIENTE HARDWARE: reconectar el teléfono a la red nueva.

Un commit por punto grande y resumen corto.

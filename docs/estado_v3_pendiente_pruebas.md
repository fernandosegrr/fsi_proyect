# Estado de v3 para la exposición

La base de la exposición es `feat/autonomo-v3`, en el código `5ac174e48957e77041352ffba510bead61c15a8a`. Está **PENDIENTE DE PROBAR EN EL CARRO REAL**. Todas las cifras son **SIMULADAS** salvo las identificadas expresamente como **VALIDADO EN EL CARRO**.

La validación física pendiente comprende M1 a M10 y los bloques A, B1, B2, C y D del [protocolo de prueba física](protocolo_prueba_fisica_autonomo_v2.md), con la [plantilla de registro](pruebas_fisicas_plantilla.csv). El manual v3 sigue vigente.

## Huecos conocidos de v3 (sin corregir)

Los resultados provienen del banco calibrado de 40 corridas por escenario y perfil, conservado en el archivo v4. **SIMULADO**.

- `bloque_sin_eco`: 40 contactos de 40 en reservado, tanto con láser normal como pesimista. La autoridad del láser prevalece aunque coincidan color y detector. Comprobar primero, con el carro detenido y un objeto oscuro, mate o transparente, si el láser devuelve eco. **SIMULADO**.
- Banquito: con láser normal, 12 contactos de 40 en desarrollo y 9 de 40 en reservado. Con láser pesimista, 24 y 15, respectivamente. **SIMULADO**.
- `objeto_bajo` y `bajo_solo_color`: con láser pesimista, entre 18 y 28 contactos de 40 según escena y conjunto. **SIMULADO**.
- `caja_y_pared_al_lado` y `pasillo`: entre 15 y 24 AYUDA de 40 en las combinaciones afectadas. En el pasillo de desarrollo hubo cero AYUDA. **SIMULADO**.

Resultados completos del banco en [fase2_defecto.txt del archivo v4](https://github.com/fernandosegrr/fsi_proyect/blob/archive/feat-autonomo-v4-a200df4/docs/precision/v4/fase2_defecto.txt). Una AYUDA puede coincidir con otro desenlace, por lo que se utiliza la cifra total de AYUDA.

## v4 descartada

La guardia v4 queda descartada para la exposición. En simulación, con la guardia encendida pidió AYUDA en 31 de 40 corridas de `libre_limpio` y en 19 de 40 de `pared_tras_el_final`. Registró 94 de 98 combinaciones con compuerta roja. Su trabajo se conserva en el tag anotado `archive/feat-autonomo-v4-a200df4`.

Commits conservados desde la base v3, sin aplicar a v3:

- `a03d8c5` fijó escenarios y semillas.
- `6f43c42` calibró el banco y midió las referencias.
- `a3a346e` documentó el diagnóstico y los ensayos físicos.
- `777bce9` implementó la guardia experimental y la telemetría de frescura.
- `aaadaaf` corrigió la lectura fuera de límites en rutas de un punto y sincronizó los accesos de la prueba de estrés, además de documentar la decisión.
- `a200df4` registró los sanitizadores en Linux y actualizó el manual de v4.

Los resultados físicos de v3 aún requieren el protocolo indicado arriba. El banco tiene supuestos de frenado, cobertura y eco que necesitan medición en el carro.

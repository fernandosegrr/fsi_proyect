# Familia nueva de escenarios «sin eco» (v6)

**SIMULADO / NO MEDIDO en el carro.** Existe para no ajustar el detector de masa contra un solo escenario (`bloque_sin_eco`, que v5 usó para ajustarse):
12 escenarios nuevos con otros colores, texturas, tamaños, posiciones angulares, con y sin sombra, sobre pisos de otra fotometría, más pisos con manchas
y reflejos de ventana saturados. Se corren con `AV6_FAMILIA=1` (DESARROLLO) o `AV6_FAMILIA=2` (RESERVADO) y entran en las evaluaciones ciegas
(`AV5_CIEGA`, que usa los rangos y colores del RESERVADO).

Semillas: DESARROLLO `6100000 + escenario * 10007 + corrida` (mundo con `semilla ^ 0x5EC0DE01`); RESERVADO `8300000 + ...` (`^ 0x5EC0DE02`).
Lo que se decide con DESARROLLO tiene que valer en RESERVADO (otros colores, otros rangos, otro piso): si mejora en una y no en la otra, se descarta.

| escenario              | tipo     | qué es                                                                                                                         | por qué existe                                                         |
| ---------------------- | -------- | ------------------------------------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------- |
| `se_negro_mate`        | obstáculo | caja negra o muy oscura y mate de 13 a 29 x 13 a 26 cm y 10 a 24 cm de alto (DES: negro, café, azul marino, carbón; RES: verde, morado, rojo oscuro, verde azulado, pardo casi negro); el láser no devuelve eco (reflectancia 0 a 0.003) | el hueco que el encargo señala: objeto oscuro sin eco del láser         |
| `se_pequeno_borde`     | obstáculo | objeto oscuro de 9 a 15 cm en el borde del carril (7 a 16 cm de la línea): solo roza el ancho del chasis; el cono del láser lo alcanza a ratos | posición angular: lo que cae en el tercio lateral de la imagen         |
| `se_alto_con_sombra`   | obstáculo | objeto oscuro y alto (25 a 42 cm) con su sombra suave sobre el piso junto a la base                                             | con sombra: la mancha del piso no es el objeto                          |
| `se_bajo_oscuro`       | obstáculo | objeto oscuro de 2.5 a 4.5 cm de alto: queda casi por debajo del haz del láser (5.5 cm)                                         | tamaño: casi plano para el láser, bajo para la cámara                   |
| `se_rayado`            | obstáculo | objeto con franjas alternas oscuras y medias (4 a 6)                                                                            | textura: sin una masa uniforme                                          |
| `se_piso_oscuro`       | obstáculo | piso oscuro (albedo 0.26 a 0.40) y objeto gris oscuro                                                                           | otro piso: poco contraste absoluto                                      |
| `se_piso_claro`        | obstáculo | piso muy claro (albedo 0.82 a 0.92) y objeto negro                                                                              | otro piso: el mayor contraste                                           |
| `se_transparente`      | límite   | acrílico transparente del color del piso y sin eco                                                                              | LÍMITE FÍSICO: ni láser ni color lo ven; no cuenta para las compuertas |
| `se_poco_contraste`    | límite   | gris apenas más oscuro que el piso (0.78 a 0.88 de su brillo) y sin eco                                                         | LÍMITE: lo mismo                                                        |
| `se_piso_manchas`      | falso    | piso limpio con varias manchas oscuras de borde duro y tono pardo (no hay ningún objeto)                                        | piso con manchas: la masa no debe confundirse con ellas                 |
| `se_reflejo_saturado`  | falso    | reflejo de ventana saturado (los píxeles llegan a 255), mucho más que `falso_reflejo`                                           | reflejos saturados                                                      |
| `se_charco_oscuro`     | falso    | mancha oscura, compacta y parda DENTRO de la línea (piso mojado, café derramado); se puede pasar                                | el negativo difícil: sobre el carril se parece a `se_bajo_oscuro`       |

**Honestidad sobre `se_charco_oscuro`:** una mancha plana oscura exactamente sobre el carril y un objeto de 3 cm oscuro sin eco se ven casi igual para una cámara de
24 x 17 celdas y no los separa ningún sensor del carro (el láser tampoco ve ninguno de los dos). Ningún detector de masa puede ganar en los dos: es la compensación
inevitable entre contactos con objetos oscuros sin eco y falsos esquives. Se reporta como esfuerzo, no como compuerta.

Los límites (`se_transparente`, `se_poco_contraste`) fallan con todas las versiones (v3, v5 y v6) y por eso no entran en las compuertas; se informan para que se vea
dónde termina lo que el carro puede hacer solo.

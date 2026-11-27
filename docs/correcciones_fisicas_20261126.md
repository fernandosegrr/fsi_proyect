# Correcciones físicas del 26 de noviembre de 2026 (`navegador.c` y `ManualFragment.kt`)

Este documento registra las dos correcciones de código aplicadas tras las pruebas físicas del 25–26 de noviembre de 2026 sobre `main` (`b396390`), la evidencia disponible y lo que sigue pendiente de validar.

## 1. Qué cambió exactamente

### `nucleo_c/navegador.c`

1. **Autoridad del láser real (`solo_laser = nav->autoridad_laser && tof_fresco(nav, ahora_ms)`):**
   - En `registrar_cuadro`: cuando `solo_laser` es verdadero, `hay` deja de activarse por consenso visual (`!solo_laser && fuentes == 3 && ...`) y queda gobernado en el centro por el láser (`hay = hay || laser_cerca`, con `(autoridad || nav->tof_paro_activo) && z == ZONA_CENTRO`). También quedan inhibidas bajo `!solo_laser` la activación de `nav->precaucion`, el frenado inmediato por cercanía 3 (`inmediato = 1`) y las marcas de borde cercano lateral (`nav->zona_borde_cerca` en `ZONA_IZQUIERDA` y `ZONA_DERECHA`). Para el vistazo lateral (`una`), se permite dispararlo aun con `fuentes_vision[zl] == 3` cuando `solo_laser` está activo.
   - En `costado_a_la_vista`: si `nav->autoridad_laser && tof_fresco(nav, ahora_ms)`, devuelve `-1` para no prolongar el tramo paralelo por visión lateral.
   - En `tick_reincorporando`: cuando `solo_laser` es verdadero, `borde_visible` permanece en `0` y se omiten los chequeos `entrada_ocupada_por_color(..., 0)` y `entrada_ocupada_por_color(..., 1)` (`zo = solo_laser ? -1 : ...`).
2. **No abortar `PARALELO` por el borde del propio objeto:**
   - En `esq_zona_que_bloquea`: mientras el navegador está en `NAV_ESQUIVANDO` y `NAV_FASE_PARALELO`, se ignora `zona_borde_cerca` en la zona donde quedó el obstáculo que se rodea (`zona_objeto = (nav->lado > 0) ? ZONA_IZQUIERDA : ZONA_DERECHA`).
3. **No saltar directo a `SIGUIENDO` al volver de `PARALELO` o `REINCORPORANDO`:**
   - En `tick_detenido`: se añade `!nav->volver_a_reinc` a la condición que permite volver a `NAV_SIGUIENDO` mediante `libre_confirmado`.
4. **Extensión de recta final con `override_largo_mm`:**
   - En `extender_recta_final`, `calcular_fin_obstaculo` y `reinc_terminar`: se elimina el bloqueo por `nav->override_largo_mm == 0` para permitir extender la última recta durante un rodeo, actualizando `override_largo_mm` si `ruta_largo_mm` lo supera y restaurando `ruta_largo_original_mm` al recortar la extensión si el carro vuelve a la línea antes del destino dibujado.

### `app_android/app/src/main/java/com/fsi/carro/ManualFragment.kt`

1. En `onStart`: copia `ajustes.usarGiroscopio` y `ajustes.usarRumbo` a `SesionCarro.logica` antes de `pedirPerfil`, y llama `logica.rumbo(logica.rumboActivo)`.
2. En `onStop`: además de `parar()`, apaga `RUMBO` con `SesionCarro.logica.rumbo(false)`.
3. En el evento `MotionEvent.ACTION_DOWN` del botón `FORWARD`: llama `SesionCarro.logica.rumbo(SesionCarro.logica.rumboActivo)` antes de `presionar(comando)`.

## 2. Qué significa «autoridad del láser real» en el nuevo código

En las palabras del diff de `nucleo_c/navegador.c`, cuando `nav->autoridad_laser` está activo y el dato del ToF es reciente (`tof_fresco(nav, ahora_ms)`), la variable `solo_laser` vale `1` y:
- «con `autoridad_laser`: solo el láser confirma el centro» (`hay = hay || laser_cerca` en `ZONA_CENTRO`, mientras `hay` visual queda bloqueado por `!solo_laser`).
- Se desactivan por `!solo_laser` la precaución visual, el disparo inmediato por cercanía 3, las marcas de borde cercano lateral (`borde_izq`/`borde_der`), la extensión por `costado_a_la_vista` y los bloqueos por `entrada_ocupada_por_color` en `tick_reincorporando`.
- La cámara conserva la capacidad de iniciar un vistazo lateral (`una` permite `solo_laser || nav->fuentes_vision[zl] != 3`).

## 3. Por qué (fallas físicas observadas)

- **Contacto con mochila negra (42×14 cm) en `main` original:** al abortar `PARALELO` por cámara lateral y girar 135°, el carro hizo contacto con la mochila (corrida 3, fallo grave con contacto).
- **`AYUDA` causa 14 en mochila negra:** en la corrida 2 con `main` original se produjeron 2 rechazos de `aprenderPiso` / bloqueo que llevaron a `AYUDA` causa 14.
- **`AYUDA` causa 2 con objeto bajo de 6 cm en piso oscuro:** en la corrida 1 a 90 cm (antes de la corrección), los chequeos solo-color dispararon `AYUDA` causa 2 sin contacto.
- **Modo Manual sin sincronizar giroscopio/rumbo:** `ManualFragment.kt` no aplicaba `usarGiroscopio`/`usarRumbo` al entrar ni apagaba `RUMBO` al salir.

## 4. Evidencia disponible (`n` y resultados)

> Nota: las corridas buenas son posteriores a las correcciones del 26-nov, se ejecutaron en los **mismos escenarios donde ocurrió la falla** y con **`n` pequeño (`n=3` por condición)**; no constituyen una validación independiente completa.

- **Objeto negro mate (mochila 42×14 cm), dinámico (**VALIDADO EN EL CARRO** en el mismo escenario):**
  - Con `main` original (`n=3`): r1 terminó sin tocar, r2 `AYUDA` causa 14, r3 fallo grave con contacto.
  - Con la corrección (`n=3`): 3/3 terminado, 0 contactos, 0 `AYUDA`, reincorporación a 1–2 cm de la línea.
- **Objetos bajos a 90 cm, objeto de 6 cm (**VALIDADO EN EL CARRO** en el mismo escenario):**
  - `n=3`: r1 `AYUDA` causa 2 sin contacto (antes de la corrección); r2 y r3 terminados con 0 contactos (después de la corrección).
  - (Objeto de 2 cm a 90 cm: `n=3`, 3/3 terminado, 0 contactos.)

## 5. Qué NO está probado y límites conocidos

- **`RUMBO:ON` en modo Manual (`ManualFragment.kt`):** **PENDIENTE** — no se ha probado físicamente en el carro.
- **Pruebas que siguen SIMULADO o NO MEDIDO:** M8 en modo Manual, M2 (muebles con patas), objeto con 0 % de eco IR, M4, M7, M9 y bloques C y D (luz cambiante, ruta en L, cuadrado).
- **Límite conocido de `aprenderPiso` (no modificado):** `aprenderPiso` rechaza el arranque con obstáculos a 40–80 cm (`descartadoPct > 5`, medido en 18 % > 5 % a 40 cm). Por ello todas las rutas deben arrancar con al menos 100 cm de piso libre delante.

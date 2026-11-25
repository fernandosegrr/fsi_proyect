# Mapa general del sistema (una página)

Tres nodos, **una sola red WiFi** y **tres enlaces**. El teléfono decide y manda; el carro obedece y se protege solo; la cámara solo ve. Este mapa sirve para ubicar cualquier pregunta antes de ir al subsistema que la contesta.

```
        Red WiFi propia «CarroFSI» · 2.4 GHz · sin internet · la crea el carro
 ┌───────────────────────────┐  (1) WebSocket /ws ┌───────────────────────────┐
 │ TELÉFONO ANDROID          │◀──────────────────▶│ CARRO: ESP32 DevKit       │
 │ app Kotlin + núcleo C99   │  órdenes ▶  ◀ SENS │ 192.168.4.1               │
 │ decide, ve y navega       │                    │ punto de acceso WiFi      │
 │ cliente 192.168.4.x       │                    │ motores · IMU · láser     │
 └─────────────▲─────────────┘                    └─────────────▲─────────────┘
               │ (3) video: /stream, /raw o UDP :4210           │ (2) la cámara se
               │                                                │ une a CarroFSI
 ┌─────────────┴─────────────┐                                  │ como cliente
 │ CÁMARA: Freenove          │ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─┘
 │ ESP32-WROVER + sensor     │
 │ GC0308 · 192.168.4.2      │   Energía: tres fuentes separadas (powerbank 1,
 └───────────────────────────┘   pilas 4×AA y powerbank 2)
```

## Quién manda qué, y por dónde

| De → a | Qué viaja | Por dónde | Cada cuánto |
|---|---|---|---|
| Teléfono → carro | Órdenes: `FORWARD`, `RUMBO`, `GIRO`, `STOP` | Enlace (1) WebSocket `/ws` en `192.168.4.1` | En manual, un latido cada 250 ms mientras se pulsa un botón |
| Carro → teléfono | Tramas `SENS` (láser e IMU) y avisos `WD:`, `TOF_PARO:` | El mismo WebSocket | `SENS` a 10 Hz |
| Cámara → teléfono | Imágenes: MJPEG `/stream`, crudo `/raw` o celdas por UDP `:4210` | Enlace (3) por la red `CarroFSI`, desde `192.168.4.2` | Unos 12 cuadros por segundo (`240×176`) |
| Cámara → red | Se une como cliente (enlace 2); no manda nada al carro | Punto de acceso del carro | Una vez, al encender |

## Quién decide y quién protege

- **El teléfono decide:** la navegación y la visión corren en la app (núcleo en C99 por JNI). El carro ejecuta lo que llega.
- **El carro se protege solo:** frena con el láser (`TOF_PARO_MM`: `120 mm` en el firmware v1.1 que está en el carro; `150 mm` en el firmware v3 `SEG=3`, solo compilado) y corta los motores si pasan `1,000 ms` sin órdenes (`TIEMPO_WATCHDOG_MS`), sin esperar al teléfono. El detalle y el orden completo están en el subsistema G, página {{pag:G}}.
- **Cada afirmación lleva una etiqueta:** **VALIDADO EN EL CARRO** (solo v1.1, 10-nov-2026), **SIMULADO** (todo el autónomo v3), **NO MEDIDO** o **PENDIENTE**.
- **Cómo usar este manual:** «si te preguntan X, ve a la página Y» está en la página {{pag:indice}} y los términos en el glosario, página {{pag:glosario}}.

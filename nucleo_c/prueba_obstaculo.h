/*
 * prueba_obstaculo.h — Cuentas de la prueba "Obstáculos: cámara": cuánto tarda el carro en frenar y cuántos cm avanza mientras tanto.
 *
 * GUÍA DE LECTURA
 *  1. Una "pasada" es una vez que el carro avanza hacia un objeto y frena. La app mide estos tiempos (ms):
 *       ms_detectar   desde que el cuadro llegó al teléfono hasta que el detector terminó de analizarlo
 *       ms_stop       desde que llegó ESE cuadro hasta que el STOP salió hacia el carro (incluye el ciclo de 50 ms del navegador)
 *       ms_respuesta  desde que salió el STOP hasta que volvió el PONG de un PING mandado justo después (el carro ya procesó el STOP)
 *     y se sabe cuánto tarda el video en llegar al teléfono (latencia_video_ms: ~320 ms con la cámara del carro, 0 con la del teléfono).
 *  2. El retraso entre que el obstáculo aparece frente a la cámara y el STOP llega al carro es:
 *         retraso = latencia_video + ms_stop + ms_respuesta / 2        (el STOP tarda la mitad del ida y vuelta en llegar)
 *  3. Mientras pasa ese retraso el carro sigue avanzando: distancia = retraso / ms_por_cm + frenado (de la calibración: 26.3 ms/cm
 *     y 0.9 cm → 38 cm/s). Se devuelve en décimas de cm (por 10) para no usar decimales.
 *  4. El resumen acumula las pasadas: promedio y máximo del retraso y de la distancia.
 */
#ifndef PRUEBA_OBSTACULO_H
#define PRUEBA_OBSTACULO_H

typedef struct {
    int ms_detectar;
    int ms_stop;
    int ms_respuesta;
    int latencia_video_ms;
} PasadaObstaculo;

typedef struct {
    int  n;                 /* pasadas anotadas */
    long suma_retraso_ms;
    int  max_retraso_ms;
    long suma_cm_x10;
    int  max_cm_x10;
} ResumenPrueba;

/* Retraso total (ms) obstáculo → STOP en el carro. Los tiempos negativos se tratan como 0. */
int po_retraso_ms(const PasadaObstaculo *p);

/* Décimas de cm que avanza el carro durante `retraso_ms`: retraso * 10 / ms_por_cm_x10 ... en décimas de cm, más el frenado (mm).
 * ms_por_cm_x10 de 0 o negativo → -1 (calibración no válida). */
int po_cm_x10(int retraso_ms, int ms_por_cm_x10, int frenado_mm);

void po_resumen_iniciar(ResumenPrueba *r);
void po_resumen_agregar(ResumenPrueba *r, int retraso_ms, int cm_x10);

/* Promedios (0 si no hay pasadas). */
int po_resumen_retraso_prom_ms(const ResumenPrueba *r);
int po_resumen_cm_prom_x10(const ResumenPrueba *r);

#endif /* PRUEBA_OBSTACULO_H */

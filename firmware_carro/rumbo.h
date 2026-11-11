/*
 * rumbo.h — Sostener el RUMBO al avanzar con el giroscopio (J16), en C puro (sin Arduino, sin malloc, solo enteros).
 *
 * Sin esto el carro avanza a ciegas: si un lado empuja un poco más, se curva (en las pruebas reales de J14, 5 a 20 cm de desvío en 150 cm).
 * Con RUMBO encendido, mientras el carro AVANZA, en cada muestra nueva del giroscopio se compara el ángulo con el objetivo y se reparte la potencia
 * entre los dos lados con un control proporcional-integral: si el carro se tuerce a la derecha, baja un poco el lado izquierdo y sube el derecho
 * (sin pasar del tope), y al revés. No mide distancia ni ve un desplazamiento de lado sin giro: solo sostiene el rumbo.
 *
 * Mensajes (los atiende carro.c; no son un movimiento: no renuevan el watchdog ni cambian el comando actual):
 *   RUMBO:ON    enciende y contesta ACK_RUMBO:1. Al arrancar el firmware y al conectarse o desconectarse el cliente queda APAGADO.
 *   RUMBO:OFF   apaga y contesta ACK_RUMBO:0: el avance vuelve a ser exactamente el de antes (potencias fijas del perfil).
 *   RUMBO:<n>   objetivo del avance EN CURSO, en décimas de grado con la convención del giroscopio (+ izquierda), el mismo ángulo que va en SENS.
 *               Sin respuesta. Al empezar cada FORWARD el objetivo es el ángulo de ese momento (sostener); la app puede mandar otro para volver
 *               a la línea. Se recorta a ±RUMBO_MAX_OBJETIVO_X10 del ángulo con el que empezó el avance. Sin avance en curso se ignora.
 *   RUMBO?      contesta RUMBO:<encendido>,<en marcha>,<objetivo x10>,<corrección en milésimas>,<aprendido en milésimas>.
 * Si el giroscopio no está listo, o el error pasa de RUMBO_ERROR_MAX_X10 (el carro chocó o lo levantaron), no corrige: potencias fijas como siempre.
 * Lo aprendido (el término integral, es decir el desbalance de los lados) pasa al siguiente avance; vive en RAM, no en la memoria no volátil.
 */
#ifndef RUMBO_H
#define RUMBO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int      encendido;       /* RUMBO:ON */
    int      en_marcha;       /* 1 mientras sostiene un avance */
    int32_t  inicio_x10;      /* ángulo del giroscopio al empezar el avance */
    int32_t  objetivo_x10;    /* rumbo que se sostiene (convención del giroscopio, + izquierda) */
    int32_t  integral;        /* error acumulado: décimas de grado x ms */
    int32_t  aprendido_mil;   /* término integral al terminar el último avance (milésimas de potencia) */
    uint32_t t_ms;            /* hora de la última corrección */
    int      hay_t;           /* 1 si t_ms vale */
    int      corr_mil;        /* última corrección (milésimas; + = girar a la izquierda) */
    int      fuera;           /* 1 si la última muestra pasó de RUMBO_ERROR_MAX_X10 (no se corrigió) */
} Rumbo;

/* Apagado, sin avance en curso y sin nada aprendido. */
void rumbo_iniciar(Rumbo *r);

/* Empieza un avance: el objetivo es el ángulo actual y el integral parte de lo aprendido. Solo si está encendido (si no, no hace nada). */
void rumbo_empezar(Rumbo *r, int32_t angulo_x10, uint32_t ahora_ms);

/* Terminó el avance (STOP, otro movimiento, watchdog): guarda lo aprendido. Sin avance en curso no hace nada. */
void rumbo_terminar(Rumbo *r);

/* Objetivo nuevo para el avance en curso (RUMBO:<n>). Devuelve 1 si se aplicó (recortado si hizo falta) y 0 si no hay avance en curso. */
int  rumbo_poner_objetivo(Rumbo *r, int32_t objetivo_x10);

/* Una muestra del giroscopio: devuelve la corrección en milésimas de potencia (+ = girar a la izquierda), entre -RUMBO_MAX_MIL y RUMBO_MAX_MIL.
 * 0 si no hay avance en curso o si el error es absurdo (r->fuera = 1). */
int  rumbo_corregir(Rumbo *r, int32_t angulo_x10, uint32_t ahora_ms);

/* Reparte la corrección en las potencias de los lados: con + el izquierdo baja y el derecho sube (y al revés). Ninguno pasa de `tope`: si uno se pasa,
 * los dos se escalan para conservar la proporción. Nunca negativos. */
void rumbo_repartir(int corr_mil, int base_izq, int base_der, int tope, int *izq, int *der);

/* Lee un objetivo de texto ("-1234"): signo opcional y de 1 a 6 cifras. Devuelve 1 y lo deja en *valor, o 0 si el texto no sirve. */
int  rumbo_leer_numero(const char *texto, int32_t *valor);

#ifdef __cplusplus
}
#endif

#endif /* RUMBO_H */

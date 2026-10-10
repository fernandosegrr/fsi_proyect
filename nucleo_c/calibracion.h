/*
 * calibracion.h — Medidas físicas del carro que convierten distancia y giro
 * en tiempo (el carro navega por tiempo, sin encoders ni GPS).
 *
 * Hay que medirlas con el carro real, en el piso donde va a andar y con la
 * batería cargada: cambian con el peso, la superficie y el voltaje.
 */
#ifndef CALIBRACION_H
#define CALIBRACION_H

/* REEMPLAZAR con los valores medidos: milisegundos de motores a velocidad
 * máxima para avanzar 1 cm en línea recta. */
#define MS_POR_CM     50

/* REEMPLAZAR con los valores medidos: milisegundos de pivote para girar 1
 * grado. */
#define MS_POR_GRADO  10

#endif /* CALIBRACION_H */

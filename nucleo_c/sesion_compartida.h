/*
 * sesion_compartida.h — Un recurso que varias pantallas comparten (el video de la cámara, la conexión con el carro).
 *
 * GUÍA DE LECTURA
 *  1. La cámara del carro atiende UN solo cliente de video a la vez. Por eso el video se abre UNA sola vez y lo comparten las pantallas
 *     que lo necesitan (Inicio, Manual, Visión, Ejecutar ruta). Este módulo es la "cabeza" de esa decisión: cuenta quién usa el recurso y
 *     dice cuándo hay que abrirlo, cerrarlo o volver a abrirlo. No abre nada por sí mismo: devuelve una ACCIÓN y la app la ejecuta.
 *  2. Estados: CERRADA → ABRIENDO → ABIERTA; si falla: FALLO (se avisa a la pantalla y se reintenta sola cada SES_REINTENTO_MS).
 *  3. USUARIOS: cada pantalla tiene un número (0 a 15). Entrar dos veces con el mismo número cuenta UNA vez (si una pantalla se
 *     recrea al girar el teléfono no se duplica). Salir sin haber entrado no hace nada.
 *  4. GRACIA: cuando sale el ÚLTIMO usuario, el recurso no se cierra al instante: espera SES_GRACIA_CIERRE_MS. Así al pasar de una
 *     pantalla a otra (o al girar el teléfono) el video no se corta y se vuelve a abrir. Si alguien entra dentro de la gracia, el cierre
 *     se cancela y el mismo recurso sigue vivo.
 *  5. Todo recibe la hora `ahora_ms` (un reloj en milisegundos que la app da). La comparación de horas aguanta que el reloj dé la vuelta.
 *  6. Solo enteros, sin malloc ni recursión.
 */
#ifndef SESION_COMPARTIDA_H
#define SESION_COMPARTIDA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SES_MAX_USUARIOS          16     /* números de usuario válidos: 0 a 15 */
#define SES_GRACIA_CIERRE_MS    1500     /* espera antes de cerrar cuando sale el último usuario */
#define SES_REINTENTO_MS        3000     /* cada cuánto se vuelve a abrir solo un recurso en FALLO (el video de la cámara y el control del carro) */

typedef enum {
    SES_CERRADA = 0,    /* nadie lo usa y no está abierto */
    SES_ABRIENDO = 1,   /* se pidió abrirlo y todavía no hay señal de que funcione */
    SES_ABIERTA = 2,    /* funcionando */
    SES_FALLO = 3       /* se abrió pero falló (o no llegó nada): se reintenta */
} EstadoSesion;

/* Lo que la app debe hacer tras llamar a una función del módulo. */
typedef enum {
    SES_NADA = 0,
    SES_ABRIR = 1,      /* abrir el recurso (estaba cerrado) */
    SES_CERRAR = 2,     /* cerrarlo (pasó la gracia y nadie lo usa) */
    SES_REABRIR = 3     /* cerrarlo y abrirlo de nuevo (reintento) */
} AccionSesion;

typedef struct {
    EstadoSesion estado;
    uint32_t usuarios;          /* un bit por usuario */
    int      cierre_pendiente;  /* 1 = hay un cierre programado */
    uint32_t t_cierre_ms;       /* hora en que se cierra si nadie entra antes */
    uint32_t t_reintento_ms;    /* hora del siguiente reintento automático (solo en FALLO) */
    int      intentos;          /* cuántas veces se abrió desde que está en uso */
    int      fallos;            /* cuántos fallos seguidos desde la última vez que funcionó */
} Sesion;

/* Deja la sesión cerrada y sin usuarios. */
void ses_iniciar(Sesion *s);

/* Entra un usuario. Devuelve SES_ABRIR si el recurso estaba cerrado (hay que abrirlo); SES_NADA si ya existe (se comparte)
 * o el número no es válido. Cancela un cierre programado. */
AccionSesion ses_entrar(Sesion *s, int usuario, uint32_t ahora_ms);

/* Sale un usuario. Si era el último, programa el cierre para dentro de SES_GRACIA_CIERRE_MS. Siempre devuelve SES_NADA
 * (el cierre llega después, con ses_tick). */
AccionSesion ses_salir(Sesion *s, int usuario, uint32_t ahora_ms);

/* Hay que llamarla seguido (cada ~250 ms). Devuelve SES_CERRAR si venció la gracia y nadie entró, o SES_REABRIR si el recurso
 * está en FALLO y ya toca el reintento automático. */
AccionSesion ses_tick(Sesion *s, uint32_t ahora_ms);

/* El recurso funciona (llegó un cuadro, se abrió la conexión): pasa a ABIERTA y se olvidan los fallos. Si estaba CERRADA se ignora
 * (un aviso atrasado de algo que ya se cerró). */
void ses_funciona(Sesion *s);

/* El recurso falló o dejó de dar señal: pasa a FALLO y programa el reintento. Si estaba CERRADA se ignora. */
void ses_fallo(Sesion *s, uint32_t ahora_ms);

/* El usuario tocó «Reintentar»: si hay usuarios, devuelve SES_REABRIR de inmediato (y la sesión queda ABRIENDO); si no, SES_NADA. */
AccionSesion ses_reintentar(Sesion *s, uint32_t ahora_ms);

/* Cuántos usuarios hay y si el número dado está dentro. */
int ses_cuenta_usuarios(const Sesion *s);
int ses_tiene_usuario(const Sesion *s, int usuario);

/* Texto corto del estado para el registro ("CERRADA", "ABRIENDO", "ABIERTA", "FALLO"). */
const char *ses_texto_estado(EstadoSesion estado);

#ifdef __cplusplus
}
#endif

#endif /* SESION_COMPARTIDA_H */

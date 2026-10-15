/*
 * SesionNativa.kt — Declara las funciones en C de la sesión compartida (nucleo_c/sesion_compartida.c).
 *
 * GUÍA DE LECTURA
 *  1. Hay DOS recursos compartidos: RECURSO_VIDEO (el video de la cámara del carro, que atiende un solo cliente) y RECURSO_CONTROL (la
 *     conexión WebSocket con la DevKit). Cada pantalla que los necesita ENTRA con su número de usuario y SALE al irse.
 *  2. Cada función devuelve la ACCIÓN que hay que hacer (ACCION_*); el módulo en C no abre ni cierra nada: lo hace SesionVideo / SesionCarro.
 *  3. Solo se llama desde el hilo principal.
 */
package com.fsi.carro

object SesionNativa {

    init {
        System.loadLibrary("carro_nativo")
    }

    const val RECURSO_VIDEO = 0
    const val RECURSO_CONTROL = 1

    // Los usuarios (cada pantalla un número distinto; ver sesion_compartida.h).
    const val USUARIO_INICIO = 0
    const val USUARIO_MANUAL = 1
    const val USUARIO_VISION = 2
    const val USUARIO_RUTA = 3
    const val USUARIO_RUTAS = 4          // el editor de rutas y su historial (usa la conexión, no el video)
    const val USUARIO_CALIBRACION = 5
    const val USUARIO_DIAGNOSTICO = 6    // la prueba de latencia con y sin video
    const val USUARIO_MAS = 7
    const val USUARIO_DETECCION = 8      // la detección que dibuja Manual sobre el video («Ver detección»)

    // Acciones (AccionSesion en sesion_compartida.h).
    const val ACCION_NADA = 0
    const val ACCION_ABRIR = 1
    const val ACCION_CERRAR = 2
    const val ACCION_REABRIR = 3

    // Estados (EstadoSesion).
    const val CERRADA = 0
    const val ABRIENDO = 1
    const val ABIERTA = 2
    const val FALLO = 3

    // Posiciones del arreglo de estado().
    const val E_ESTADO = 0
    const val E_USUARIOS = 1
    const val E_INTENTOS = 2
    const val E_FALLOS = 3

    external fun entrar(recurso: Int, usuario: Int, ahoraMs: Long): Int
    external fun salir(recurso: Int, usuario: Int, ahoraMs: Long): Int
    external fun tick(recurso: Int, ahoraMs: Long): Int
    external fun funciona(recurso: Int)
    external fun fallo(recurso: Int, ahoraMs: Long)
    external fun reintentar(recurso: Int, ahoraMs: Long): Int
    external fun estado(recurso: Int): IntArray
    external fun reiniciar()
}

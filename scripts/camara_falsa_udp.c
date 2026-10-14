/*
 * camara_falsa_udp.c — La parte UDP (modo CELDAS) de una cámara del carro falsa, para probar la app sin el carro.
 *
 * GUÍA DE LECTURA
 *  1. Hace lo mismo que camara_prueba.ino en el puerto UDP 4210: contesta "HOLA" con "HOLA-OK celdas" y recuerda a quién (dirección y puerto, sin IP fija); contesta
 *     "PING n" con "PONG n"; y, mientras haya un cliente que dijo HOLA en los últimos 5 s, manda un paquete de celdas por cuadro (formato de nucleo_c/paquete_celdas.c).
 *  2. Las celdas salen de una escena sintética (nucleo_c/pisos_sinteticos.c: piso liso con una caja que se acerca cada 12 s) promediada con el MISMO obstaculos_promediar
 *     que usa el firmware de la cámara. No hay ninguna foto: solo números.
 *  3. Se compila para el teléfono con el NDK (scripts/probar_modos_video.ps1 lo hace) y corre allí como programa de línea de comandos (por adb shell), porque el teléfono y la
 *     PC no comparten una red UDP por el cable USB. En la PC también se puede compilar con gcc para una prueba rápida.
 *  4. Uso:  camara_falsa_udp [puerto=4210] [fps=10] [segundos=300]
 */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "obstaculos.h"
#include "paquete_celdas.h"
#include "pisos_sinteticos.h"

static long long ahora_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

int main(int argc, char **argv)
{
    int puerto = (argc > 1) ? atoi(argv[1]) : 4210;
    int fps = (argc > 2) ? atoi(argv[2]) : 10;
    int segundos = (argc > 3) ? atoi(argv[3]) : 300;
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in propia, cliente;
    int hay_cliente = 0;
    long long ultimo_hola = 0, inicio = ahora_ms(), proximo_cuadro = 0;
    unsigned int numero = 0;
    static unsigned char rgb[PS_ANCHO * PS_ALTO * 3];
    static unsigned char celdas[PAQ_MAX_COLUMNAS * PAQ_MAX_FILAS * 3];
    static unsigned char paquete[PAQ_MAX_TOTAL];
    struct timeval espera = { 0, 2000 };

    if (fps < 1) {
        fps = 1;
    }
    memset(&propia, 0, sizeof propia);
    propia.sin_family = AF_INET;
    propia.sin_addr.s_addr = htonl(INADDR_ANY);
    propia.sin_port = htons((unsigned short)puerto);
    if (s < 0 || bind(s, (struct sockaddr *)&propia, sizeof propia) != 0) {
        printf("camara_falsa_udp: no se pudo abrir el puerto %d (%s)\n", puerto, strerror(errno));
        return 1;
    }
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &espera, sizeof espera);
    printf("camara_falsa_udp: escuchando en el puerto UDP %d, %d fps, %d s\n", puerto, fps, segundos);
    fflush(stdout);

    while (ahora_ms() - inicio < (long long)segundos * 1000LL) {
        char buf[64];
        struct sockaddr_in origen;
        socklen_t largo_origen = sizeof origen;
        int n = (int)recvfrom(s, buf, sizeof buf - 1, 0, (struct sockaddr *)&origen, &largo_origen);
        long long ahora = ahora_ms();

        if (n > 0) {
            buf[n] = 0;
            if (strncmp(buf, "HOLA", 4) == 0) {
                if (!hay_cliente) {
                    printf("camara_falsa_udp: cliente %s:%d\n", inet_ntoa(origen.sin_addr), (int)ntohs(origen.sin_port));
                    fflush(stdout);
                }
                cliente = origen;
                hay_cliente = 1;
                ultimo_hola = ahora;
                sendto(s, "HOLA-OK celdas", 14, 0, (struct sockaddr *)&origen, largo_origen);
            } else if (strncmp(buf, "PING", 4) == 0) {
                char resp[64];
                int m = snprintf(resp, sizeof resp, "PONG%s", buf + 4);
                sendto(s, resp, (size_t)m, 0, (struct sockaddr *)&origen, largo_origen);
            }
        }
        if (hay_cliente && ahora - ultimo_hola > 5000) {
            hay_cliente = 0;
        }
        if (hay_cliente && ahora >= proximo_cuadro) {
            EscenaSintetica e;
            DatosPaquete d;
            int columnas = 0, filas = 0, largo;
            double fase = (double)((ahora - inicio) % 12000) / 1000.0;   /* 0 a 12 s */

            memset(&e, 0, sizeof e);
            e.piso = PS_LISO;
            e.sigma = 3;
            e.exposicion_pct = 100;
            e.semilla = (unsigned long)numero;
            if (fase > 5.0) {   /* la caja entra y se acerca */
                e.objeto = PS_CAJA;
                e.obj_x = 80;
                e.obj_y = 30 + (int)((fase - 5.0) * 17.0);
            }
            ps_dibujar(&e, rgb);
            if (obstaculos_promediar(rgb, PS_ANCHO, PS_ALTO, celdas, (int)sizeof celdas, &columnas, &filas) == OBS_OK) {
                d.numero = ++numero;
                d.ms_captura = 12;   /* una captura+armado inventados de 12 ms */
                d.ancho = PS_ANCHO;
                d.alto = PS_ALTO;
                d.columnas = columnas;
                d.filas = filas;
                largo = paquete_armar(paquete, (int)sizeof paquete, &d, celdas);
                if (largo > 0) {
                    sendto(s, paquete, (size_t)largo, 0, (struct sockaddr *)&cliente, sizeof cliente);
                }
            }
            proximo_cuadro = ahora + 1000 / fps;
        }
    }
    printf("camara_falsa_udp: fin (%u paquetes de celdas mandados)\n", numero);
    return 0;
}

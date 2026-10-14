# Módulos en C estándar del núcleo (calibración, trayectoria, obstáculos, fusión, movimiento visual y navegador): sin librerías externas ni -lm.
#   make test   compila y ejecuta las pruebas de los módulos
#   make clean  borra los ejecutables
CC     = gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -Werror

all: pruebas_calibracion pruebas_trayectoria pruebas_obstaculos pruebas_piso pruebas_fusion pruebas_movimiento pruebas_navegador

pruebas_calibracion: pruebas_calibracion.c calibracion.c calibracion.h
	$(CC) $(CFLAGS) -o $@ pruebas_calibracion.c calibracion.c

pruebas_trayectoria: pruebas_trayectoria.c trayectoria.c trayectoria.h calibracion.c calibracion.h
	$(CC) $(CFLAGS) -o $@ pruebas_trayectoria.c trayectoria.c calibracion.c

pruebas_obstaculos: pruebas_obstaculos.c obstaculos.c obstaculos.h
	$(CC) $(CFLAGS) -o $@ pruebas_obstaculos.c obstaculos.c

pruebas_piso: pruebas_piso.c obstaculos.c obstaculos.h
	$(CC) $(CFLAGS) -o $@ pruebas_piso.c obstaculos.c

pruebas_fusion: pruebas_fusion.c fusion.c fusion.h obstaculos.c obstaculos.h
	$(CC) $(CFLAGS) -o $@ pruebas_fusion.c fusion.c obstaculos.c

pruebas_movimiento: pruebas_movimiento.c movimiento_visual.c movimiento_visual.h
	$(CC) $(CFLAGS) -o $@ pruebas_movimiento.c movimiento_visual.c

pruebas_navegador: pruebas_navegador.c simulador_navegador.c simulador_navegador.h navegador.c navegador.h trayectoria.c trayectoria.h obstaculos.h calibracion.c calibracion.h
	$(CC) $(CFLAGS) -o $@ pruebas_navegador.c simulador_navegador.c navegador.c trayectoria.c calibracion.c

test: all
	./pruebas_calibracion
	./pruebas_trayectoria
	./pruebas_obstaculos
	./pruebas_piso
	./pruebas_fusion
	./pruebas_movimiento
	./pruebas_navegador

clean:
	rm -f pruebas_calibracion pruebas_calibracion.exe pruebas_trayectoria pruebas_trayectoria.exe pruebas_obstaculos pruebas_obstaculos.exe pruebas_piso pruebas_piso.exe pruebas_fusion pruebas_fusion.exe pruebas_movimiento pruebas_movimiento.exe pruebas_navegador pruebas_navegador.exe

.PHONY: all test clean

# Módulos en C estándar del núcleo (trayectoria, obstáculos y navegador): sin librerías externas ni -lm.
#   make test   compila y ejecuta las pruebas de los tres módulos
#   make clean  borra los ejecutables
CC     = gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -Werror

all: pruebas_trayectoria pruebas_obstaculos pruebas_navegador

pruebas_trayectoria: pruebas_trayectoria.c trayectoria.c trayectoria.h calibracion.h
	$(CC) $(CFLAGS) -o $@ pruebas_trayectoria.c trayectoria.c

pruebas_obstaculos: pruebas_obstaculos.c obstaculos.c obstaculos.h
	$(CC) $(CFLAGS) -o $@ pruebas_obstaculos.c obstaculos.c

pruebas_navegador: pruebas_navegador.c simulador_navegador.c simulador_navegador.h navegador.c navegador.h trayectoria.c trayectoria.h obstaculos.h calibracion.h
	$(CC) $(CFLAGS) -o $@ pruebas_navegador.c simulador_navegador.c navegador.c trayectoria.c

test: all
	./pruebas_trayectoria
	./pruebas_obstaculos
	./pruebas_navegador

clean:
	rm -f pruebas_trayectoria pruebas_trayectoria.exe pruebas_obstaculos pruebas_obstaculos.exe pruebas_navegador pruebas_navegador.exe

.PHONY: all test clean

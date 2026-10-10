# Módulos en C estándar del núcleo (trayectoria y obstáculos): sin librerías externas ni -lm.
#   make test   compila y ejecuta las pruebas de los dos módulos
#   make clean  borra los ejecutables
CC     = gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -Werror

all: pruebas_trayectoria pruebas_obstaculos

pruebas_trayectoria: pruebas_trayectoria.c trayectoria.c trayectoria.h calibracion.h
	$(CC) $(CFLAGS) -o $@ pruebas_trayectoria.c trayectoria.c

pruebas_obstaculos: pruebas_obstaculos.c obstaculos.c obstaculos.h
	$(CC) $(CFLAGS) -o $@ pruebas_obstaculos.c obstaculos.c

test: all
	./pruebas_trayectoria
	./pruebas_obstaculos

clean:
	rm -f pruebas_trayectoria pruebas_trayectoria.exe pruebas_obstaculos pruebas_obstaculos.exe

.PHONY: all test clean

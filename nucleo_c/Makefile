# Módulo de trayectoria (C estándar, sin librerías externas ni -lm).
#   make test   compila y ejecuta las pruebas
#   make clean  borra el ejecutable
CC     = gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -Werror

pruebas_trayectoria: pruebas_trayectoria.c trayectoria.c trayectoria.h calibracion.h
	$(CC) $(CFLAGS) -o $@ pruebas_trayectoria.c trayectoria.c

test: pruebas_trayectoria
	./pruebas_trayectoria

clean:
	rm -f pruebas_trayectoria pruebas_trayectoria.exe

.PHONY: test clean

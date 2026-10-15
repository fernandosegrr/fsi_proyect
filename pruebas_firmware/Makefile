# Pruebas de PC de la lógica del carro (sin placa).
#   make        compila pruebas_carro
#   make test   compila y ejecuta, y comprueba por hash la pagina web del carro (scripts/verificar_pagina.py)
#   make clean  borra el ejecutable
# carro.c se compila SIN modificar, desde la carpeta del sketch.

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Wpedantic -Werror -I../firmware_carro
FUENTES = pruebas_carro.c ../firmware_carro/carro.c

pruebas_carro: $(FUENTES) ../firmware_carro/carro.h ../firmware_carro/config.h
	$(CC) $(CFLAGS) -o $@ $(FUENTES)

test: pruebas_carro
	./pruebas_carro
	python ../scripts/verificar_pagina.py

clean:
	rm -f pruebas_carro pruebas_carro.exe

.PHONY: test clean

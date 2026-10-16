# Pruebas de PC de la lógica del carro (sin placa).
#   make        compila pruebas_carro
#   make test   compila y ejecuta, y comprueba por hash la pagina web del carro (scripts/verificar_pagina.py)
#   make clean  borra el ejecutable
# carro.c se compila SIN modificar, desde la carpeta del sketch.
# Además hay una segunda prueba, pruebas_escalonado, que compila lo mismo con -DARRANQUE_ESCALONADO_MS=40 (el arranque escalonado de config.h, apagado por defecto).

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Wpedantic -Werror -I../firmware_carro
FUENTES = pruebas_carro.c ../firmware_carro/carro.c

pruebas_carro: $(FUENTES) ../firmware_carro/carro.h ../firmware_carro/config.h
	$(CC) $(CFLAGS) -o $@ $(FUENTES)

pruebas_escalonado: $(FUENTES) ../firmware_carro/carro.h ../firmware_carro/config.h
	$(CC) $(CFLAGS) -DARRANQUE_ESCALONADO_MS=40 -o $@ $(FUENTES)

test: pruebas_carro pruebas_escalonado
	./pruebas_carro
	./pruebas_escalonado
	python ../scripts/verificar_pagina.py

clean:
	rm -f pruebas_carro pruebas_carro.exe pruebas_escalonado pruebas_escalonado.exe

.PHONY: test clean

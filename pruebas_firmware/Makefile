# Pruebas de PC de la lógica del carro (sin placa).
#   make        compila pruebas_carro
#   make test   compila y ejecuta, y comprueba por hash la pagina web del carro (scripts/verificar_pagina.py)
#   make clean  borra el ejecutable
# carro.c se compila SIN modificar, desde la carpeta del sketch.
# pruebas_camara prueba las decisiones de recuperación de la cámara (../camara_prueba/recuperacion.c).
# Además hay una segunda prueba, pruebas_escalonado, que compila lo mismo con -DARRANQUE_ESCALONADO_MS=40 (el arranque escalonado de config.h, apagado por defecto).

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Wpedantic -Werror -I../firmware_carro -I.
# Los sensores (J11) se compilan SIN modificar, desde la carpeta del sketch; sensores_falsos.c hace de MPU6050 y VL53L0X.
SENSORES = ../firmware_carro/imu.c ../firmware_carro/tof.c ../firmware_carro/giro.c ../firmware_carro/sensores.c sensores_falsos.c
FUENTES = pruebas_carro.c ../firmware_carro/carro.c $(SENSORES)
CABECERAS = ../firmware_carro/carro.h ../firmware_carro/config.h ../firmware_carro/hw_sensores.h ../firmware_carro/imu.h ../firmware_carro/tof.h ../firmware_carro/giro.h ../firmware_carro/sensores.h sensores_falsos.h

pruebas_carro: $(FUENTES) $(CABECERAS)
	$(CC) $(CFLAGS) -o $@ $(FUENTES) -lpthread

pruebas_escalonado: $(FUENTES) $(CABECERAS)
	$(CC) $(CFLAGS) -DARRANQUE_ESCALONADO_MS=40 -o $@ $(FUENTES) -lpthread

pruebas_camara: pruebas_camara.c ../camara_prueba/recuperacion.c ../camara_prueba/recuperacion.h
	$(CC) $(CFLAGS) -I../camara_prueba -o $@ pruebas_camara.c ../camara_prueba/recuperacion.c

pruebas_sensores: pruebas_sensores.c ../firmware_carro/carro.c $(SENSORES) $(CABECERAS)
	$(CC) $(CFLAGS) -o $@ pruebas_sensores.c ../firmware_carro/carro.c $(SENSORES) -lm

test: pruebas_carro pruebas_escalonado pruebas_camara pruebas_sensores
	./pruebas_carro
	./pruebas_sensores
	./pruebas_escalonado
	./pruebas_camara
	python ../scripts/verificar_pagina.py

clean:
	rm -f pruebas_sensores pruebas_sensores.exe pruebas_carro pruebas_carro.exe pruebas_escalonado pruebas_escalonado.exe pruebas_camara pruebas_camara.exe

.PHONY: test clean

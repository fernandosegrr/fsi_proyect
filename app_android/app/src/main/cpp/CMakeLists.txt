# Guía de lectura: compila la biblioteca nativa "carro_nativo".
#  - puente.c         : puente JNI (está aquí, en la app).
#  - trayectoria.c    : módulo de cálculo de rutas, que vive en ../nucleo_c
#                       y se referencia DESDE ALLÁ, sin copiarlo.
#  - obstaculos.c     : detector de obstáculos por color de piso (también en ../nucleo_c).
#  - navegador.c      : cerebro del modo semiautónomo (también en ../nucleo_c).
#  - calibracion.c    : calibración del carro (arranque, ms por cm, frenado, ms por grado), también en ../nucleo_c.
# El resultado es libcarro_nativo.so, que Kotlin carga con System.loadLibrary.

cmake_minimum_required(VERSION 3.22)
project(carro_nativo C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Esta carpeta es app_android/app/src/main/cpp; nucleo_c queda 5 niveles arriba.
get_filename_component(NUCLEO_C "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../nucleo_c" ABSOLUTE)

add_library(carro_nativo SHARED
    puente.c
    ${NUCLEO_C}/trayectoria.c
    ${NUCLEO_C}/obstaculos.c
    ${NUCLEO_C}/navegador.c
    ${NUCLEO_C}/calibracion.c
    ${NUCLEO_C}/fusion.c
)

target_include_directories(carro_nativo PRIVATE ${NUCLEO_C})
target_compile_options(carro_nativo PRIVATE -Wall -Wextra)

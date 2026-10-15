# Guía de lectura: compila la biblioteca nativa "carro_nativo".
#  - puente.c         : puente JNI (está aquí, en la app).
#  - trayectoria.c    : módulo de cálculo de rutas, que vive en ../nucleo_c
#                       y se referencia DESDE ALLÁ, sin copiarlo.
#  - obstaculos.c     : detector de obstáculos por color de piso (también en ../nucleo_c).
#  - navegador.c      : cerebro del modo semiautónomo (también en ../nucleo_c).
#  - calibracion.c    : calibración del carro (arranque, ms por cm, frenado, ms por grado), también en ../nucleo_c.
#  - movimiento_piso.c: flujo del piso (desplazamiento de bloques contra lo esperado), también en ../nucleo_c.
#  - paquete_celdas.c : paquete UDP del modo de video CELDAS, también en ../nucleo_c.
#  - sesion_compartida.c, latencia_video.c : sesión compartida del video y de la conexión, y la comparación de latencia con y sin video (también en ../nucleo_c).
#  - ruta_editor.c, sim_ruta.c, simulador_navegador.c : el editor de rutas y la simulación de rutas sin el carro (también en ../nucleo_c).
#  - corredor.c       : corredor de trayectoria (huella del carro en la imagen), también en ../nucleo_c.
#  - video_stats.c, prueba_obstaculo.c : estadística del video del carro y cuentas de la prueba de obstáculos (también en ../nucleo_c).
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
    ${NUCLEO_C}/movimiento_visual.c
    ${NUCLEO_C}/video_stats.c
    ${NUCLEO_C}/prueba_obstaculo.c
    ${NUCLEO_C}/corredor.c
    ${NUCLEO_C}/cualquiera.c
    ${NUCLEO_C}/movimiento_piso.c
    ${NUCLEO_C}/paquete_celdas.c
    ${NUCLEO_C}/sesion_compartida.c
    ${NUCLEO_C}/latencia_video.c
    ${NUCLEO_C}/ruta_editor.c
    ${NUCLEO_C}/sim_ruta.c
    ${NUCLEO_C}/simulador_navegador.c
)

target_include_directories(carro_nativo PRIVATE ${NUCLEO_C})
target_compile_options(carro_nativo PRIVATE -Wall -Wextra)

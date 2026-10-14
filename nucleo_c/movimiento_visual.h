/*
 * movimiento_visual.h — Cuánto giró el carro entre dos cuadros de la cámara (giro visual).
 *
 * GUÍA DE LECTURA
 *  1. Problema: el carro gira "por tiempo" y no tiene IMU ni encoders, así que un giro de 90 grados puede salir
 *     de 70 u 110 según el piso y la batería. La cámara sí ve girar el cuarto: si el carro gira a la derecha,
 *     lo que se ve se corre hacia la IZQUIERDA en la imagen.
 *  2. Método (proyección de columnas): de cada cuadro se toma SOLO LA MITAD SUPERIOR (el horizonte, paredes y
 *     muebles, se mueve limpio; el piso de abajo brilla y cambia), y se suma el brillo de cada columna: queda
 *     una "huella" de una línea. Se compara la huella del cuadro nuevo con la del anterior corriéndola a
 *     izquierda y derecha y se queda el corrimiento donde más se parecen (con subpíxel simple).
 *  3. Con ~10 a 12 cuadros por segundo y un giro rápido el corrimiento entre cuadros puede ser grande, por eso el
 *     rango de búsqueda es de al menos un tercio del ancho. Si la mejor coincidencia es débil (escena casi
 *     uniforme, o parecida en varias posiciones) se devuelve "sin confianza" y NO se debe sumar.
 *  4. Corrimiento → grados con el campo de visión horizontal (FOV, 60 grados de partida, calibrable): giro =
 *     corrimiento * FOV / ancho (aproximación lineal; el error crece hacia los bordes y se compensa calibrando
 *     el FOV con un giro de 90 grados medido, ver cal_ajustar_fov en calibracion.h).
 *  5. Signo: el giro del carro es POSITIVO a la derecha (igual que el navegador); un corrimiento del contenido
 *     hacia la izquierda de la imagen es un giro a la derecha.
 *  Solo enteros, sin malloc, sin recursión; los errores son códigos de retorno.
 */
#ifndef MOVIMIENTO_VISUAL_H
#define MOVIMIENTO_VISUAL_H

#ifdef __cplusplus
extern "C" {   /* permite usarlo desde C++ / JNI */
#endif

#define MV_MAX_ANCHO        640    /* ancho máximo de imagen */
#define MV_MIN_ANCHO         30
#define MV_FOV_DEFECTO_X10  600    /* campo de visión horizontal por defecto: 60.0 grados */

/* Parámetros de la decisión de confianza (ver mv_corrimiento). */
#define MV_ENERGIA_MIN        60   /* energía mínima de bordes de la huella (diferencia media entre columnas vecinas) */
#define MV_MEJOR_MAX_PCT      55   /* el mejor costo debe ser menor que este % del costo medio de todos los corrimientos */
#define MV_SEGUNDO_MAX_PCT    88   /* y menor que este % del segundo mejor mínimo (lejos del primero) */
#define MV_MAX_SALTO           4   /* si hay más de estos cuadros seguidos sin resultado válido, se cambia el cuadro de referencia */
#define MV_PRED_TOL_MIN_X16   64   /* una coincidencia ambigua se acepta si cae a menos de 4 píxeles (x16) de lo que se esperaba... */
#define MV_PRED_TOL_PCT       40   /* ...o a menos de este % del corrimiento esperado, lo que sea mayor */

/* Por qué un resultado no es válido (motivo). */
#define MV_OK                  0
#define MV_SIN_PREVIO          1   /* es el primer cuadro: no hay con qué comparar */
#define MV_ESCENA_UNIFORME     2   /* la huella casi no tiene bordes (pared lisa, oscuridad) */
#define MV_COINCIDENCIA_DEBIL  3   /* ningún corrimiento se parece de verdad */
#define MV_AMBIGUO             4   /* dos corrimientos lejanos se parecen casi igual (textura que se repite) */
#define MV_FUERA_DE_RANGO      5   /* el mejor corrimiento está justo en el borde del rango: puede ser más grande */

/* Errores de las funciones. */
#define MV_ERR_ARGUMENTO      -1
#define MV_ERR_TAMANO         -2   /* ancho fuera de MV_MIN_ANCHO a MV_MAX_ANCHO, alto menor que 2 */

typedef struct {
    int valido;             /* 1 si el corrimiento es confiable: solo entonces se puede sumar */
    int motivo;             /* MV_OK o el motivo por el que no es válido */
    int corrimiento_x16;    /* píxeles que se movió el contenido, por 16 (subpíxel); + = hacia la derecha de la imagen */
    int confianza_mil;      /* de 0 a 1000: qué tan distinta es la mejor coincidencia del resto */
    int giro_grados_x10;    /* giro del carro, por 10, con el FOV dado; + = a la derecha (0 si no es válido) */
    int energia;            /* energía de bordes de la huella nueva (para depurar) */
    int cuadros;            /* cuántos cuadros abarca este corrimiento: 1 normalmente; 2 o más si se rechazaron cuadros intermedios y se comparó contra el último bueno */
    int por_prediccion;     /* 1 si la coincidencia era ambigua y se aceptó porque coincide con el movimiento que se venía llevando */
} MovimientoVisual;

/* Lo que se recuerda del cuadro anterior (su huella). */
typedef struct {
    int tiene_previo;
    int ancho;
    int huella[MV_MAX_ANCHO];   /* huella del cuadro de REFERENCIA (el último cuadro con resultado válido) */
    int cuadros_ref;            /* cuántos cuadros han pasado desde la referencia (1 = el anterior) */
    int tiene_pred;             /* 1 si hay un corrimiento por cuadro que se venía llevando */
    int pred_x16;               /* corrimiento por cuadro (x16) del último resultado válido */
} EstimadorMovimiento;

/* Deja el estimador sin cuadro previo. */
void mv_iniciar(EstimadorMovimiento *estimador);

/* Huella de un cuadro: suma de brillo por columna de la MITAD SUPERIOR (brillo = (R+G+B)/3). `huella` debe tener
 * `ancho` posiciones. Devuelve MV_OK o un error. */
int mv_huella(const unsigned char *rgb, int ancho, int alto, int *huella);

/* Compara dos huellas de `n` columnas y busca el corrimiento (de -rango a +rango píxeles) con el que la segunda se parece más a la
 * primera. `rango` es de al menos n/3 (se corrige si es menor, y se recorta si es muy grande). Llena `salida` (sin el giro en grados). */
int mv_corrimiento(const int *anterior, const int *nueva, int n, int rango, MovimientoVisual *salida);

/* Igual, pero con una PREDICCIÓN: `pred_x16` es el corrimiento que se esperaba (el del cuadro anterior, por 16). Si la mejor coincidencia
 * resulta ambigua (otra casi igual de buena) pero cae cerca de lo esperado, se acepta (salida->por_prediccion = 1). Con `tiene_pred` = 0
 * es idéntica a mv_corrimiento. */
int mv_corrimiento_pred(const int *anterior, const int *nueva, int n, int rango, int tiene_pred, int pred_x16, MovimientoVisual *salida);

/* Convierte un corrimiento (píxeles por 16) en grados del carro por 10, con el campo de visión `fov_x10` (grados por 10) y el
 * ancho de la imagen. Positivo = giro a la derecha: un corrimiento del contenido a la IZQUIERDA es positivo. */
int mv_corrimiento_a_grados_x10(int corrimiento_x16, int ancho, int fov_x10);

/* Procesa un cuadro nuevo: calcula su huella, la compara con la del cuadro anterior (si hay) y deja la nueva como "anterior". Con un
 * cuadro de otro ancho que el anterior, empieza de cero (MV_SIN_PREVIO). Devuelve MV_OK o un error de argumentos/tamaño; si el
 * corrimiento no es confiable, MV_OK igual y salida->valido = 0 con el motivo. */
int mv_procesar(EstimadorMovimiento *estimador, const unsigned char *rgb, int ancho, int alto, int fov_x10,
                MovimientoVisual *salida);

#ifdef __cplusplus
}
#endif

#endif /* MOVIMIENTO_VISUAL_H */

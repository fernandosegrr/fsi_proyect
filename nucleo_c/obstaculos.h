/*
 * obstaculos.h — Detector de obstáculos por color de piso.
 *
 * GUÍA DE LECTURA
 *  1. Idea: el carro ve el piso libre en la parte baja de la imagen. Todo lo que no se
 *     parezca al color del piso, y esté en la zona de avance, es un obstáculo. Sirve para
 *     objetos que un modelo tipo COCO no conoce (una caja, una pared).
 *  2. Entrada: una imagen RGB de CUALQUIER tamaño razonable (3 bytes por píxel: R, G, B, fila
 *     por fila). El tamaño (ancho y alto) se indica en cada llamada; no se supone 320x240.
 *  3. La imagen se divide en celdas de 10x10 píxeles: columnas = ancho / 10 y filas = alto / 10
 *     (los píxeles que sobran en el borde derecho o abajo no se miran). Ejemplos:
 *     320x240 → 32 x 24 celdas; 240x176 → 24 x 17; 160x120 → 16 x 12.
 *  4. El color del piso se toma de una franja de celdas abajo y al centro (se asume libre). Su
 *     tamaño y la fila mínima de la zona de avance se dan como PROPORCIÓN de la imagen (en
 *     milésimas: 500 = 0.5), así que valen para cualquier tamaño.
 *     El piso puede tener VARIOS COLORES (baldosas, vetas, alfombras): se aprenden hasta
 *     OBS_MAX_GRUPOS grupos de color (k-medias corto sobre la franja) y una celda es piso si
 *     se parece a CUALQUIERA de ellos.
 *  5. Una celda es "no piso" si su color se aleja de todos los colores del piso (ver UMBRAL_COLOR, etc.).
 *     Con un ModeloPiso (obstaculos_analizar_modelo) el modelo del piso se conserva entre cuadros: se
 *     actualiza despacio solo con celdas de piso mientras el carro avanza sin obstáculos, y se
 *     reaprende solo si todo se ve "no piso" durante 1 s (cambio de piso).
 *  6. Se mira la zona de avance (por defecto la mitad inferior) en tres zonas (izquierda, centro,
 *     derecha) y cada una dice si hay obstáculo y qué tan cerca está (0 a 3).
 *  Solo enteros, sin malloc, sin recursión; los errores son códigos de retorno.
 *
 *  Los valores de calibración (umbrales) son de partida: hay que ajustarlos con la
 *  cámara real del carro y el piso real (ver docs/obstaculos.md).
 */
#ifndef OBSTACULOS_H
#define OBSTACULOS_H

#ifdef __cplusplus
extern "C" {   /* permite usarlo desde C++ / JNI */
#endif

/* ── Geometría de la imagen y de la cuadrícula de celdas ───────────────── */

#define OBS_LADO_CELDA    10   /* cada celda mide 10x10 píxeles */

/* Tamaños máximo y mínimo de la cuadrícula (sin malloc: los arreglos son de tamaño fijo). */
#define OBS_MAX_COLUMNAS  64   /* hasta 640 píxeles de ancho */
#define OBS_MAX_FILAS     48   /* hasta 480 píxeles de alto */
#define OBS_MIN_COLUMNAS   3   /* con menos de 3x3 celdas no hay tres zonas con sentido */
#define OBS_MIN_FILAS      3

/* Tamaño de REFERENCIA (el que se usó para calibrar al principio): 320x240 = 32 x 24 celdas.
 * Ya no es obligatorio; se usa en las pruebas y para explicar los valores por defecto. */
#define OBS_ANCHO        320
#define OBS_ALTO         240
#define OBS_COLUMNAS     (OBS_ANCHO / OBS_LADO_CELDA)   /* 32 */
#define OBS_FILAS        (OBS_ALTO / OBS_LADO_CELDA)    /* 24 */

/* Una rejilla chica (menos de OBS_AVISO_COLUMNAS x OBS_AVISO_FILAS celdas) sí funciona, pero
 * cada celda pesa más y el resultado es más tosco: el resultado trae el aviso `rejilla_chica`. */
#define OBS_AVISO_COLUMNAS 20
#define OBS_AVISO_FILAS    15

/* Todo lo que depende del tamaño se da como PROPORCIÓN en MILÉSIMAS (1000 = 100 %). Al analizar
 * se convierte a celdas con: celdas = total_de_celdas * milesimas / 1000 (entero, hacia abajo).
 *
 * Franja de referencia del piso (valores POR DEFECTO; se cambian con ConfigObstaculos):
 *   ancho 800‰ (la tira ancha: 26 de 32 celdas), alto 84‰ (2 de 24 celdas), centrada en el ancho (500‰) y pegada abajo (borde inferior en 1000‰).
 *   Antes era de 188‰ (6 de 32 celdas): sobre un piso de baldosas de dos colores la franja angosta caía en UNA sola baldosa y el otro color se marcaba
 *   como obstáculo. La tira ancha ve los dos colores y las manchas chicas se descartan (OBS_APOYO_MIN_PCT). */
#define OBS_REF_ANCHO_MIL        800
#define OBS_REF_ALTO_MIL          84
#define OBS_REF_CENTRO_X_MIL     500
#define OBS_REF_BORDE_INF_MIL   1000

/* Solo se buscan obstáculos en la zona de avance: de la FILA MÍNIMA hacia abajo. Las filas de
 * arriba (el fondo del cuarto, paredes lejanas) se ignoran. La fila mínima es una proporción del
 * alto: 500‰ = la mitad inferior (en 24 filas, de la fila 12 a la 23); por defecto 440‰ (de la fila 10 en 24 filas). */
#define OBS_FILA_MINIMA_MIL      440   /* validado en el carro real (ver docs/obstaculos.md): mira un poco más arriba que la mitad inferior */

/* ── Parámetros a calibrar ─────────────────────────────────────────────────
 * Los colores se comparan sobre el promedio de cada celda (cada canal de 0 a 255).
 *
 * Una celda es PISO si se cumple una de estas dos condiciones:
 *   A) su color es casi igual al de referencia: la suma de las diferencias absolutas
 *      de los tres canales (R, G, B) es menor o igual que UMBRAL_COLOR; o
 *   B) tiene el mismo "tono" pero distinta luz (una zona más clara por una lámpara o
 *      más oscura por la distancia): se compara el color NORMALIZADO por brillo
 *      (cada canal dividido entre la suma R+G+B, así la luz casi no cuenta) y es
 *      menor o igual que UMBRAL_CROMA, y además el brillo (R+G+B) no difiere más de
 *      UMBRAL_BRILLO. Esto tolera degradados de luz, pero no una caja muy oscura.
 * Si no cumple ninguna, la celda es NO PISO.
 */
#define UMBRAL_COLOR      40   /* suma |dR|+|dG|+|dB|, de 0 a 765 (valor POR DEFECTO validado en el carro real, ver docs/obstaculos.md) */
#define UMBRAL_CROMA      30   /* suma de diferencias del color normalizado, de 0 a 512 */
#define UMBRAL_BRILLO    150   /* |(R+G+B) - (R+G+B de referencia)|, de 0 a 765 */
#define SUMA_MIN_CROMA    90   /* debajo de este R+G+B el color normalizado no es confiable */

/* La franja de referencia debe ser uniforme: si alguna de sus celdas se aleja del
 * promedio de la franja más de MAX_VARIACION_REF (misma suma de canales), probablemente
 * hay un objeto encima y se devuelve OBS_ERR_REFERENCIA. */
#define MAX_VARIACION_REF  40

/* Una zona tiene obstáculo si hay al menos MIN_CELDAS celdas "no piso" confirmadas.
 * Una celda "no piso" aislada (sin ninguna vecina "no piso") se descarta como ruido. */
#define MIN_CELDAS          7   /* valor POR DEFECTO validado en el carro real, ver docs/obstaculos.md y ConfigObstaculos */

/* Cercanía según la fila más baja (la más cercana al carro) que alcanza el obstáculo. Los
 * límites también son proporciones del alto (en 24 filas: media desde la fila 16 y cerca desde
 * la 20, como antes):
 *   fila >= filas * OBS_CERCA_MIL / 1000  → 3 (muy cerca)
 *   fila >= filas * OBS_MEDIA_MIL / 1000  → 2 (media distancia)
 *   en otro caso                          → 1 (lejos)
 * y 0 si la zona no tiene obstáculo. */
#define OBS_MEDIA_MIL     667
#define OBS_CERCA_MIL     834

/* ── Pisos difíciles: negros y de 2 colores (mejoras_piso) ───────────────────
 *  - BRILLO_OSCURO: si la celda Y el color de referencia son más oscuros que esto (suma R+G+B, 0 a 765; 150 = unos 50 por canal), NO se compara
 *    el color: solo la luminancia (el brillo), con una diferencia mínima tolerada DIF_MIN_OSCURO (o el umbral de color, el que sea mayor). En lo
 *    oscuro el color es ruido (el tono normalizado se dispara) y un reflejo o un degradado de luz cambia el brillo más de lo que cabe en el umbral.
 *  - OBS_APOYO_MIN_PCT: al agrupar la franja de referencia, un grupo de color solo cuenta como piso si lo respaldan al menos este % de las celdas
 *    de la franja (así una franja ancha aprende los dos colores de un piso de baldosas, y una mancha o un objeto chico NO se aprende como piso).
 *  - Reajuste de brillo (OBS_GANANCIA_*): si TODO el piso de la zona de avance cambia de brillo de golpe (la exposición automática de la cámara),
 *    se escalan los colores aprendidos en vez de marcar todo como obstáculo. Solo si el cambio es de al menos OBS_GANANCIA_MIN_MIL (180 = 18 %), lo
 *    mismo en al menos OBS_GANANCIA_CONSISTENCIA_PCT % de las celdas (dentro de +-OBS_GANANCIA_TOLERANCIA_MIL milésimas del cambio mediano), y en cada
 *    una de las tres zonas: un objeto grande que tapa parte del piso NO lo cumple. */
#define BRILLO_OSCURO                  150
#define DIF_MIN_OSCURO                  90
#define OBS_APOYO_MIN_PCT               15
#define OBS_GANANCIA_MIN_MIL           180
#define OBS_GANANCIA_CONSISTENCIA_PCT   70
#define OBS_GANANCIA_TOLERANCIA_MIL    150

/* Motivos (bits de ResultadoObstaculos.motivo): por qué el resultado de este cuadro es el que es. */
#define OBS_MOTIVO_OBSTACULO       1   /* alguna zona tiene obstáculo */
#define OBS_MOTIVO_FRANJA_TAPADA   2   /* la franja de referencia no sirvió: se usó lo aprendido */
#define OBS_MOTIVO_REAJUSTE        4   /* se reajustó el brillo del piso (cambio de exposición) */
#define OBS_MOTIVO_PISO_NUEVO      8   /* se reaprendió el piso (todo se veía "no piso" durante 1 s) */
#define OBS_MOTIVO_PISO_OSCURO    16   /* el piso de la zona de avance es oscuro (brillo medio bajo BRILLO_OSCURO / 3 por canal) */
#define OBS_MOTIVO_TODO_NO_PISO   32   /* casi todo lo de la zona de avance se ve "no piso" */

/* ── Piso con varios colores y aprendizaje ─────────────────────────────── */

#define OBS_MAX_GRUPOS        4    /* máximo de grupos de color del piso (K) */
#define OBS_GRUPOS_DEFECTO    4    /* K por defecto */
#define OBS_APRENDIZAJE_DEFECTO 20 /* velocidad de aprendizaje por defecto: 20 milésimas = 2 % por cuadro */
#define OBS_MAX_DIST_GRUPOS  150   /* dos grupos de la franja más lejos que esto (suma |dR|+|dG|+|dB|) y de
                                      distinto tono se consideran contaminados: ERR_REFERENCIA */
#define OBS_DIST_FUSION       24   /* grupos más cerca que esto se funden en uno */
#define OBS_MS_PISO_NUEVO   1000   /* ms con TODO lejos de todos los grupos para reaprender el piso */
#define OBS_UMBRAL_BORDE      24   /* salto de color entre celdas vecinas para contar como borde (señal opcional) */

/* Lo que el detector recuerda del piso entre cuadros. Se inicia con obstaculos_modelo_iniciar() y se
 * pasa a cada llamada de obstaculos_analizar_modelo(). */
typedef struct {
    int valido;                              /* 1 si ya aprendió el piso */
    int grupos;                              /* cuántos grupos hay (1 a OBS_MAX_GRUPOS) */
    int color256[OBS_MAX_GRUPOS][3];         /* color (R, G, B) de cada grupo, por 256: así el aprendizaje lento no se pierde en redondeos */
    int peso[OBS_MAX_GRUPOS];                /* cuántas celdas respaldaron cada grupo la última vez */
    int sin_piso_activo;                     /* 1 si lleva un tiempo con todo "no piso" */
    unsigned int sin_piso_desde_ms;          /* hora en que empezó ese tiempo */
    int reaprendidos;                        /* cuántas veces se reaprendió el piso (cambio de piso) */
} ModeloPiso;

/* ── Resultado ─────────────────────────────────────────────────────────── */

#define ZONA_IZQUIERDA  0
#define ZONA_CENTRO     1
#define ZONA_DERECHA    2
#define OBS_ZONAS       3

typedef struct {
    int hay_obstaculo;   /* 1 si la zona tiene obstáculo, 0 si no */
    int cercania;        /* 0 = sin obstáculo, 1 = lejos, 2 = media, 3 = muy cerca */
    int celdas;          /* cuántas celdas "no piso" confirmadas hay en la zona */
    /* Corredor de trayectoria (corredor.h). Los llena corredor_aplicar_*; con el corredor apagado, o si nadie los llena, valen 0 y el
     * navegador usa hay_obstaculo y cercania de siempre. */
    int celdas_corredor;     /* peso (celdas) de lo que cae DENTRO del corredor en esta zona */
    int fuera_corredor;      /* 1 si hay obstáculo en la zona pero NO en el corredor: solo avisa, no frena */
    int cercania_corredor;   /* cercanía (1 a 3) de lo que está dentro del corredor; 0 = usar `cercania` */
} ZonaObstaculo;

typedef struct {
    ZonaObstaculo zona[OBS_ZONAS];
    int ref_r, ref_g, ref_b;   /* color de referencia del piso (promedio de la franja) */
    int columnas, filas;       /* tamaño de la cuadrícula de ESTE análisis (solo se usa [0..filas-1][0..columnas-1]) */
    int fila_minima_usada;     /* fila (desde arriba) donde empieza la zona de avance en este análisis */
    int rejilla_chica;         /* 1 si la cuadrícula es chica (ver OBS_AVISO_*): funciona pero es más tosca */
    int ref_columna, ref_fila;       /* primera celda de la franja de referencia usada en este análisis */
    int ref_ncolumnas, ref_nfilas;   /* tamaño de esa franja, en celdas */
    int grupos;                      /* cuántos grupos de color del piso se usaron */
    int grupo_color[OBS_MAX_GRUPOS][3];   /* el color de cada grupo (los primeros `grupos`) */
    int franja_contaminada;          /* 1 si la franja no sirvió pero se usó el modelo ya aprendido */
    int piso_reaprendido;            /* 1 si en ESTE cuadro se reaprendió el piso (cambio de piso): avisar */
    int aprendio;                    /* 1 si en este cuadro se actualizó el modelo con celdas de piso */
    int celdas_borde;                /* cuántas celdas se marcaron SOLO por la señal de bordes */
    /* Diagnóstico del cuadro (registro PISO_DIAG): */
    int brillo_medio;                /* brillo medio de la zona de avance (0 a 255 por canal) */
    int ganancia_mil;                /* factor con que se reajustó el brillo del piso en este cuadro (1000 = no se reajustó) */
    int celdas_marcadas;             /* celdas "no piso" confirmadas en la zona de avance */
    int motivo;                      /* bits OBS_MOTIVO_* */
    /* 1 en cada celda "no piso" confirmada (toda la imagen); sirve para depurar y dibujar */
    unsigned char no_piso[OBS_MAX_FILAS][OBS_MAX_COLUMNAS];
} ResultadoObstaculos;

/* ── Configuración ajustable ───────────────────────────────────────────────
 * Los parámetros que más se ajustan al probar con la cámara y el piso reales. Los demás
 * (UMBRAL_CROMA, UMBRAL_BRILLO...) siguen siendo constantes. La app los cambia en vivo.
 * Todo lo que depende del tamaño de la imagen va en MILÉSIMAS (proporciones). */
typedef struct {
    int umbral_color;           /* ver UMBRAL_COLOR; de 0 a 765 */
    int min_celdas;             /* ver MIN_CELDAS; al menos 1 */
    int ref_ancho_mil;          /* ancho de la franja de referencia, en milésimas del ancho (1 a 1000) */
    int ref_alto_mil;           /* alto de la franja de referencia, en milésimas del alto (1 a 1000) */
    int ref_centro_x_mil;       /* dónde está el CENTRO de la franja a lo ancho (0 a 1000; 500 = al centro) */
    int ref_borde_inf_mil;      /* dónde está el BORDE DE ABAJO de la franja (1 a 1000; 1000 = pegada abajo) */
    int fila_minima_mil;        /* fila mínima de la zona de avance como proporción del alto (0 a 1000);
                                   500 = 0.5 = la mitad inferior. Las filas de arriba se ignoran */
    int grupos_piso;            /* K: cuántos grupos de color del piso se buscan (1 a OBS_MAX_GRUPOS) */
    int aprendizaje_mil;        /* velocidad de aprendizaje del modelo, en milésimas por cuadro (0 a 1000; 0 = no aprende) */
    int usar_bordes;            /* 1 = señal extra de bordes entre celdas vecinas (objetos casi del color del piso) */
    int mejoras_piso;           /* 1 (por defecto) = brillo oscuro, grupos con apoyo mínimo y reajuste de brillo; 0 = el detector de antes (para comparar) */
    int piso_fijo;              /* 1 = durante una ruta: no se reaprende el piso ni se agregan colores nuevos (el brillo sí se reajusta) */
} ConfigObstaculos;

/* Códigos de retorno de obstaculos_analizar. */
#define OBS_OK                   0
#define OBS_ERR_IMAGEN_NULA     -1   /* la imagen o el resultado son NULL */
#define OBS_ERR_TAMANO          -2   /* tamaño raro: ancho o alto no positivos, mayor que 640x480 o con menos de 3x3 celdas */
#define OBS_ERR_REFERENCIA      -3   /* la franja de referencia no es uniforme (¿hay un objeto?) */
#define OBS_ERR_CONFIG          -4   /* la configuración no es válida (proporciones fuera de 0 a 1000, etc.) */

/* ── Funciones ─────────────────────────────────────────────────────────── */

/* Llena la configuración con los valores por defecto (los de las constantes de arriba). */
void obstaculos_config_defecto(ConfigObstaculos *config);

/* Analiza la imagen RGB (ancho x alto x 3 bytes; de 30x30 hasta 640x480) con la configuración
 * dada y llena el resultado. Devuelve OBS_OK o un código de error; con un error el resultado
 * queda en ceros. */
int obstaculos_analizar_con(const ConfigObstaculos *config, const unsigned char *rgb,
                            int ancho, int alto, ResultadoObstaculos *resultado);

/* Deja el modelo del piso vacío (no aprendió nada). */
void obstaculos_modelo_iniciar(ModeloPiso *modelo);

/* Analiza la imagen CONSERVANDO el modelo del piso entre cuadros:
 *  - la primera vez (o tras un reinicio) aprende los grupos de color de la franja de referencia;
 *  - si la franja deja de servir (un obstáculo encima) usa el modelo ya aprendido en vez de dar error;
 *  - si `avanzando` es 1 y no hay obstáculo, actualiza despacio el modelo con las celdas de piso;
 *  - si todo queda lejos de todos los grupos durante OBS_MS_PISO_NUEVO ms (usa `ahora_ms`), reaprende
 *    el piso desde la franja y avisa con resultado->piso_reaprendido.
 * `avanzando`: 1 si el carro está avanzando ahora. Los demás argumentos y códigos son como en
 * obstaculos_analizar_con. Con un error el modelo no cambia. */
int obstaculos_analizar_modelo(const ConfigObstaculos *config, ModeloPiso *modelo, const unsigned char *rgb,
                               int ancho, int alto, int avanzando, unsigned int ahora_ms,
                               ResultadoObstaculos *resultado);

/* ── Aprender el piso (botón «Aprender piso») ──────────────────────────────
 * Con el carro QUIETO, durante ~1 s se le dan al acumulador los cuadros (obstaculos_aprender_cuadro) y al final se agrupan los colores de TODA la zona
 * de avance (no solo de la franja de abajo): hasta K grupos, descartando los de poco apoyo (OBS_APOYO_MIN_PCT). El resultado reemplaza el modelo del piso. */
typedef struct {
    long suma[OBS_MAX_FILAS][OBS_MAX_COLUMNAS][3];   /* suma de los colores de cada celda en todos los cuadros */
    int cuadros;                                     /* cuántos cuadros se sumaron */
    int ancho, alto;                                 /* tamaño de los cuadros (todos iguales) */
    int fila_minima;                                 /* primera fila de la zona de avance */
} AcumuladorPiso;

/* Lo que se aprendió. */
typedef struct {
    int grupos;              /* cuántos colores quedaron */
    int brillo_medio;        /* brillo medio de la zona de avance (0 a 255 por canal) */
    int muy_oscuro;          /* 1 si el piso es muy oscuro (brillo medio bajo BRILLO_OSCURO / 3) */
    int muy_variado;         /* 1 si más del 20 % de las celdas quedó en colores de poco apoyo (descartados) o hace falta el máximo de colores */
    int descartado_pct;      /* % de las celdas de la zona en grupos descartados por poco apoyo */
    int cuadros;             /* cuántos cuadros se usaron */
    int color[OBS_MAX_GRUPOS][3];
    int peso_pct[OBS_MAX_GRUPOS];   /* % de las celdas que respaldan cada color */
} InformePiso;

#define OBS_APRENDER_MIN_CUADROS  3

/* Deja el acumulador vacío. */
void obstaculos_aprender_iniciar(AcumuladorPiso *acumulador);

/* Suma un cuadro. Devuelve OBS_OK o un error (tamaño raro, configuración mala, o tamaño distinto al de los cuadros anteriores). */
int obstaculos_aprender_cuadro(AcumuladorPiso *acumulador, const ConfigObstaculos *config, const unsigned char *rgb, int ancho, int alto);

/* Agrupa lo acumulado, deja el modelo del piso con esos colores y llena el informe. Devuelve OBS_OK, OBS_ERR_CONFIG o OBS_ERR_REFERENCIA (muy pocos
 * cuadros: OBS_APRENDER_MIN_CUADROS). Con un error el modelo no cambia. */
int obstaculos_aprender_terminar(const AcumuladorPiso *acumulador, const ConfigObstaculos *config, ModeloPiso *modelo, InformePiso *informe);

/* Texto corto en español de los motivos (bits OBS_MOTIVO_*), para el registro PISO_DIAG. Escribe en `texto` (capacidad `capacidad`). */
void obstaculos_texto_motivo(int motivo, char *texto, int capacidad);

/* Igual que obstaculos_analizar_con pero con la configuración por defecto. */
int obstaculos_analizar(const unsigned char *rgb, int ancho, int alto, ResultadoObstaculos *resultado);

/* Cuántas columnas / filas de celdas tiene una imagen de ese ancho / alto (0 si no es positivo). */
int obstaculos_columnas_de(int ancho);
int obstaculos_filas_de(int alto);

/* Texto corto en español para un código de retorno. */
const char *obstaculos_texto_error(int codigo);

#ifdef __cplusplus
}
#endif

#endif /* OBSTACULOS_H */

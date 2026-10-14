/*
 * pruebas_calibracion.c — Pruebas de calibracion.c en PC.
 *
 * GUÍA DE LECTURA
 *  1. Estadística: promedio, mediana y promedio sin datos atípicos.
 *  2. Valores por defecto, validez y qué falta medir.
 *  3. Ajuste de las pruebas de avance y de giro (con datos exactos y con errores).
 *  4. Conversiones cm → ms y grados → ms (y al revés), con arranque, frenado y lado del giro.
 *  5. Texto: exportar e importar la calibración.
 *  6. DATOS SIMULADOS CON RUIDO de más/menos 10 %: un carro "real" con valores conocidos se
 *     "mide" como lo hará el usuario (con ruido en cada medida), se calibra, y se comprueba que lo
 *     calculado se parece a la verdad. Se repite con muchas semillas distintas.
 */

#include <stdio.h>
#include <string.h>
#include "calibracion.h"

static int casos = 0;
static int fallos = 0;

static void caso(const char *nombre, int ok)
{
    casos++;
    if (!ok) {
        fallos++;
    }
    printf("%s  %s\n", ok ? "OK   " : "FALLO", nombre);
}

static int valor_absoluto(int x)
{
    return (x < 0) ? -x : x;
}

/* ── 1. Estadística ────────────────────────────────────────────────────── */

static void pruebas_estadistica(void)
{
    static const int a[] = { 10, 20, 30 };
    static const int b[] = { 10, 20, 30, 41 };
    static const int c[] = { 5, 1, 3 };
    static const int negativos[] = { -10, -20, -31 };
    static const int atipico[] = { 100, 102, 300 };
    static const int sin_atipicos[] = { 100, 101, 99 };
    static const int dos[] = { 100, 500 };
    static const int unico[] = { 77 };
    static const int ceros[] = { 0, 0, 0 };
    static const int cero_y_uno[] = { 0, 0, 5 };
    static const int muchos[CAL_MAX_MUESTRAS + 1] = { 1 };
    int usados = -1;
    int copia[3];

    caso("promedio de 10, 20 y 30 = 20", cal_promedio(a, 3) == 20);
    caso("promedio redondea al mas cercano (10, 20, 30, 41 = 25.25 -> 25)", cal_promedio(b, 4) == 25);
    caso("promedio de negativos (-10, -20, -31 = -20.33 -> -20)", cal_promedio(negativos, 3) == -20);
    caso("promedio de lista vacia o nula = 0", cal_promedio(a, 0) == 0 && cal_promedio(NULL, 3) == 0);
    caso("promedio de demasiados datos = 0", cal_promedio(muchos, CAL_MAX_MUESTRAS + 1) == 0);
    caso("mediana impar (5, 1, 3 -> 3)", cal_mediana(c, 3) == 3);
    caso("mediana par (10, 20, 30, 41 -> 25)", cal_mediana(b, 4) == 25);
    memcpy(copia, c, sizeof copia);
    cal_mediana(copia, 3);
    caso("la mediana no modifica la lista original", memcmp(copia, c, sizeof copia) == 0);
    caso("mediana de lista nula = 0", cal_mediana(NULL, 3) == 0);

    caso("sin atipicos: 100, 102, 300 -> descarta el 300 y promedia 101", cal_promedio_sin_atipicos(atipico, 3, &usados) == 101 && usados == 2);
    caso("sin atipicos: datos parecidos se usan todos", cal_promedio_sin_atipicos(sin_atipicos, 3, &usados) == 100 && usados == 3);
    caso("sin atipicos: con 2 datos no se rechaza ninguno", cal_promedio_sin_atipicos(dos, 2, &usados) == 300 && usados == 2);
    caso("sin atipicos: con 1 dato es ese dato", cal_promedio_sin_atipicos(unico, 1, &usados) == 77 && usados == 1);
    caso("sin atipicos: ceros no divide entre cero", cal_promedio_sin_atipicos(ceros, 3, &usados) == 0 && usados == 3);
    caso("sin atipicos: mediana cero y un dato lejano se rechaza", cal_promedio_sin_atipicos(cero_y_uno, 3, &usados) == 0 && usados == 2);
    caso("sin atipicos: lista nula = 0 y usados 0", cal_promedio_sin_atipicos(NULL, 3, &usados) == 0 && usados == 0);
}

/* ── 2. Por defecto y validez ──────────────────────────────────────────── */

static void pruebas_defecto(void)
{
    CalibracionCarro cal;
    CalibracionCarro mala;

    cal_defecto(&cal);
    caso("por defecto: es valida y no hay nada medido", cal_es_valida(&cal) && cal.medidos == 0);
    caso("por defecto: coincide con MS_POR_CM (50 ms/cm) y MS_POR_GRADO (10 ms/grado)",
         cal.ms_por_cm_x10 == MS_POR_CM * 10 && cal.ms_por_grado_izq_x10 == MS_POR_GRADO * 10 &&
         cal.ms_por_grado_der_x10 == MS_POR_GRADO * 10 && cal.arranque_ms == 0 && cal.frenado_mm == 0);
    caso("por defecto: faltan los 4 bits de medicion", cal_faltantes(&cal) == CAL_MEDIDO_TODO);
    caso("cal_faltantes de NULL = todo", cal_faltantes(NULL) == CAL_MEDIDO_TODO);
    caso("cal_es_valida de NULL = 0", cal_es_valida(NULL) == 0);

    mala = cal; mala.ms_por_cm_x10 = 0;
    caso("no valida: ms por cm = 0", !cal_es_valida(&mala));
    mala = cal; mala.arranque_ms = -1;
    caso("no valida: arranque negativo", !cal_es_valida(&mala));
    mala = cal; mala.arranque_ms = 5000;
    caso("no valida: arranque de 5 s", !cal_es_valida(&mala));
    mala = cal; mala.ms_por_grado_der_x10 = 5;
    caso("no valida: ms por grado muy chico", !cal_es_valida(&mala));
    mala = cal; mala.frenado_mm = 900;
    caso("no valida: frenado de 90 cm", !cal_es_valida(&mala));
    mala = cal; mala.medidos = 99;
    caso("no valida: bits de medicion desconocidos", !cal_es_valida(&mala));

    caso("cal_poner_arranque guarda y marca el bit", cal_poner_arranque(&cal, 150) == CAL_OK && cal.arranque_ms == 150 &&
         (cal.medidos & CAL_MEDIDO_ARRANQUE) && cal_faltantes(&cal) == (CAL_MEDIDO_TODO & ~CAL_MEDIDO_ARRANQUE));
    caso("cal_poner_arranque rechaza -5 y 9000 sin cambiar nada",
         cal_poner_arranque(&cal, -5) == CAL_ERR_RANGO && cal_poner_arranque(&cal, 9000) == CAL_ERR_RANGO && cal.arranque_ms == 150);
    caso("cal_poner_arranque con NULL = CAL_ERR_ARGUMENTO", cal_poner_arranque(NULL, 100) == CAL_ERR_ARGUMENTO);
}

/* ── 3. Ajuste de avance y giro con datos exactos ──────────────────────── */

static void pruebas_ajuste(void)
{
    CalibracionCarro cal;
    CalibracionCarro antes;
    /* Carro con arranque 150 ms, 40 ms/cm (4 ms por mm) y 30 mm de frenado:
     *   mm(ms) = (ms - 150) / 4 + 30   ->  1000: 242.5, 2000: 492.5, 3000: 742.5 */
    static const int ms3[] = { 1000, 2000, 3000 };
    static const int mm3[] = { 242, 492, 742 };
    static const int ms2[] = { 1000, 2000 };
    static const int mm2[] = { 242, 492 };
    static const int ms_malo[] = { 1000, 2000 };
    static const int mm_decrece[] = { 500, 300 };
    static const int ms_iguales[] = { 1000, 1000 };
    static const int mm_distintos[] = { 200, 300 };
    static const int ms_corto[] = { 100, 2000 };   /* el primero no pasa el arranque de 150 ms */
    static const int mm_corto[] = { 5, 492 };
    static const int ms_lento[] = { 1000, 2000 };   /* 1 mm en mil ms: 1000 ms por mm = 10000 ms/cm, fuera de rango */
    static const int mm_lento[] = { 10, 11 };
    static const int ms_9[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    /* Giro izquierdo: arranque 150, 9 ms/grado, frenado 6 grados:
     *   grados_x10(ms) = (ms - 150) / 9 * 10 + 60 -> 500: 450, 1000: 1005 */
    static const int ms_g[] = { 500, 1000, 1500 };
    static const int g_izq[] = { 450, 1005, 1561 };      /* grados x10 */
    static const int g_der[] = { 344, 779, 1214 };        /* otro lado: 11.5 ms/grado, frenado 4 grados */

    cal_defecto(&cal);
    cal_poner_arranque(&cal, 150);

    caso("avance con 3 puntos exactos: 40.0 ms/cm", cal_ajustar_avance(&cal, 3, ms3, mm3) == CAL_OK && cal.ms_por_cm_x10 >= 399 && cal.ms_por_cm_x10 <= 401);
    caso("avance con 3 puntos exactos: frenado de unos 30 mm", valor_absoluto(cal.frenado_mm - 30) <= 2);
    caso("avance marca su bit", (cal.medidos & CAL_MEDIDO_AVANCE) != 0);

    cal_defecto(&cal);
    cal_poner_arranque(&cal, 150);
    caso("avance con 2 puntos (1000 y 2000 ms): 40.0 ms/cm y frenado 30 mm",
         cal_ajustar_avance(&cal, 2, ms2, mm2) == CAL_OK && cal.ms_por_cm_x10 == 400 && valor_absoluto(cal.frenado_mm - 30) <= 2);

    cal_defecto(&cal);
    caso("avance sin haber medido el arranque (0): el frenado absorbe el arranque (puede salir 0)",
         cal_ajustar_avance(&cal, 3, ms3, mm3) == CAL_OK && cal.ms_por_cm_x10 >= 399 && cal.ms_por_cm_x10 <= 401 && cal.frenado_mm >= 0);

    cal_defecto(&cal);
    cal_poner_arranque(&cal, 150);
    antes = cal;
    caso("avance con datos que decrecen: CAL_ERR_DATOS y no cambia nada",
         cal_ajustar_avance(&cal, 2, ms_malo, mm_decrece) == CAL_ERR_DATOS && memcmp(&cal, &antes, sizeof cal) == 0);
    caso("avance con tiempos iguales: CAL_ERR_DATOS", cal_ajustar_avance(&cal, 2, ms_iguales, mm_distintos) == CAL_ERR_DATOS);
    caso("avance con una prueba mas corta que el arranque: CAL_ERR_DATOS", cal_ajustar_avance(&cal, 2, ms_corto, mm_corto) == CAL_ERR_DATOS);
    caso("avance con una pendiente absurda (10 s por cm): CAL_ERR_RANGO y no cambia nada",
         cal_ajustar_avance(&cal, 2, ms_lento, mm_lento) == CAL_ERR_RANGO && memcmp(&cal, &antes, sizeof cal) == 0);
    caso("avance con 1 punto o 9 puntos: CAL_ERR_ARGUMENTO", cal_ajustar_avance(&cal, 1, ms3, mm3) == CAL_ERR_ARGUMENTO &&
         cal_ajustar_avance(&cal, 9, ms_9, ms_9) == CAL_ERR_ARGUMENTO);
    caso("avance con NULL: CAL_ERR_ARGUMENTO", cal_ajustar_avance(NULL, 3, ms3, mm3) == CAL_ERR_ARGUMENTO &&
         cal_ajustar_avance(&cal, 3, NULL, mm3) == CAL_ERR_ARGUMENTO);

    /* Giro: cada lado se ajusta aparte. */
    cal_defecto(&cal);
    cal_poner_arranque(&cal, 150);
    caso("giro izquierdo: ~9 ms/grado, frenado de unos 6 grados",
         cal_ajustar_giro(&cal, CAL_LADO_IZQUIERDA, 3, ms_g, g_izq) == CAL_OK &&
         valor_absoluto(cal.ms_por_grado_izq_x10 - 90) <= 2 && valor_absoluto(cal.frenado_grados_izq_x10 - 60) <= 6);
    caso("giro izquierdo no cambia el derecho (sigue por defecto) y marca solo su bit",
         cal.ms_por_grado_der_x10 == MS_POR_GRADO * 10 && (cal.medidos & CAL_MEDIDO_GIRO_IZQ) && !(cal.medidos & CAL_MEDIDO_GIRO_DER));
    caso("giro derecho: ~11.5 ms/grado, frenado de unos 4 grados",
         cal_ajustar_giro(&cal, CAL_LADO_DERECHA, 3, ms_g, g_der) == CAL_OK &&
         valor_absoluto(cal.ms_por_grado_der_x10 - 115) <= 3 && valor_absoluto(cal.frenado_grados_der_x10 - 40) <= 6);
    caso("los dos lados quedan distintos", cal.ms_por_grado_izq_x10 != cal.ms_por_grado_der_x10);
    caso("giro con un lado que no existe: CAL_ERR_ARGUMENTO", cal_ajustar_giro(&cal, 0, 3, ms_g, g_izq) == CAL_ERR_ARGUMENTO);
    caso("giro con datos que decrecen: CAL_ERR_DATOS", cal_ajustar_giro(&cal, CAL_LADO_DERECHA, 2, ms_malo, mm_decrece) == CAL_ERR_DATOS);
}

/* ── 4. Conversiones ───────────────────────────────────────────────────── */

static void pruebas_conversiones(void)
{
    CalibracionCarro cal;

    cal_defecto(&cal);
    caso("por defecto: 40 cm = 2000 ms (MS_POR_CM 50) y 90 grados = 900 ms (MS_POR_GRADO 10)",
         cal_cm_a_ms(&cal, 40) == 40 * MS_POR_CM && cal_grados_a_ms(&cal, 90) == 90 * MS_POR_GRADO &&
         cal_grados_a_ms(&cal, -90) == 90 * MS_POR_GRADO);
    caso("por defecto: ms -> mm y ms -> grados son los de siempre",
         cal_ms_a_mm(&cal, 2000) == 400 && cal_ms_a_grados(&cal, CAL_LADO_DERECHA, 900) == 90);
    caso("0 cm o 0 grados = 0 ms; cm negativos = 0 ms", cal_cm_a_ms(&cal, 0) == 0 && cal_grados_a_ms(&cal, 0) == 0 && cal_cm_a_ms(&cal, -5) == 0);
    caso("con NULL no revienta", cal_cm_a_ms(NULL, 10) == 0 && cal_grados_a_ms(NULL, 10) == 0 && cal_ms_a_mm(NULL, 100) == 0 &&
         cal_ms_a_grados(NULL, 1, 100) == 0 && cal_frenado_grados(NULL, 1) == 0);

    /* Carro con arranque 150 ms, 40 ms/cm y frenado 30 mm; giro izq 9 ms/grado (frenado 6),
     * der 12 ms/grado (frenado 4). */
    cal.medidos = CAL_MEDIDO_TODO;
    cal.arranque_ms = 150;
    cal.ms_por_cm_x10 = 400;
    cal.frenado_mm = 30;
    cal.ms_por_grado_izq_x10 = 90;
    cal.frenado_grados_izq_x10 = 60;
    cal.ms_por_grado_der_x10 = 120;
    cal.frenado_grados_der_x10 = 40;

    caso("40 cm con arranque 150 y frenado 30 mm: 150 + (400 - 30) * 4 = 1630 ms", cal_cm_a_ms(&cal, 40) == 1630);
    caso("el tiempo de avance crece con la distancia: 10 cm < 20 cm < 40 cm", cal_cm_a_ms(&cal, 10) < cal_cm_a_ms(&cal, 20) &&
         cal_cm_a_ms(&cal, 20) < cal_cm_a_ms(&cal, 40));
    caso("3 cm (30 mm) = justo el frenado: no hace falta motor (0 ms)", cal_cm_a_ms(&cal, 3) == 0);
    caso("2 cm (menos que el frenado): 0 ms", cal_cm_a_ms(&cal, 2) == 0);
    caso("ida y vuelta: el avance de cm_a_ms mas el frenado da los cm pedidos (40 cm = 400 mm)",
         valor_absoluto(cal_ms_a_mm(&cal, cal_cm_a_ms(&cal, 40)) + cal.frenado_mm - 400) <= 2);
    caso("ida y vuelta con varias distancias (10, 25, 60, 100 cm) dentro de 2 mm",
         valor_absoluto(cal_ms_a_mm(&cal, cal_cm_a_ms(&cal, 10)) + cal.frenado_mm - 100) <= 2 &&
         valor_absoluto(cal_ms_a_mm(&cal, cal_cm_a_ms(&cal, 25)) + cal.frenado_mm - 250) <= 2 &&
         valor_absoluto(cal_ms_a_mm(&cal, cal_cm_a_ms(&cal, 60)) + cal.frenado_mm - 600) <= 2 &&
         valor_absoluto(cal_ms_a_mm(&cal, cal_cm_a_ms(&cal, 100)) + cal.frenado_mm - 1000) <= 2);
    caso("antes del arranque el carro no se ha movido: 100 ms = 0 mm; justo 150 ms = 0 mm", cal_ms_a_mm(&cal, 100) == 0 && cal_ms_a_mm(&cal, 150) == 0);

    caso("giro izquierdo 90: 150 + (900 - 60) * 9 / 10 = 906 ms", cal_grados_a_ms(&cal, -90) == 906);
    caso("giro derecho 90: 150 + (900 - 40) * 12 / 10 = 1182 ms", cal_grados_a_ms(&cal, 90) == 1182);
    caso("izquierda y derecha NO son simetricas", cal_grados_a_ms(&cal, -90) != cal_grados_a_ms(&cal, 90));
    caso("ms a grados usa el lado: 906 ms a la izquierda = 84 grados (+ frenado 6 = 90)",
         cal_ms_a_grados(&cal, CAL_LADO_IZQUIERDA, 906) + cal_frenado_grados(&cal, CAL_LADO_IZQUIERDA) == 90);
    caso("ms a grados derecha: 1182 ms = 86 grados (+ frenado 4 = 90)",
         cal_ms_a_grados(&cal, CAL_LADO_DERECHA, 1182) + cal_frenado_grados(&cal, CAL_LADO_DERECHA) == 90);
    caso("el mismo tiempo gira distinto a cada lado", cal_ms_a_grados(&cal, CAL_LADO_IZQUIERDA, 1000) > cal_ms_a_grados(&cal, CAL_LADO_DERECHA, 1000));
    caso("frenado de giro en grados enteros (6 y 4)", cal_frenado_grados(&cal, CAL_LADO_IZQUIERDA) == 6 && cal_frenado_grados(&cal, CAL_LADO_DERECHA) == 4);
    caso("giro de 5 grados a la izquierda (menos que el frenado de 6): 0 ms", cal_grados_a_ms(&cal, -5) == 0);
}

/* ── 5. Texto ──────────────────────────────────────────────────────────── */

static void pruebas_texto(void)
{
    CalibracionCarro cal, leida, antes;
    char texto[CAL_LARGO_TEXTO];
    char chico[10];
    int largo;

    cal_defecto(&cal);
    cal.medidos = CAL_MEDIDO_TODO;
    cal.arranque_ms = 143;
    cal.ms_por_cm_x10 = 417;
    cal.frenado_mm = 28;
    cal.ms_por_grado_izq_x10 = 93;
    cal.ms_por_grado_der_x10 = 121;
    cal.frenado_grados_izq_x10 = 55;
    cal.frenado_grados_der_x10 = 35;

    largo = cal_a_texto(&cal, texto, sizeof texto);
    caso("cal_a_texto escribe algo con 'clave=valor' y devuelve el largo", largo > 50 && largo == (int)strlen(texto) && strstr(texto, "arranque_ms=143\n") != NULL &&
         strstr(texto, "ms_por_grado_der_x10=121\n") != NULL);
    cal_defecto(&leida);
    caso("ida y vuelta: cal_de_texto(cal_a_texto(x)) == x", cal_de_texto(texto, &leida) == CAL_OK && memcmp(&cal, &leida, sizeof cal) == 0);
    caso("la calibracion por defecto tambien hace ida y vuelta", (cal_defecto(&leida), cal_a_texto(&leida, texto, sizeof texto) > 0) &&
         cal_de_texto(texto, &cal) == CAL_OK && memcmp(&cal, &leida, sizeof cal) == 0);

    cal_defecto(&cal);
    cal_a_texto(&cal, texto, sizeof texto);
    caso("texto bien escrito con CRLF y comentarios",
         cal_de_texto("# calibracion\r\n\r\nmedidos=15\r\narranque_ms=140\r\nms_por_cm_x10=420\r\nfrenado_mm=25\r\n"
                      "ms_por_grado_izq_x10=90\r\nms_por_grado_der_x10=110\r\nfrenado_grados_izq_x10=50\r\n  frenado_grados_der_x10=40  \r\n", &leida) == CAL_OK &&
         leida.arranque_ms == 140 && leida.ms_por_grado_der_x10 == 110 && leida.frenado_grados_der_x10 == 40);

    cal_defecto(&antes);
    leida = antes;
    caso("falta una clave: CAL_ERR_TEXTO y no cambia nada",
         cal_de_texto("medidos=15\narranque_ms=140\n", &leida) == CAL_ERR_TEXTO && memcmp(&leida, &antes, sizeof leida) == 0);
    caso("clave desconocida: CAL_ERR_TEXTO", cal_de_texto("hola=3\n", &leida) == CAL_ERR_TEXTO);
    caso("clave repetida: CAL_ERR_TEXTO", cal_de_texto("medidos=1\nmedidos=2\n", &leida) == CAL_ERR_TEXTO);
    caso("valor que no es numero: CAL_ERR_TEXTO", cal_de_texto("medidos=abc\n", &leida) == CAL_ERR_TEXTO);
    caso("basura despues del numero: CAL_ERR_TEXTO", cal_de_texto("medidos=15 hola\n", &leida) == CAL_ERR_TEXTO);
    caso("texto vacio: CAL_ERR_TEXTO", cal_de_texto("", &leida) == CAL_ERR_TEXTO);
    caso("texto NULL o destino NULL: CAL_ERR_ARGUMENTO", cal_de_texto(NULL, &leida) == CAL_ERR_ARGUMENTO && cal_de_texto("x=1", NULL) == CAL_ERR_ARGUMENTO);

    caso("valor fuera de rango (arranque 9000 ms): CAL_ERR_TEXTO",
         cal_de_texto("medidos=0\narranque_ms=9000\nms_por_cm_x10=500\nfrenado_mm=0\nms_por_grado_izq_x10=100\n"
                      "ms_por_grado_der_x10=100\nfrenado_grados_izq_x10=0\nfrenado_grados_der_x10=0\n", &leida) == CAL_ERR_TEXTO);
    cal_defecto(&cal);
    caso("buffer chico: cal_a_texto da CAL_ERR_ARGUMENTO y deja texto vacio", cal_a_texto(&cal, chico, sizeof chico) == CAL_ERR_ARGUMENTO && chico[0] == '\0');
    caso("buffer nulo: CAL_ERR_ARGUMENTO", cal_a_texto(&cal, NULL, 100) == CAL_ERR_ARGUMENTO && cal_a_texto(NULL, texto, 100) == CAL_ERR_ARGUMENTO);
}

/* ── 6. Datos simulados con ruido de +-10 % ────────────────────────────── */

static unsigned int semilla = 777u;

static unsigned int aleatorio(void)
{
    semilla = semilla * 1103515245u + 12345u;
    return (semilla >> 16) & 0x7FFF;
}

/* Multiplica `valor` por un factor al azar entre 90 % y 110 % (ruido de +-10 %). */
static int con_ruido(int valor)
{
    int porcentaje = 90 + (int)(aleatorio() % 21u);   /* 90 a 110 */

    return (int)(((long)valor * porcentaje + 50) / 100);
}

/* Un "carro real" con valores verdaderos conocidos. */
static CalibracionCarro carro_real(void)
{
    CalibracionCarro real;

    cal_defecto(&real);
    real.medidos = CAL_MEDIDO_TODO;
    real.arranque_ms = 140;
    real.ms_por_cm_x10 = 420;            /* 42 ms por cm */
    real.frenado_mm = 25;
    real.ms_por_grado_izq_x10 = 90;      /* 9 ms por grado */
    real.ms_por_grado_der_x10 = 115;     /* 11.5 ms por grado */
    real.frenado_grados_izq_x10 = 60;
    real.frenado_grados_der_x10 = 40;
    return real;
}

/* Lo que mediría el usuario (con ruido) tras mandar FORWARD `ms` ms: mm avanzados, ya con frenado. */
static int medir_avance(const CalibracionCarro *real, int ms)
{
    return con_ruido(cal_ms_a_mm(real, ms) + real->frenado_mm);
}

/* Igual para girar `ms` ms hacia `lado`: grados por 10 con ruido. */
static int medir_giro_x10(const CalibracionCarro *real, int lado, int ms)
{
    return con_ruido(cal_ms_a_grados(real, lado, ms) * 10 + ((lado < 0) ? real->frenado_grados_izq_x10 : real->frenado_grados_der_x10));
}

/* Hace TODO el procedimiento de la app con datos con ruido y devuelve la calibración calculada.
 * Pasos: arranque (5 toques con ruido, mediana sin atipicos), avance (3 tiempos x 3 repeticiones,
 * promedio sin atipicos por tiempo) y giro de cada lado (3 tiempos x 3 repeticiones). Un 10 % de las
 * veces se mete una medida disparatada (por ejemplo, un cero mal anotado) en una repeticion. */
static int calibrar_con_ruido(const CalibracionCarro *real, int con_disparates, CalibracionCarro *resultado)
{
    static const int TIEMPOS_AVANCE[3] = { 1000, 2000, 3000 };
    static const int TIEMPOS_GIRO[3] = { 400, 800, 1200 };
    int toques[5];
    int ms[3], mm[3], g[3];
    int i, k, lado, codigo;

    cal_defecto(resultado);
    for (i = 0; i < 5; i++) {
        toques[i] = con_ruido(real->arranque_ms);
    }
    codigo = cal_poner_arranque(resultado, cal_promedio_sin_atipicos(toques, 5, NULL));
    if (codigo != CAL_OK) {
        return codigo;
    }

    for (k = 0; k < 3; k++) {
        int rep[3];

        ms[k] = TIEMPOS_AVANCE[k];
        for (i = 0; i < 3; i++) {
            rep[i] = medir_avance(real, TIEMPOS_AVANCE[k]);
        }
        if (con_disparates && aleatorio() % 10u == 0) {
            rep[aleatorio() % 3u] *= 3;      /* un dato mal anotado */
        }
        mm[k] = cal_promedio_sin_atipicos(rep, 3, NULL);
    }
    codigo = cal_ajustar_avance(resultado, 3, ms, mm);
    if (codigo != CAL_OK) {
        return codigo;
    }

    for (lado = CAL_LADO_IZQUIERDA; lado <= CAL_LADO_DERECHA; lado += 2) {
        for (k = 0; k < 3; k++) {
            int rep[3];

            ms[k] = TIEMPOS_GIRO[k];
            for (i = 0; i < 3; i++) {
                rep[i] = medir_giro_x10(real, lado, TIEMPOS_GIRO[k]);
            }
            if (con_disparates && aleatorio() % 10u == 0) {
                rep[aleatorio() % 3u] *= 3;
            }
            g[k] = cal_promedio_sin_atipicos(rep, 3, NULL);
        }
        codigo = cal_ajustar_giro(resultado, lado, 3, ms, g);
        if (codigo != CAL_OK) {
            return codigo;
        }
    }
    return CAL_OK;
}

/* Error relativo en por ciento (valor absoluto) de `medido` respecto a `verdad`. */
static int error_pct(int medido, int verdad)
{
    return valor_absoluto(medido - verdad) * 100 / verdad;
}

static void pruebas_ruido(void)
{
    const int PRUEBAS = 400;
    CalibracionCarro real = carro_real();
    CalibracionCarro calculada;
    int i, errores_codigo = 0, dentro_avance = 0, dentro_izq = 0, dentro_der = 0, dentro_conv = 0, mala_conv_total = 0;
    int peor_avance = 0, peor_izq = 0, peor_der = 0, peor_conv = 0;
    int suma_error_avance = 0;

    for (i = 0; i < PRUEBAS; i++) {
        int e_av, e_izq, e_der, e_conv, mm_real, ms_ordenado;

        semilla = 1000u + (unsigned int)i * 7919u;
        if (calibrar_con_ruido(&real, 0, &calculada) != CAL_OK) {
            errores_codigo++;
            continue;
        }
        e_av = error_pct(calculada.ms_por_cm_x10, real.ms_por_cm_x10);
        e_izq = error_pct(calculada.ms_por_grado_izq_x10, real.ms_por_grado_izq_x10);
        e_der = error_pct(calculada.ms_por_grado_der_x10, real.ms_por_grado_der_x10);
        suma_error_avance += e_av;
        if (e_av > peor_avance) peor_avance = e_av;
        if (e_izq > peor_izq) peor_izq = e_izq;
        if (e_der > peor_der) peor_der = e_der;
        if (e_av <= 10) dentro_avance++;
        if (e_izq <= 10) dentro_izq++;
        if (e_der <= 10) dentro_der++;

        /* Lo importante: se pide al carro real 40 cm con la calibracion calculada; ¿cuánto avanza de verdad? */
        ms_ordenado = cal_cm_a_ms(&calculada, 40);
        mm_real = cal_ms_a_mm(&real, ms_ordenado) + real.frenado_mm;   /* el carro real */
        e_conv = error_pct(mm_real, 400);
        if (e_conv > peor_conv) peor_conv = e_conv;
        if (e_conv <= 10) dentro_conv++;
        if (e_conv > 20) mala_conv_total++;
    }
    printf("      ruido +-10 %%, %d calibraciones: error del ms/cm: medio %d %% y peor %d %%; ms/grado izq peor %d %%, der peor %d %%; 40 cm pedidos -> peor error %d %%\n",
           PRUEBAS, suma_error_avance / (PRUEBAS - errores_codigo), peor_avance, peor_izq, peor_der, peor_conv);

    caso("con ruido de +-10 % el procedimiento completo nunca da error de datos o de rango", errores_codigo == 0);
    caso("ms por cm: el error medio es menor que 5 %", suma_error_avance / (PRUEBAS - errores_codigo) < 5);
    caso("ms por cm: al menos 90 % de las calibraciones quedan a menos de 10 % de la verdad", dentro_avance * 100 / PRUEBAS >= 90);
    caso("ms por grado izquierdo: al menos 90 % a menos de 10 %", dentro_izq * 100 / PRUEBAS >= 90);
    caso("ms por grado derecho: al menos 90 % a menos de 10 %", dentro_der * 100 / PRUEBAS >= 90);
    caso("el peor ms por cm sigue siendo razonable (menos de 25 %)", peor_avance < 25);
    caso("pedir 40 cm con la calibracion calculada: al menos 90 % de las veces el carro real avanza 40 cm +-10 %", dentro_conv * 100 / PRUEBAS >= 90);
    caso("pedir 40 cm: nunca se pasa de 20 % de error en mas de 2 % de las calibraciones", mala_conv_total * 100 / PRUEBAS <= 2);
}

/* Con un 10 % de mediciones disparatadas (un dato mal anotado entre tres repeticiones), el
 * rechazo de atípicos debe mantener el resultado casi igual de bueno. */
static void pruebas_disparates(void)
{
    const int PRUEBAS = 400;
    CalibracionCarro real = carro_real();
    CalibracionCarro calculada, sin_rechazo;
    int i, buenas = 0, errores_codigo = 0;

    (void)sin_rechazo;
    for (i = 0; i < PRUEBAS; i++) {
        semilla = 50000u + (unsigned int)i * 104729u;
        if (calibrar_con_ruido(&real, 1, &calculada) != CAL_OK) {
            errores_codigo++;
            continue;
        }
        if (error_pct(calculada.ms_por_cm_x10, real.ms_por_cm_x10) <= 15) {
            buenas++;
        }
    }
    printf("      con datos disparatados (10 %% de las repeticiones multiplicadas por 3): %d de %d calibraciones con ms/cm dentro de 15 %%\n", buenas, PRUEBAS);
    caso("con datos disparatados el rechazo de atipicos mantiene al menos 85 % de las calibraciones dentro de 15 %", buenas * 100 / PRUEBAS >= 85);
    caso("con datos disparatados no hay errores de codigo", errores_codigo == 0);
}

/* Un carro calibrado con la verdad exacta (sin ruido): la conversion es casi perfecta. */
static void pruebas_sin_ruido(void)
{
    CalibracionCarro real = carro_real();
    CalibracionCarro calculada;
    int i, ms[3] = { 1000, 2000, 3000 }, mm[3];

    cal_defecto(&calculada);
    cal_poner_arranque(&calculada, real.arranque_ms);
    for (i = 0; i < 3; i++) {
        mm[i] = cal_ms_a_mm(&real, ms[i]) + real.frenado_mm;
    }
    cal_ajustar_avance(&calculada, 3, ms, mm);
    caso("sin ruido: el ms por cm sale casi exacto (+-1 de 420)", valor_absoluto(calculada.ms_por_cm_x10 - 420) <= 1);
    caso("sin ruido: el frenado sale casi exacto (+-2 mm de 25)", valor_absoluto(calculada.frenado_mm - 25) <= 2);
}

/* ── Campo de visión de la cámara (giro visual) ──────────────────────── */

static void pruebas_fov(void)
{
    CalibracionCarro cal, antes, leida;
    char texto[CAL_LARGO_TEXTO];

    cal_defecto(&cal);
    caso("por defecto el campo de vision es 60 grados (600) y no cuenta como medido", cal.fov_grados_x10 == 600 && !(cal.medidos & CAL_MEDIDO_FOV));
    caso("el campo de vision NO es obligatorio: sin medirlo, cal_faltantes sigue siendo solo los 4 bits de siempre",
         cal_faltantes(&cal) == CAL_MEDIDO_TODO && CAL_MEDIDO_TODO == 15);

    caso("un giro real de 90 grados que la camara midio como 80 (FOV 60): el nuevo FOV es 67.5 grados (675)",
         cal_ajustar_fov(&cal, 900, 800) == CAL_OK && cal.fov_grados_x10 == 675 && (cal.medidos & CAL_MEDIDO_FOV));
    cal_defecto(&cal);
    caso("si la camara midio de mas (100 en vez de 90): el FOV baja a 54 grados (540)", cal_ajustar_fov(&cal, 900, 1000) == CAL_OK && cal.fov_grados_x10 == 540);
    cal_defecto(&cal);
    caso("si midio justo lo real, el FOV no cambia", cal_ajustar_fov(&cal, 900, 900) == CAL_OK && cal.fov_grados_x10 == 600);
    cal_defecto(&cal);
    cal_ajustar_fov(&cal, 900, 800);
    caso("ajustar el FOV dos veces compone (cada vez corrige el valor actual)", cal.fov_grados_x10 == 675 &&
         cal_ajustar_fov(&cal, 900, 950) == CAL_OK && cal.fov_grados_x10 == 639);

    cal_defecto(&cal);
    antes = cal;
    caso("datos no positivos: CAL_ERR_DATOS y no cambia nada", cal_ajustar_fov(&cal, 0, 800) == CAL_ERR_DATOS && cal_ajustar_fov(&cal, 900, -5) == CAL_ERR_DATOS &&
         memcmp(&cal, &antes, sizeof cal) == 0);
    caso("un resultado absurdo (FOV de 675 grados): CAL_ERR_RANGO y no cambia nada", cal_ajustar_fov(&cal, 9000, 800) == CAL_ERR_RANGO && memcmp(&cal, &antes, sizeof cal) == 0);
    caso("un FOV de menos de 20 grados tambien es CAL_ERR_RANGO", cal_ajustar_fov(&cal, 100, 900) == CAL_ERR_RANGO);
    caso("cal NULL: CAL_ERR_ARGUMENTO", cal_ajustar_fov(NULL, 900, 800) == CAL_ERR_ARGUMENTO);

    cal_defecto(&cal);
    cal.fov_grados_x10 = 10;
    caso("un FOV de 1 grado no es valido", !cal_es_valida(&cal));
    cal.fov_grados_x10 = 1600;
    caso("un FOV de 160 grados no es valido", !cal_es_valida(&cal));
    cal_defecto(&cal);
    cal.medidos = 31;
    caso("el bit del FOV (16) hace medidos = 31, que sigue siendo valido", cal_es_valida(&cal));
    cal.medidos = 32;
    caso("medidos = 32 no es valido", !cal_es_valida(&cal));

    /* Texto: el FOV viaja, y los textos viejos (sin FOV) se siguen leyendo. */
    cal_defecto(&cal);
    cal.medidos = 31;
    cal.fov_grados_x10 = 688;
    caso("el texto exportado trae el FOV y se lee de vuelta igual", cal_a_texto(&cal, texto, sizeof texto) > 0 && strstr(texto, "fov_grados_x10=688\n") != NULL &&
         cal_de_texto(texto, &leida) == CAL_OK && memcmp(&cal, &leida, sizeof cal) == 0);
    caso("un texto de la version anterior (8 claves, sin FOV) se sigue leyendo y deja el FOV por defecto",
         cal_de_texto("medidos=15\narranque_ms=140\nms_por_cm_x10=420\nfrenado_mm=25\nms_por_grado_izq_x10=90\nms_por_grado_der_x10=110\n"
                      "frenado_grados_izq_x10=50\nfrenado_grados_der_x10=40\n", &leida) == CAL_OK && leida.fov_grados_x10 == 600 && leida.arranque_ms == 140);
    caso("un FOV repetido en el texto es error", cal_de_texto("medidos=0\narranque_ms=0\nms_por_cm_x10=500\nfrenado_mm=0\nms_por_grado_izq_x10=100\n"
         "ms_por_grado_der_x10=100\nfrenado_grados_izq_x10=0\nfrenado_grados_der_x10=0\nfov_grados_x10=600\nfov_grados_x10=600\n", &leida) == CAL_ERR_TEXTO);
    caso("un FOV fuera de rango en el texto es error", cal_de_texto("medidos=0\narranque_ms=0\nms_por_cm_x10=500\nfrenado_mm=0\nms_por_grado_izq_x10=100\n"
         "ms_por_grado_der_x10=100\nfrenado_grados_izq_x10=0\nfrenado_grados_der_x10=0\nfov_grados_x10=5\n", &leida) == CAL_ERR_TEXTO);
}

int main(void)
{
    pruebas_estadistica();
    pruebas_defecto();
    pruebas_ajuste();
    pruebas_conversiones();
    pruebas_texto();
    pruebas_fov();
    pruebas_sin_ruido();
    pruebas_ruido();
    pruebas_disparates();

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}

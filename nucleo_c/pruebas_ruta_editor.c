/* pruebas_ruta_editor.c — pruebas de ruta_editor.c: agregar, mover, borrar, deshacer, plantillas, resumen y guardar/abrir (sin hardware). */
#include <stdio.h>
#include <string.h>
#include "ruta_editor.h"

static int casos = 0, fallos = 0;

static void caso(const char *nombre, int ok)
{
    casos++;
    if (!ok) {
        fallos++;
    }
    printf("%s  %s\n", ok ? "OK   " : "FALLO", nombre);
}

static int es(const EditorRuta *e, int i, int c, int f)
{
    return i < e->actual.n && e->actual.v[i].columna == c && e->actual.v[i].fila == f;
}

int main(void)
{
    EditorRuta e;
    ResumenRuta r;
    Punto celdas[MAX_PUNTOS];
    char texto[400];
    int n, i;

    /* ── Agregar ───────────────────────────────────────────────────────── */
    re_iniciar(&e);
    caso("recién iniciado: sin puntos y sin historial", e.actual.n == 0 && !re_puede_deshacer(&e) && !re_puede_rehacer(&e));
    caso("agregar un punto dentro: bien", re_agregar(&e, 2, 6) == RE_OK && e.actual.n == 1);
    caso("agregar fuera (columna 12): error de fuera", re_agregar(&e, 12, 3) == RE_ERR_FUERA && e.actual.n == 1);
    caso("agregar fuera (fila -1): error de fuera", re_agregar(&e, 3, -1) == RE_ERR_FUERA);
    caso("repetir el último punto: error de repetido", re_agregar(&e, 2, 6) == RE_ERR_REPETIDO && e.actual.n == 1);
    caso("un punto lejano se agrega (los pasos intermedios los pone re_expandir)", re_agregar(&e, 2, 2) == RE_OK && e.actual.n == 2);
    n = re_expandir(&e.actual, celdas, MAX_PUNTOS);
    caso("(2,6) a (2,2): 5 celdas vecinas", n == 5 && celdas[0].fila == 6 && celdas[4].fila == 2 && celdas[2].columna == 2);

    /* Diagonal y luego recto, igual que arrastrar el dedo. */
    re_limpiar(&e);
    re_agregar(&e, 0, 0);
    re_agregar(&e, 3, 1);
    n = re_expandir(&e.actual, celdas, MAX_PUNTOS);
    caso("(0,0) a (3,1): primero diagonal (1,1) y luego recto (2,1), (3,1)", n == 4 && celdas[1].columna == 1 && celdas[1].fila == 1 && celdas[2].columna == 2 && celdas[2].fila == 1 && celdas[3].columna == 3);

    /* ── Máximos ───────────────────────────────────────────────────────── */
    re_iniciar(&e);
    for (i = 0; i < RE_MAX_VERTICES; i++) {
        re_agregar(&e, i % 2, 0 + (i / 2) % 2);   /* ida y vuelta corta: no pasa de MAX_PUNTOS celdas */
    }
    caso("hay un tope de vértices (RE_MAX_VERTICES)", e.actual.n <= RE_MAX_VERTICES);
    re_iniciar(&e);
    re_agregar(&e, 0, 0);
    for (i = 1; i <= 5; i++) {
        re_agregar(&e, (i % 2) ? 11 : 0, (i % 2) ? 7 : 0);   /* cada tramo son 11 celdas: 1 + 5 x 11 = 56 celdas, todavía cabe */
    }
    caso("5 tramos largos (56 celdas) caben", e.actual.n == 6 && re_expandir(&e.actual, celdas, MAX_PUNTOS) == 56);
    caso("el sexto tramo pasaría de 64 celdas: error de larga y la ruta no cambia", re_agregar(&e, 0, 0) == RE_ERR_LARGA && e.actual.n == 6);

    /* ── Mover y borrar ────────────────────────────────────────────────── */
    re_iniciar(&e);
    re_agregar(&e, 1, 6);
    re_agregar(&e, 1, 3);
    re_agregar(&e, 4, 3);
    caso("mover el punto 1 a (2,3): bien", re_mover(&e, 1, 2, 3) == RE_OK && es(&e, 1, 2, 3));
    caso("mover fuera: error y no cambia", re_mover(&e, 1, 20, 3) == RE_ERR_FUERA && es(&e, 1, 2, 3));
    caso("mover un punto que no existe: error de índice", re_mover(&e, 9, 0, 0) == RE_ERR_INDICE);
    caso("mover encima del vecino: error de repetido", re_mover(&e, 1, 4, 3) == RE_ERR_REPETIDO && es(&e, 1, 2, 3));
    caso("mover al mismo lugar no es un cambio (no llena el historial)", (n = e.n_deshacer, re_mover(&e, 1, 2, 3) == RE_OK && e.n_deshacer == n));
    caso("borrar el punto 1: quedan 2", re_borrar_punto(&e, 1) == RE_OK && e.actual.n == 2 && es(&e, 1, 4, 3));
    caso("borrar un índice inexistente: error", re_borrar_punto(&e, 5) == RE_ERR_INDICE);
    re_iniciar(&e);
    re_agregar(&e, 1, 1);
    re_agregar(&e, 3, 1);
    re_agregar(&e, 1, 1);
    caso("borrar el del medio de A-B-A deja un solo A (sin repetidos seguidos)", re_borrar_punto(&e, 1) == RE_OK && e.actual.n == 1);

    /* Un arrastre entero (varios mover seguidos del mismo punto) es UN solo paso de deshacer. */
    re_iniciar(&e);
    re_agregar(&e, 1, 6);
    re_agregar(&e, 1, 3);
    n = e.n_deshacer;
    re_mover(&e, 1, 2, 3);
    re_mover(&e, 1, 3, 3);
    re_mover(&e, 1, 4, 3);
    caso("arrastrar un punto por 3 celdas guarda UN solo paso de deshacer", e.n_deshacer == n + 1 && es(&e, 1, 4, 3));
    re_fin_arrastre(&e);
    re_mover(&e, 1, 5, 3);
    caso("después de soltar, otro arrastre es otro paso", e.n_deshacer == n + 2);
    re_deshacer(&e);
    caso("deshacer vuelve al punto donde se soltó la primera vez", es(&e, 1, 4, 3));
    re_deshacer(&e);
    caso("y otro deshacer vuelve al punto de antes de arrastrar", es(&e, 1, 1, 3));

    /* ── Deshacer y rehacer ────────────────────────────────────────────── */
    re_iniciar(&e);
    re_agregar(&e, 1, 6);
    re_agregar(&e, 1, 3);
    re_agregar(&e, 4, 3);
    caso("tras 3 cambios se puede deshacer y no rehacer", re_puede_deshacer(&e) && !re_puede_rehacer(&e) && e.actual.n == 3);
    caso("deshacer quita el último punto", re_deshacer(&e) == RE_OK && e.actual.n == 2 && re_puede_rehacer(&e));
    caso("deshacer otra vez", re_deshacer(&e) == RE_OK && e.actual.n == 1);
    caso("rehacer lo vuelve", re_rehacer(&e) == RE_OK && e.actual.n == 2 && es(&e, 1, 1, 3));
    re_agregar(&e, 5, 3);
    caso("un cambio nuevo borra lo que se podía rehacer", !re_puede_rehacer(&e) && re_rehacer(&e) == RE_ERR_NADA);
    re_limpiar(&e);
    caso("limpiar deja la ruta vacía", e.actual.n == 0);
    caso("y se puede deshacer el limpiar", re_deshacer(&e) == RE_OK && e.actual.n == 3);
    re_iniciar(&e);
    caso("deshacer sin historial: error", re_deshacer(&e) == RE_ERR_NADA);
    for (i = 0; i < 40; i++) {
        re_agregar(&e, i % 12, (i / 12) % 8 == 0 ? 0 : 1);
    }
    caso("el historial tiene tope (RE_MAX_HISTORIAL)", e.n_deshacer <= RE_MAX_HISTORIAL);
    {
        int pasos = 0;
        while (re_deshacer(&e) == RE_OK) pasos++;
        caso("se puede deshacer hasta el tope y no más", pasos == e.n_rehacer && pasos <= RE_MAX_HISTORIAL);
    }

    /* ── Plantillas ────────────────────────────────────────────────────── */
    for (i = 0; i < RE_N_PLANTILLAS; i++) {
        char nombre[80];
        int ok;
        re_iniciar(&e);
        ok = re_plantilla(&e, i) == RE_OK && e.actual.n >= 2;
        for (n = 0; n < e.actual.n; n++) {
            ok = ok && e.actual.v[n].columna >= 0 && e.actual.v[n].columna < RE_COLUMNAS && e.actual.v[n].fila >= 0 && e.actual.v[n].fila < RE_FILAS;
        }
        r = re_resumen(&e, NULL);
        ok = ok && r.codigo == 0 && r.n_celdas >= 2 && r.largo_cm > 0;
        sprintf(nombre, "plantilla %d: cabe en la cuadrícula y se puede recorrer (%d celdas, %d cm)", i, r.n_celdas, r.largo_cm);
        caso(nombre, ok);
    }
    re_iniciar(&e);
    re_plantilla(&e, RE_PLANTILLA_RECTA);
    r = re_resumen(&e, NULL);
    caso("línea recta: 5 celdas de 20 cm = 100 cm y un solo giro (ninguno si ya mira al frente)", r.largo_cm == 100 && r.giros == 0 && r.n_celdas == 6);
    re_plantilla(&e, RE_PLANTILLA_CUADRADO);
    r = re_resumen(&e, NULL);
    caso("cuadrado de 40 cm: 4 lados de 40 cm = 160 cm y 3 giros (el último queda sin girar)", r.largo_cm == 160 && r.giros >= 3 && r.n_vertices == 5);
    caso("el cuadrado no tiene giros cerrados", (r.advertencias & RE_ADV_GIRO_CERRADO) == 0);
    re_plantilla(&e, RE_PLANTILLA_OCHO);
    r = re_resumen(&e, NULL);
    caso("el ocho tiene 9 vértices, largo mayor a 2 m y no pasa de 64 celdas", r.n_vertices == 9 && r.largo_cm > 200 && r.n_celdas <= MAX_PUNTOS && r.codigo == 0);
    re_plantilla(&e, RE_PLANTILLA_ZIGZAG);
    r = re_resumen(&e, NULL);
    caso("zigzag: 6 tramos de una celda = 120 cm y 5 giros o más", r.largo_cm == 120 && r.giros >= 5);
    caso("poner una plantilla se puede deshacer", re_deshacer(&e) == RE_OK && e.actual.n == 9);
    caso("plantilla desconocida: error de argumento", re_plantilla(&e, 9) == RE_ERR_ARGUMENTO);

    /* ── Resumen ───────────────────────────────────────────────────────── */
    re_iniciar(&e);
    r = re_resumen(&e, NULL);
    caso("sin puntos: advertencia de pocos puntos y todo en cero", (r.advertencias & RE_ADV_POCOS_PUNTOS) && r.largo_cm == 0 && r.tiempo_ms == 0);
    re_agregar(&e, 3, 3);
    r = re_resumen(&e, NULL);
    caso("un solo punto: pocos puntos", (r.advertencias & RE_ADV_POCOS_PUNTOS) != 0);
    re_iniciar(&e);
    re_agregar(&e, 3, 2);
    re_agregar(&e, 3, 5);
    r = re_resumen(&e, NULL);
    caso("ruta que va hacia ABAJO: el carro debe dar la vuelta (giro de 180): giro cerrado", (r.advertencias & RE_ADV_GIRO_CERRADO) != 0 && r.giros >= 1);
    re_iniciar(&e);
    re_agregar(&e, 3, 5);
    re_agregar(&e, 3, 2);
    r = re_resumen(&e, NULL);
    caso("ruta recta hacia arriba (3 celdas): 60 cm, sin giros y con tiempo estimado", r.largo_cm == 60 && r.giros == 0 && r.tiempo_ms > 0);
    {
        CalibracionCarro lenta;
        ResumenRuta r2;

        cal_defecto(&lenta);
        lenta.ms_por_cm_x10 = lenta.ms_por_cm_x10 * 2;   /* un carro el doble de lento */
        r2 = re_resumen(&e, &lenta);
        caso("con un carro más lento el tiempo estimado sube", r2.tiempo_ms > r.tiempo_ms && r2.largo_cm == r.largo_cm);
    }
    re_iniciar(&e);
    re_agregar(&e, 1, 5);
    re_agregar(&e, 3, 5);
    re_agregar(&e, 1, 5);
    r = re_resumen(&e, NULL);
    caso("ir y volver sobre el mismo camino: giro cerrado", (r.advertencias & RE_ADV_GIRO_CERRADO) != 0);

    /* ── Guardar y abrir ───────────────────────────────────────────────── */
    re_iniciar(&e);
    re_plantilla(&e, RE_PLANTILLA_CUADRADO);
    n = re_a_texto(&e, texto, sizeof texto);
    printf("      texto: %s\n", texto);
    caso("a texto: empieza con RUTA1;12x8 y tiene los 5 puntos", n > 10 && strncmp(texto, "RUTA1;12x8;", 11) == 0 && (int)strlen(texto) == n);
    {
        EditorRuta otro;
        re_iniciar(&otro);
        re_agregar(&otro, 0, 0);
        caso("de texto: lee la misma ruta", re_de_texto(texto, &otro) == RE_OK && otro.actual.n == e.actual.n && memcmp(otro.actual.v, e.actual.v, sizeof(Punto) * (size_t)e.actual.n) == 0);
        caso("al abrir se borra el historial", !re_puede_deshacer(&otro) && !re_puede_rehacer(&otro));
        caso("guardar y abrir otra vez da el mismo texto", (re_a_texto(&otro, texto + 200, 150) > 0) && strcmp(texto, texto + 200) == 0);
    }
    {
        EditorRuta vacia;
        re_iniciar(&vacia);
        n = re_a_texto(&vacia, texto, sizeof texto);
        caso("una ruta vacía también se guarda y se abre", n > 0 && re_de_texto(texto, &e) == RE_OK && e.actual.n == 0);
    }
    re_iniciar(&e);
    re_agregar(&e, 5, 5);
    caso("texto con otro tamaño de cuadrícula: error", re_de_texto("RUTA1;10x8;1,1;2,2", &e) == RE_ERR_TEXTO && e.actual.n == 1);
    caso("texto con un punto fuera: error y no cambia", re_de_texto("RUTA1;12x8;1,1;12,2", &e) == RE_ERR_TEXTO && e.actual.n == 1);
    caso("texto con basura al final: error", re_de_texto("RUTA1;12x8;1,1;2,2x", &e) == RE_ERR_TEXTO);
    caso("texto sin la cabecera: error", re_de_texto("1,1;2,2", &e) == RE_ERR_TEXTO);
    caso("texto con dos puntos iguales seguidos: error", re_de_texto("RUTA1;12x8;1,1;1,1", &e) == RE_ERR_TEXTO);
    caso("texto con coma de menos: error", re_de_texto("RUTA1;12x8;1;2,2", &e) == RE_ERR_TEXTO);
    caso("texto vacío: error", re_de_texto("", &e) == RE_ERR_TEXTO);
    caso("texto con ruta demasiado larga: error", re_de_texto("RUTA1;12x8;0,0;11,7;0,0;11,7;0,0;11,7;0,0", &e) == RE_ERR_TEXTO);
    caso("punteros nulos: error de argumento", re_de_texto(NULL, &e) == RE_ERR_ARGUMENTO && re_a_texto(NULL, texto, 100) == RE_ERR_ARGUMENTO && re_a_texto(&e, texto, 4) == RE_ERR_ARGUMENTO);
    {
        EditorRuta grande;
        char chico[24];
        re_iniciar(&grande);
        re_plantilla(&grande, RE_PLANTILLA_OCHO);
        caso("un texto que no cabe en el espacio dado: error y no escribe de más", re_a_texto(&grande, chico, sizeof chico) == RE_ERR_ARGUMENTO);
    }

    /* ── Nombres ───────────────────────────────────────────────────────── */
    caso("nombre «Cuadrado 40» sirve", re_nombre_valido("Cuadrado 40"));
    caso("nombre con acento «Ruta pequeña» sirve", re_nombre_valido("Ruta peque\xC3\xB1" "a"));
    caso("nombre con guion y guion bajo sirve", re_nombre_valido("ruta-1_b"));
    caso("nombre vacío no sirve", !re_nombre_valido(""));
    caso("nombre que empieza con espacio no sirve", !re_nombre_valido(" ruta"));
    caso("nombre que termina con espacio no sirve", !re_nombre_valido("ruta "));
    caso("nombre con barra no sirve", !re_nombre_valido("a/b"));
    caso("nombre con punto no sirve", !re_nombre_valido("ruta.txt"));
    caso("nombre de 24 letras sirve", re_nombre_valido("abcdefghijklmnopqrstuvwx"));
    caso("nombre de 25 letras no sirve", !re_nombre_valido("abcdefghijklmnopqrstuvwxy"));
    caso("nombre nulo no sirve", !re_nombre_valido(NULL));
    caso("textos de error con contenido", strlen(re_texto_error(RE_ERR_FUERA)) > 10 && strlen(re_texto_error(-99)) > 5);

    printf("\n%d casos, %d fallos\n", casos, fallos);
    return fallos == 0 ? 0 : 1;
}

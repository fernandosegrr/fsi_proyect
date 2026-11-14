/*
 * mundo_v2.c — Mundo sintético del banco autónomo v2 (ver mundo_v2.h). SIMULADO: nada de aquí es una medición.
 *
 * GUÍA DE LECTURA
 *  1. Construcción del mundo: mv_objeto / mv_prisma / mv_caja / mv_mueble / mv_parche.
 *  2. rayo_prisma(): intersección rayo-caja (método de las losas); lo usan la cámara y el láser.
 *  3. color_piso(): losetas con juntas (cobertura analítica de la junta dentro de la huella de la celda),
 *     manchas de luz, sombra propia y reflejo especular de una ventana.
 *  4. mv_render_celdas(): un rayo por media celda; después la "cámara": ganancia (exposición), balance de
 *     blancos, ruido y recorte a 0..255. La exposición y el balance automáticos se actualizan con el cuadro.
 *  5. mv_laser(): rejilla de rayos dentro del cono; señal = cobertura x reflectancia / distancia^2.
 *  6. mv_detector(): proyección de las esquinas del objeto + probabilidad de detección con memoria.
 *  7. mv_contacto(): ejes separadores entre el chasis girado y cada prisma bajo.
 */
#include <math.h>
#include <string.h>
#include "mundo_v2.h"

#define MV_PI 3.14159265358979323846

static double rad(double grados) { return grados * MV_PI / 180.0; }
static double limitar(double v, double a, double b) { return v < a ? a : (v > b ? b : v); }

double mv_uniforme(Mundo *m)
{
    m->azar = m->azar * 1664525u + 1013904223u;
    return (double)(m->azar >> 8) / 16777216.0;
}

double mv_normal(Mundo *m, double sigma)
{
    double s = 0.0;
    int i;

    for (i = 0; i < 6; i++) s += mv_uniforme(m);
    return (s - 3.0) * 1.41421356 * sigma;   /* la suma de 6 uniformes tiene desviación 0.707 */
}

void mv_iniciar(Mundo *m, uint32_t semilla)
{
    memset(m, 0, sizeof *m);
    m->azar = semilla * 2654435761u + 12345u;
    m->foto.loseta_mm = 330.0;
    m->foto.junta_mm = 4.0;
    m->foto.junta_factor = 0.80;
    m->foto.var_loseta = 0.02;
    m->foto.albedo[0] = 0.72; m->foto.albedo[1] = 0.70; m->foto.albedo[2] = 0.66;
    m->foto.sombra_propia = 1.0;
    m->foto.aec_auto = 1;
    m->foto.awb_auto = 1;
    m->foto.ganancia = 0.85;
    m->foto.wb[0] = m->foto.wb[1] = m->foto.wb[2] = 1.0;
    m->foto.objetivo_luma = 128.0;
    m->foto.ruido = 1.5;
    m->foto.escalon_ganancia = 1.0;
    m->foto.fondo[0] = 0.50; m->foto.fondo[1] = 0.48; m->foto.fondo[2] = 0.45;
    m->foto.brillo_piso = 0.0;
    m->det_falsos_por_inferencia = 0.005;
}

int mv_objeto(Mundo *m, const char *nombre, double recall, int clase, uint32_t aparece_ms, int en_ruta)
{
    MvObjeto *o;

    if (m->n_objetos >= MV_MAX_OBJETOS) return -1;
    o = &m->objetos[m->n_objetos];
    memset(o, 0, sizeof *o);
    o->nombre = nombre;
    o->recall = recall;
    o->clase = clase;
    o->aparece_ms = aparece_ms;
    o->en_ruta = en_ruta;
    o->bx0 = o->by0 = 1e18;
    o->bx1 = o->by1 = -1e18;
    return m->n_objetos++;
}

int mv_prisma(Mundo *m, int objeto, double x0, double y0, double x1, double y1, double z0, double z1,
              int r, int g, int b, double reflectancia)
{
    MvPrisma *q;
    MvObjeto *o;

    if (m->n_prismas >= MV_MAX_PRISMAS || objeto < 0 || objeto >= m->n_objetos) return -1;
    q = &m->prismas[m->n_prismas];
    q->x0 = x0 < x1 ? x0 : x1; q->x1 = x0 < x1 ? x1 : x0;
    q->y0 = y0 < y1 ? y0 : y1; q->y1 = y0 < y1 ? y1 : y0;
    q->z0 = z0; q->z1 = z1;
    q->rgb[0] = (unsigned char)r; q->rgb[1] = (unsigned char)g; q->rgb[2] = (unsigned char)b;
    q->reflectancia = reflectancia;
    q->objeto = objeto;
    q->fantasma = 0;
    o = &m->objetos[objeto];
    if (q->x0 < o->bx0) o->bx0 = q->x0;
    if (q->y0 < o->by0) o->by0 = q->y0;
    if (q->x1 > o->bx1) o->bx1 = q->x1;
    if (q->y1 > o->by1) o->by1 = q->y1;
    return m->n_prismas++;
}

int mv_caja(Mundo *m, const char *nombre, double cx, double cy, double ancho, double fondo, double alto,
            int r, int g, int b, double reflectancia, double recall, int clase, uint32_t aparece_ms)
{
    int o = mv_objeto(m, nombre, recall, clase, aparece_ms, 1);

    if (o < 0) return -1;
    mv_prisma(m, o, cx - ancho / 2, cy - fondo / 2, cx + ancho / 2, cy + fondo / 2, 0.0, alto, r, g, b, reflectancia);
    return o;
}

int mv_mueble(Mundo *m, const char *nombre, double cx, double cy, double ancho, double fondo, double pata_mm,
              double alto_asiento, double respaldo_mm, int r, int g, int b, double recall, int clase)
{
    int o = mv_objeto(m, nombre, recall, clase, 0, 1);
    int i;

    if (o < 0) return -1;
    for (i = 0; i < 4; i++) {
        double px = (i & 1) ? cx + ancho / 2 - pata_mm : cx - ancho / 2;
        double py = (i & 2) ? cy + fondo / 2 - pata_mm : cy - fondo / 2;

        mv_prisma(m, o, px, py, px + pata_mm, py + pata_mm, 0.0, alto_asiento, r * 7 / 10, g * 7 / 10, b * 7 / 10, 0.45);
    }
    mv_prisma(m, o, cx - ancho / 2, cy - fondo / 2, cx + ancho / 2, cy + fondo / 2, alto_asiento, alto_asiento + 30.0, r, g, b, 0.5);
    if (respaldo_mm > 0.0) {   /* el respaldo va en el lado lejano (y menor = más adelante en una ruta hacia -y) */
        mv_prisma(m, o, cx - ancho / 2, cy - fondo / 2, cx + ancho / 2, cy - fondo / 2 + 30.0, alto_asiento, alto_asiento + respaldo_mm, r, g, b, 0.5);
    }
    {   /* La planta entera del mueble es zona prohibida: el carro NO debe meterse entre las patas (quedan 4 cm de holgura por lado). */
        int k = mv_prisma(m, o, cx - ancho / 2, cy - fondo / 2, cx + ancho / 2, cy + fondo / 2, 0.0, 10.0, 0, 0, 0, 0.0);

        if (k >= 0) m->prismas[k].fantasma = 1;
    }
    return o;
}

int mv_parche(Mundo *m, double cx, double cy, double hx, double hy, double rot_grados, double factor, double borde_mm)
{
    MvParche *p;

    if (m->foto.n_parches >= MV_MAX_PARCHES) return -1;
    p = &m->foto.parche[m->foto.n_parches];
    p->cx = cx; p->cy = cy; p->hx = hx; p->hy = hy;
    p->rot_rad = rad(rot_grados);
    p->factor = factor;
    p->borde_mm = borde_mm > 1.0 ? borde_mm : 1.0;
    if (factor > 1.0) { p->tinte[0] = 1.04; p->tinte[1] = 1.0; p->tinte[2] = 0.93; }   /* sol: algo más cálido */
    else              { p->tinte[0] = 0.97; p->tinte[1] = 1.0; p->tinte[2] = 1.05; }   /* sombra: algo más fría */
    return m->foto.n_parches++;
}

/* ── Rayo contra un prisma (losas). Devuelve 1 y la distancia de entrada si lo cruza por delante. ── */
static int losa(double o, double d, double a, double b, double *t0, double *t1)
{
    if (fabs(d) < 1e-12) {
        return o >= a && o <= b;
    }
    {
        double ta = (a - o) / d, tb = (b - o) / d;

        if (ta > tb) { double tt = ta; ta = tb; tb = tt; }
        if (ta > *t0) *t0 = ta;
        if (tb < *t1) *t1 = tb;
        return *t0 <= *t1;
    }
}

static int rayo_prisma(const MvPrisma *q, double ox, double oy, double oz, double dx, double dy, double dz, double *t_ent)
{
    double t0 = 0.0, t1 = 1e18;

    if (!losa(ox, dx, q->x0, q->x1, &t0, &t1)) return 0;
    if (!losa(oy, dy, q->y0, q->y1, &t0, &t1)) return 0;
    if (!losa(oz, dz, q->z0, q->z1, &t0, &t1)) return 0;
    if (t1 <= 1e-6) return 0;
    *t_ent = t0;
    return 1;
}

static int presente(const Mundo *m, const MvPrisma *q, uint32_t t_ms)
{
    return m->objetos[q->objeto].aparece_ms <= t_ms;
}

/* Lo que ven la cámara y el láser: los prismas presentes que no son solo de contacto. */
static int visible(const Mundo *m, const MvPrisma *q, uint32_t t_ms)
{
    return !q->fantasma && presente(m, q, t_ms);
}

/* Ruido estable por loseta: un número en [-1, 1] que depende solo de la loseta. */
static double ruido_loseta(long i, long j)
{
    uint32_t h = (uint32_t)(i * 73856093L) ^ (uint32_t)(j * 19349663L);

    h ^= h >> 13; h *= 1274126177u; h ^= h >> 16;
    return (double)(h & 0xFFFF) / 32767.5 - 1.0;
}

/* Fracción de un intervalo [c - h, c + h] que tapan las líneas de junta (ancho w, una cada T). */
static double cobertura_juntas(double c, double h, double T, double w)
{
    double n;

    if (h < 0.5) h = 0.5;
    n = floor((c + h) / T) - floor((c - h) / T);
    if (n <= 0.0) return 0.0;
    return limitar(n * w / (2.0 * h), 0.0, 1.0);
}

/* Color lineal (0..~4, 1 = blanco con luz ambiente) del punto del piso (px, py). `ha` y `hl` son el ancho y el largo de la huella de la
 * celda en el piso; (vx, vy) la dirección horizontal de la vista; (dx, dy, dz) el rayo que llega (para el reflejo). */
static void color_piso(const Mundo *m, MvPose carro, double px, double py, double ha, double hl, double vx, double vy,
                       double dx, double dy, double dz, double color[3])
{
    const MvFoto *f = &m->foto;
    double cr = cos(f->rot_rad), sr = sin(f->rot_rad);
    double qx = px - f->off_x, qy = py - f->off_y;
    double p = qx * cr + qy * sr, q = -qx * sr + qy * cr;
    double vp = fabs(vx * cr + vy * sr), vq = fabs(-vx * sr + vy * cr);
    double hp = 0.5 * (vp * hl + vq * ha), hq = 0.5 * (vq * hl + vp * ha);
    double cov_p = cobertura_juntas(p, hp, f->loseta_mm, f->junta_mm);
    double cov_q = cobertura_juntas(q, hq, f->loseta_mm, f->junta_mm);
    double cov = cov_p + cov_q - cov_p * cov_q;
    double var = 1.0 + f->var_loseta * ruido_loseta((long)floor(p / f->loseta_mm), (long)floor(q / f->loseta_mm));
    double luz[3] = { 1.0, 1.0, 1.0 };
    double espec = 0.0;
    int i, c;

    for (i = 0; i < f->n_parches; i++) {
        const MvParche *k = &f->parche[i];
        double ex = px - k->cx, ey = py - k->cy;
        double lx = ex * cos(k->rot_rad) + ey * sin(k->rot_rad), ly = -ex * sin(k->rot_rad) + ey * cos(k->rot_rad);
        double fuera_x = fabs(lx) - k->hx, fuera_y = fabs(ly) - k->hy;
        double fuera = fuera_x > fuera_y ? fuera_x : fuera_y;
        double w = limitar(0.5 - fuera / k->borde_mm, 0.0, 1.0);

        if (w > 0.0) {
            for (c = 0; c < 3; c++) luz[c] *= 1.0 + w * (k->factor * k->tinte[c] - 1.0);
        }
    }
    if (f->sombra_propia < 1.0) {   /* sombra del carro proyectada hacia adelante (luz a la espalda) */
        double fx = sin(carro.rumbo_rad), fy = -cos(carro.rumbo_rad);
        double ad = (px - carro.x) * fx + (py - carro.y) * fy, la = -(px - carro.x) * fy + (py - carro.y) * fx;
        double w = limitar(0.5 - (fabs(la) - 150.0) / 40.0, 0.0, 1.0) * limitar(0.5 - (ad - 430.0) / 60.0, 0.0, 1.0);

        for (c = 0; c < 3; c++) luz[c] *= 1.0 + w * (f->sombra_propia - 1.0);
    }
    if (f->reflejo && f->brillo_piso > 0.0 && dz < -1e-6) {   /* la ventana vista en el espejo del piso: rayo reflejado (dx, dy, -dz) */
        double den = dx * f->vent_nx + dy * f->vent_ny;

        if (fabs(den) > 1e-9) {
            double s = ((f->vent_x - px) * f->vent_nx + (f->vent_y - py) * f->vent_ny) / den;

            if (s > 0.0) {
                double hx = px + s * dx, hy = py + s * dy, hz = s * (-dz);
                double l = (hx - f->vent_x) * (-f->vent_ny) + (hy - f->vent_y) * f->vent_nx;
                double difuso = s * 0.05 + 1.0;   /* el piso no es un espejo perfecto: borde borroso de ~3 grados */
                double wl = limitar(0.5 + (f->vent_semiancho - fabs(l)) / difuso, 0.0, 1.0);
                double dzmin = (hz - f->vent_z0) < (f->vent_z1 - hz) ? (hz - f->vent_z0) : (f->vent_z1 - hz);
                double wz = limitar(0.5 + dzmin / difuso, 0.0, 1.0);
                double norma = sqrt(dx * dx + dy * dy + dz * dz);
                double cosi = norma > 0.0 ? -dz / norma : 1.0;
                double fresnel = 0.04 + 0.96 * pow(1.0 - cosi, 5.0);   /* a ras del piso el reflejo es mucho más fuerte */

                espec = f->brillo_piso * f->vent_brillo * wl * wz * (0.25 + 4.0 * fresnel);
            }
        }
    }
    for (c = 0; c < 3; c++) {
        color[c] = f->albedo[c] * var * (1.0 - cov * (1.0 - f->junta_factor)) * luz[c] + espec;
    }
}

void mv_render_celdas(Mundo *m, MvPose p, uint32_t t_ms, unsigned char *celdas)
{
    static double lin[MV_FILAS][MV_COLUMNAS][3];
    const double F = (MV_CAM_ANCHO_PX / 2.0) / tan(rad(MV_CAM_FOV_GRADOS / 2.0));
    const double st = sin(rad(MV_CAM_INCL_GRADOS)), ct = cos(rad(MV_CAM_INCL_GRADOS));
    double fx = sin(p.rumbo_rad), fy = -cos(p.rumbo_rad), rx = cos(p.rumbo_rad), ry = sin(p.rumbo_rad);
    double ox = p.x + fx * MV_CAM_ADELANTE_MM, oy = p.y + fy * MV_CAM_ADELANTE_MM, oz = MV_CAM_ALTURA_MM;
    MvFoto *f = &m->foto;
    int fila, col, sub, i, c;
    double suma[3] = { 0.0, 0.0, 0.0 }, suma_luma = 0.0;
    int n_wb = 0;

    for (fila = 0; fila < MV_FILAS; fila++) {
        double v = fila * 10.0 + 5.0;
        double b = (v - MV_CAM_ALTO_PX / 2.0) / F;
        double b_arr = (v - 5.0 - MV_CAM_ALTO_PX / 2.0) / F, b_aba = (v + 5.0 - MV_CAM_ALTO_PX / 2.0) / F;
        double dz = -(st + b * ct), dfw = ct - b * st;
        double huella_l = 0.0;

        if (st + b_arr * ct > 1e-6 && st + b_aba * ct > 1e-6) {   /* largo en el piso de las 10 filas de píxeles de la celda */
            double x_arr = oz / (st + b_arr * ct) * (ct - b_arr * st), x_aba = oz / (st + b_aba * ct) * (ct - b_aba * st);

            huella_l = fabs(x_arr - x_aba);
        }
        for (col = 0; col < MV_COLUMNAS; col++) {
            double acum[3] = { 0.0, 0.0, 0.0 };

            for (sub = 0; sub < 2; sub++) {
                double u = col * 10.0 + 2.5 + 5.0 * sub;
                double a = (u - MV_CAM_ANCHO_PX / 2.0) / F;
                double dx = fx * dfw + rx * a, dy = fy * dfw + ry * a;
                double t_piso = (dz < -1e-9) ? oz / (-dz) : 1e18;
                double t_min = 1e18;
                int mejor = -1;

                for (i = 0; i < m->n_prismas; i++) {
                    double t;

                    if (visible(m, &m->prismas[i], t_ms) && rayo_prisma(&m->prismas[i], ox, oy, oz, dx, dy, dz, &t) && t < t_min) {
                        t_min = t;
                        mejor = i;
                    }
                }
                if (mejor >= 0 && t_min < t_piso) {
                    for (c = 0; c < 3; c++) acum[c] += m->prismas[mejor].rgb[c] / 255.0 * 0.9;
                } else if (t_piso < 6000.0) {
                    double color[3];

                    color_piso(m, p, ox + dx * t_piso, oy + dy * t_piso, t_piso * 5.0 / F, huella_l, fx, fy, dx, dy, dz, color);
                    for (c = 0; c < 3; c++) acum[c] += color[c];
                } else {
                    for (c = 0; c < 3; c++) acum[c] += f->fondo[c];
                }
            }
            for (c = 0; c < 3; c++) lin[fila][col][c] = acum[c] / 2.0;
        }
    }
    if (f->escalon_ganancia != 1.0 && t_ms >= f->escalon_ms) {   /* cambio brusco de exposición (la cámara reacciona a algo fuera del modelo) */
        f->ganancia *= f->escalon_ganancia;
        f->escalon_ganancia = 1.0;
    }
    for (fila = 0; fila < MV_FILAS; fila++) {
        for (col = 0; col < MV_COLUMNAS; col++) {
            double px[3];
            int saturada = 0;

            for (c = 0; c < 3; c++) {
                double val = f->ganancia * f->wb[c] * lin[fila][col][c] * 255.0 + mv_normal(m, f->ruido);

                if (val >= 255.0) { val = 255.0; saturada = 1; }
                if (val < 0.0) val = 0.0;
                px[c] = val;
                celdas[(fila * MV_COLUMNAS + col) * 3 + c] = (unsigned char)(val + 0.5);
            }
            suma_luma += 0.299 * px[0] + 0.587 * px[1] + 0.114 * px[2];
            if (!saturada) {
                for (c = 0; c < 3; c++) suma[c] += px[c];
                n_wb++;
            }
        }
    }
    if (f->aec_auto) {   /* exposición automática: lleva la luminancia media del cuadro al objetivo, poco a poco */
        double media = suma_luma / (MV_FILAS * MV_COLUMNAS);

        if (media > 1.0) f->ganancia = limitar(f->ganancia * pow(f->objetivo_luma / media, 0.25), 0.15, 6.0);
    }
    if (f->awb_auto && n_wb > 20 && suma[0] > 1.0 && suma[2] > 1.0) {   /* balance de blancos de "mundo gris" */
        f->wb[0] = limitar(f->wb[0] * pow(suma[1] / suma[0], 0.08), 0.6, 1.6);
        f->wb[2] = limitar(f->wb[2] * pow(suma[1] / suma[2], 0.08), 0.6, 1.6);
    }
}

int mv_laser(Mundo *m, MvPose p, uint32_t t_ms, int *mm)
{
    enum { NH = 9, NV = 9 };
    const double medio = rad(MV_LASER_CONO_GRADOS / 2.0);
    double fx = sin(p.rumbo_rad), fy = -cos(p.rumbo_rad), rx = cos(p.rumbo_rad), ry = sin(p.rumbo_rad);
    double ox = p.x + fx * MV_LASER_ADELANTE_MM, oy = p.y + fy * MV_LASER_ADELANTE_MM, oz = MV_LASER_ALTURA_MM;
    double dist[NH * NV], refl[NH * NV], peso[NH * NV];
    double corr = (mv_uniforme(m) - 0.5) * 2.0 * medio / (NH - 1);   /* la rejilla de rayos se corre un poco en cada lectura */
    double peso_total = 0.0;
    int n_golpes = 0, ih, iv, i, intento;

    *mm = (int)MV_LASER_ALCANCE_MM;
    for (ih = 0; ih < NH; ih++) {
        for (iv = 0; iv < NV; iv++) {
            double ah = ((double)ih / (NH - 1) - 0.5) * 2.0 * medio + corr;
            double av = ((double)iv / (NV - 1) - 0.5) * 2.0 * medio;
            double dx, dy, dz, t_piso, t_min = 1e18, r_min = 0.0, w;

            if (ah * ah + av * av > medio * medio * 1.02) continue;   /* fuera del cono */
            /* El sensor es más sensible en el eje que en el borde del cono (SUPUESTO: campana de ~8 grados). */
            w = exp(-(ah * ah + av * av) / (rad(8.0) * rad(8.0)));
            peso_total += w;
            dx = (fx * cos(ah) + rx * sin(ah)) * cos(av);
            dy = (fy * cos(ah) + ry * sin(ah)) * cos(av);
            dz = sin(av);
            t_piso = (dz < -1e-9) ? oz / (-dz) : 1e18;
            for (i = 0; i < m->n_prismas; i++) {
                double t;

                if (m->prismas[i].reflectancia > 0.0 && visible(m, &m->prismas[i], t_ms) &&
                    rayo_prisma(&m->prismas[i], ox, oy, oz, dx, dy, dz, &t) && t < t_min) {
                    t_min = t;
                    r_min = m->prismas[i].reflectancia;
                }
            }
            if (t_min < t_piso && t_min <= MV_LASER_ALCANCE_MM) {
                dist[n_golpes] = t_min;
                refl[n_golpes] = r_min;
                peso[n_golpes] = w;
                n_golpes++;
            }
        }
    }
    if (peso_total <= 0.0 || mv_uniforme(m) < 0.02) return 0;   /* 2 % de lecturas perdidas */
    for (intento = 0; intento < 3 && n_golpes > 0; intento++) {   /* del grupo más cercano al más lejano */
        double d0 = 1e18, suma_r = 0.0, senal, dd;
        int k = 0;

        for (i = 0; i < n_golpes; i++) if (dist[i] < d0) d0 = dist[i];
        for (i = 0; i < n_golpes; i++) {
            if (dist[i] <= d0 + 80.0) suma_r += refl[i] * peso[i];
        }
        dd = d0 > 30.0 ? d0 : 30.0;
        /* Normalizada: un blanco de cartón (0.5) que llena el cono se ve justo hasta el alcance. */
        senal = (suma_r / peso_total) / 0.5 * (MV_LASER_ALCANCE_MM / dd) * (MV_LASER_ALCANCE_MM / dd);
        if (senal >= 1.0) {
            double leido = d0 * (1.0 + mv_normal(m, 0.02)) + mv_normal(m, 3.0);

            *mm = (int)limitar(leido, 8.0, MV_LASER_ALCANCE_MM);
            return 1;
        }
        for (i = 0; i < n_golpes; i++) {   /* ese grupo no devuelve suficiente: se mira lo que hay detrás */
            if (dist[i] > d0 + 80.0) { dist[k] = dist[i]; refl[k] = refl[i]; peso[k] = peso[i]; k++; }
        }
        n_golpes = k;
    }
    return 0;
}

void mv_detector(Mundo *m, MvPose p, uint32_t t_ms, uint32_t marca_ms, DeteccionesModelo *salida)
{
    const double F = (MV_CAM_ANCHO_PX / 2.0) / tan(rad(MV_CAM_FOV_GRADOS / 2.0));
    const double st = sin(rad(MV_CAM_INCL_GRADOS)), ct = cos(rad(MV_CAM_INCL_GRADOS));
    double fx = sin(p.rumbo_rad), fy = -cos(p.rumbo_rad), rx = cos(p.rumbo_rad), ry = sin(p.rumbo_rad);
    double ox = p.x + fx * MV_CAM_ADELANTE_MM, oy = p.y + fy * MV_CAM_ADELANTE_MM;
    int o, i, k;

    memset(salida, 0, sizeof *salida);
    salida->marca_ms = marca_ms;
    for (o = 0; o < m->n_objetos; o++) {
        MvObjeto *ob = &m->objetos[o];
        double u0 = 1e18, v0 = 1e18, u1 = -1e18, v1 = -1e18, cu0, cv0, cu1, cv1, area, area_rec, prob;
        int delante = 0, visto = 0;

        if (ob->recall <= 0.0 || ob->aparece_ms > t_ms) { ob->det_estado = 0; continue; }
        for (i = 0; i < m->n_prismas; i++) {
            const MvPrisma *q = &m->prismas[i];

            if (q->objeto != o || q->fantasma) continue;
            for (k = 0; k < 8; k++) {
                double wx = (k & 1) ? q->x1 : q->x0, wy = (k & 2) ? q->y1 : q->y0, wz = (k & 4) ? q->z1 : q->z0;
                double ad = (wx - ox) * fx + (wy - oy) * fy, la = (wx - ox) * rx + (wy - oy) * ry, ar = wz - MV_CAM_ALTURA_MM;
                double prof = ad * ct - ar * st, abajo = -ad * st - ar * ct;

                if (prof > 30.0) {
                    double u = MV_CAM_ANCHO_PX / 2.0 + F * la / prof, v = MV_CAM_ALTO_PX / 2.0 + F * abajo / prof;

                    delante++;
                    if (u < u0) u0 = u;
                    if (u > u1) u1 = u;
                    if (v < v0) v0 = v;
                    if (v > v1) v1 = v;
                }
            }
        }
        if (delante < 4) { ob->det_estado = 0; continue; }
        cu0 = limitar(u0, 0.0, MV_CAM_ANCHO_PX); cu1 = limitar(u1, 0.0, MV_CAM_ANCHO_PX);
        cv0 = limitar(v0, 0.0, MV_CAM_ALTO_PX); cv1 = limitar(v1, 0.0, MV_CAM_ALTO_PX);
        area = (u1 - u0) * (v1 - v0);
        area_rec = (cu1 - cu0) * (cv1 - cv0);
        if (area_rec < 0.015 * MV_CAM_ANCHO_PX * MV_CAM_ALTO_PX || cv1 - cv0 < 12.0 || area <= 0.0) { ob->det_estado = 0; continue; }
        prob = ob->recall * limitar(area_rec / area * 1.3, 0.0, 1.0);
        if (area_rec > 0.60 * MV_CAM_ANCHO_PX * MV_CAM_ALTO_PX) prob *= 0.3;   /* demasiado cerca: solo se ve un pedazo */
        if (area_rec < 0.03 * MV_CAM_ANCHO_PX * MV_CAM_ALTO_PX) prob *= 0.5;   /* muy chico en la imagen */
        prob = ob->det_estado ? prob + 0.35 * (1.0 - prob) : prob * 0.8;       /* lo que se vio en la inferencia anterior tiende a seguir viéndose */
        visto = mv_uniforme(m) < prob;
        if ((visto || mv_uniforme(m) < 0.4 * prob) && salida->n < FUS_MAX_CAJAS) {
            CajaModelo *c = &salida->cajas[salida->n++];
            double jw = (cu1 - cu0) * 0.025, jh = (cv1 - cv0) * 0.025;

            c->clase = ob->clase;
            c->confianza_mil = visto ? 600 + (int)(330.0 * mv_uniforme(m)) : 300 + (int)(299.0 * mv_uniforme(m));   /* la segunda es una caja débil, bajo el umbral */
            c->xmin_mil = (int)limitar((cu0 + mv_normal(m, jw)) * 1000.0 / MV_CAM_ANCHO_PX, 0.0, 1000.0);
            c->xmax_mil = (int)limitar((cu1 + mv_normal(m, jw)) * 1000.0 / MV_CAM_ANCHO_PX, 0.0, 1000.0);
            c->ymin_mil = (int)limitar((cv0 + mv_normal(m, jh)) * 1000.0 / MV_CAM_ALTO_PX, 0.0, 1000.0);
            c->ymax_mil = (int)limitar((cv1 + mv_normal(m, jh)) * 1000.0 / MV_CAM_ALTO_PX, 0.0, 1000.0);
        }
        ob->det_estado = visto;
    }
    if (m->det_falso_vivo <= 0 && mv_uniforme(m) < m->det_falsos_por_inferencia) {   /* una caja inventada ("refrigerator" sobre la pared) */
        double cx = 150.0 + 700.0 * mv_uniforme(m), w = 200.0 + 300.0 * mv_uniforme(m);
        double ymax = 550.0 + 400.0 * mv_uniforme(m), h = 200.0 + 300.0 * mv_uniforme(m);

        m->det_falso.clase = (int)(90.0 * mv_uniforme(m));
        m->det_falso.confianza_mil = 600 + (int)(180.0 * mv_uniforme(m));
        m->det_falso.xmin_mil = (int)limitar(cx - w / 2, 0.0, 1000.0);
        m->det_falso.xmax_mil = (int)limitar(cx + w / 2, 0.0, 1000.0);
        m->det_falso.ymax_mil = (int)limitar(ymax, 0.0, 1000.0);
        m->det_falso.ymin_mil = (int)limitar(ymax - h, 0.0, 1000.0);
        m->det_falso_vivo = 1 + (int)(3.0 * mv_uniforme(m));
    }
    if (m->det_falso_vivo > 0) {
        if (salida->n < FUS_MAX_CAJAS) salida->cajas[salida->n++] = m->det_falso;
        m->det_falso_vivo--;
    }
}

/* Proyección de un prisma (rectángulo alineado a los ejes) sobre un eje (ax, ay): centro y semiancho. */
static void proyectar_prisma(const MvPrisma *q, double ax, double ay, double *centro, double *semi)
{
    double cx = (q->x0 + q->x1) / 2.0, cy = (q->y0 + q->y1) / 2.0;

    *centro = cx * ax + cy * ay;
    *semi = fabs(ax) * (q->x1 - q->x0) / 2.0 + fabs(ay) * (q->y1 - q->y0) / 2.0;
}

static int toca_prisma(const MvPrisma *q, MvPose p, double semi_largo, double semi_ancho)
{
    double fx = sin(p.rumbo_rad), fy = -cos(p.rumbo_rad), rx = cos(p.rumbo_rad), ry = sin(p.rumbo_rad);
    double ejes[4][2];
    int e;

    ejes[0][0] = 1.0; ejes[0][1] = 0.0;
    ejes[1][0] = 0.0; ejes[1][1] = 1.0;
    ejes[2][0] = fx;  ejes[2][1] = fy;
    ejes[3][0] = rx;  ejes[3][1] = ry;
    for (e = 0; e < 4; e++) {
        double ax = ejes[e][0], ay = ejes[e][1], cq, sq;
        double cc = p.x * ax + p.y * ay;
        double sc = fabs(fx * ax + fy * ay) * semi_largo + fabs(rx * ax + ry * ay) * semi_ancho;

        proyectar_prisma(q, ax, ay, &cq, &sq);
        if (fabs(cc - cq) > sc + sq) return 0;   /* hay un eje que los separa */
    }
    return 1;
}

int mv_contacto(const Mundo *m, MvPose p, uint32_t t_ms, int *objeto)
{
    int i;

    for (i = 0; i < m->n_prismas; i++) {
        const MvPrisma *q = &m->prismas[i];

        if (q->z0 >= MV_CARRO_ALTO_MM || !presente(m, q, t_ms)) continue;
        if (toca_prisma(q, p, MV_CARRO_LARGO_MM / 2.0, MV_CARRO_ANCHO_MM / 2.0)) {
            if (objeto != NULL) *objeto = q->objeto;
            return 1;
        }
    }
    return 0;
}

int mv_contacto_tipo(const Mundo *m, MvPose p, uint32_t t_ms, int *objeto)
{
    int i, bajo = 0;

    for (i = 0; i < m->n_prismas; i++) {
        const MvPrisma *q = &m->prismas[i];

        if (q->z0 >= MV_CARRO_ALTO_MM || !presente(m, q, t_ms)) continue;
        if (toca_prisma(q, p, MV_CARRO_LARGO_MM / 2.0, MV_CARRO_ANCHO_MM / 2.0)) {
            if (!q->fantasma) {
                if (objeto != NULL) *objeto = q->objeto;
                return 1;
            }
            if (!bajo && objeto != NULL) *objeto = q->objeto;
            bajo = 1;
        }
    }
    return bajo ? 2 : 0;
}

double mv_libre_adelante(const Mundo *m, MvPose p, uint32_t t_ms)
{
    double fx = sin(p.rumbo_rad), fy = -cos(p.rumbo_rad);
    double d;

    for (d = 0.0; d < 3000.0; d += 10.0) {
        MvPose q = p;

        q.x += fx * d;
        q.y += fy * d;
        if (mv_contacto(m, q, t_ms, NULL)) return d;
    }
    return 9999.0;
}

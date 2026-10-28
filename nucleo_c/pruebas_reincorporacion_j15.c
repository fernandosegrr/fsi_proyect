/* J15: mundo independiente del estimador. Solo números; nunca imágenes reales.
 * Recta de 2 m, caja de 20 x 20 cm a 1 m del centro inicial. El chasis se
 * aproxima por un rectángulo de 20 x 24 cm (supuesto conservado en ambos modos).
 * La curvatura derecha equivale a un rumbo medio de 2 a 6 grados en 150 cm.
 * Estos supuestos no sustituyen una medición física del carro y de la caja. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "simulador_navegador.h"
#include "sim_ruta.h"

#define CORRIDAS 400
#define HISTORIA 32
static Navegador nav;
static Simulador sim;
static uint32_t azar;
static int casos, fallos;
static const int senos[19] = {0,87,174,259,342,423,500,574,643,707,766,819,866,906,940,966,985,996,1000};

static int abs_i(int a) { return a < 0 ? -a : a; }
static int seno(int g)
{
    int s = 1, i, r;
    g = (g % 360 + 360) % 360;
    if (g > 180) { g -= 180; s = -1; }
    if (g > 90) g = 180 - g;
    i = g / 5; r = g % 5;
    return s * (senos[i] + (r ? (senos[i+1] - senos[i]) * r / 5 : 0));
}
static int entre(int a, int b)
{
    azar = azar * 1664525u + 1013904223u;
    return a + (int)((azar >> 8) % (uint32_t)(b-a+1));
}
/* Suma de 12 uniformes: aproximación normal, sigma 1.1/1.7 grados, tope ±3. */
static int error_giro(int sigma)
{
    int i, suma = 0;
    for (i=0; i<12; i++) suma += entre(-500,500);
    suma = suma * sigma / 1000;
    return suma < -30 ? -30 : (suma > 30 ? 30 : suma);
}
static void caso(const char *nombre, int ok)
{
    casos++; if (!ok) fallos++;
    printf("%s %s\n", ok ? "OK    " : "FALLO ", nombre);
}
/* Intersección rectángulo del carro/caja por cuatro ejes separadores.
 * Se comprueba en cada tick (menos de 1 cm entre muestras a 19.3 cm/s). */
static int toca_caja(void)
{
    int x=sim_x_mm(&sim)-400, y=sim_y_mm(&sim)-1000;
    int s=seno(sim_rumbo_grados(&sim)), c=seno(sim_rumbo_grados(&sim)+90);
    if (abs_i(x)*1000 > 100000 + abs_i(c)*100 + abs_i(s)*120) return 0;
    if (abs_i(y)*1000 > 100000 + abs_i(s)*100 + abs_i(c)*120) return 0;
    if (abs_i(x*c+y*s) > 100000 + 100*(abs_i(c)+abs_i(s))) return 0;
    if (abs_i(x*s-y*c) > 120000 + 100*(abs_i(c)+abs_i(s))) return 0;
    return 1;
}
/* Rayo láser desde el centro: intersección con la caja y rango de 1.2 m.
 * Pasos de 5 mm: resolución suficiente para el umbral de 400 mm. */
static int laser(void)
{
    int d, x=sim_x_mm(&sim), y=sim_y_mm(&sim), h=sim_rumbo_grados(&sim);
    for (d=5; d<1200; d+=5) {
        int px=x+seno(h)*d/1000, py=y-seno(h+90)*d/1000;
        if (abs_i(px-400)<=100 && abs_i(py-1000)<=100) return d;
    }
    return 1200;
}

typedef struct { int error, estimado, llego, toco, esquivas, giros_tiempo; } Resultado;
static Resultado correr(uint32_t semilla, int obstaculo, int ruta_l, int antes)
{
    Punto ruta[11], caja={2,5};
    int hx[HISTORIA], hy[HISTORIA], hh[HISTORIA];
    CalibracionCarro cal;
    Resultado r;
    int i, t, retraso, z, a, d;
    memset(&r,0,sizeof r);
    azar=semilla;
    for (i=0;i<11;i++) {
        ruta[i].columna = ruta_l && i>5 ? 2+i-5 : 2;
        ruta[i].fila = ruta_l && i>5 ? 5 : 10-i;
    }
    cal_defecto(&cal);
    cal.arranque_ms=100;
    cal.ms_por_cm_x10=518; /* 19.3 cm/s */
    cal.frenado_mm=30;
    cal.ms_por_grado_izq_x10=74; cal.ms_por_grado_der_x10=78;
    cal_desplazamiento_de_medida(-1,90,-90,90,&a,&d);
    cal_poner_desplazamiento(&cal,-1,a,d);
    cal_desplazamiento_de_medida(1,150,165,90,&a,&d);
    cal_poner_desplazamiento(&cal,1,a,d);
    navegador_iniciar(&nav);
    nav.sin_rumbo_medido=antes;
    navegador_poner_calibracion(&nav,&cal);
    navegador_poner_giroscopio(&nav,1);
    navegador_poner_sensor_distancia(&nav,1);
    navegador_poner_autoridad_laser(&nav,1);
    navegador_poner_requiere_camara(&nav,1);
    navegador_poner_pedir_ayuda(&nav,1);
    navegador_poner_latencia_visual(&nav,250);
    navegador_cargar_ruta(&nav,ruta,11);
    sim_iniciar(&sim,2,10,0);
    sim.real=cal;
    sim.desbalance_mil=entre(1006,1018);
    /* ±5 % en el pivote: variación supuesta (no hay dispersión medida). */
    i=entre(95,105);
    sim.real.desp_adelante_der_um=cal.desp_adelante_der_um*i/100;
    sim.real.desp_derecha_der_um=cal.desp_derecha_der_um*i/100;
    i=entre(95,105);
    sim.real.desp_adelante_izq_um=cal.desp_adelante_izq_um*i/100;
    sim.real.desp_derecha_izq_um=cal.desp_derecha_izq_um*i/100;
    sim.real.frenado_mm=entre(25,35); /* parte mecánica, no los 9–14 cm totales */
    sim.fw_giroscopio=1; sim.tof_activo=1; sim.laser_externo=1;
    sim.periodo_cuadro_ms=100; /* diez cuadros/s; entrega en ticks de 50 ms */
    retraso=entre(4,6); /* 200–300 ms de video */
    for(i=0;i<HISTORIA;i++) { hx[i]=400; hy[i]=2000; hh[i]=0; }
    navegador_comenzar(&nav,0);
    for(t=0;t<90000;t+=NAV_TICK_MS) {
        int hay[3]={0,0,0}, cerc[3]={0,0,0}, celdas[3]={0,0,0};
        int k=(t/NAV_TICK_MS)%HISTORIA, viejo=(k+HISTORIA-retraso)%HISTORIA;
        hx[k]=sim_x_mm(&sim); hy[k]=sim_y_mm(&sim); hh[k]=sim_rumbo_grados(&sim);
        if(obstaculo) sr_vista(hx[viejo],hy[viejo],hh[viejo],&caja,1,hay,cerc,celdas);
        sim.n_eventos=0;
        for(z=0;z<3;z++) if(hay[z]) sim_agregar_obstaculo_celdas(&sim,(uint32_t)t,(uint32_t)t+50,z,cerc[z],celdas[z]);
        sim.laser_externo_mm=obstaculo ? laser() : 1200;
        sim.fw_error_izq_x10=error_giro(11);
        sim.fw_error_der_x10=error_giro(17);
        sim_correr(&sim,&nav,NAV_TICK_MS);
        if(obstaculo && toca_caja()) r.toco=1;
        if(nav.estado==NAV_TERMINADO || nav.estado==NAV_ERROR || nav.estado==NAV_AYUDA) break;
    }
    r.llego=nav.estado==NAV_TERMINADO;
    r.error=ruta_l ? abs_i(sim_y_mm(&sim)-1000) : abs_i(sim_x_mm(&sim)-400);
    r.estimado=ruta_l ? abs_i(nav.y_mm-1000) : abs_i(nav.x_mm-400);
    r.esquivas=nav.esquivadas;
    r.giros_tiempo=nav.n_giros_tiempo;
    return r;
}
static void ordenar(int *v)
{
    int i,j;
    for(i=1;i<CORRIDAS;i++) { int n=v[i]; for(j=i;j>0 && v[j-1]>n;j--) v[j]=v[j-1]; v[j]=n; }
}
typedef struct { int media, p95, exito, toques, llegan, esquivas, decenas; } Resumen;
static Resumen monte_carlo(int obstaculo, int ruta_l, int antes)
{
    Resumen resumen;
    int i, errores[CORRIDAS], suma=0, toques=0, llegan=0, exito=0, decenas=0, esquivas=0;
    for(i=0;i<CORRIDAS;i++) {
        Resultado r=correr(15001u+(uint32_t)i,obstaculo,ruta_l,antes);
        errores[i]=r.error; suma+=r.error; toques+=r.toco; llegan+=r.llego;
        exito+=r.llego && !r.toco && r.error<200;
        decenas+=r.llego && r.error>=200;
        esquivas+=r.esquivas;
    }
    ordenar(errores);
    printf("J15 %s escenario=%s n=%d media_mm=%d p50_mm=%d p95_mm=%d max_mm=%d toques=%d llegan=%d exito=%d decenas=%d esquivas=%d\n",
           antes ? "ANTES" : "DESPUES",obstaculo ? "B6" : (ruta_l ? "L" : "recta"), CORRIDAS,suma/CORRIDAS,errores[199],errores[379],errores[399],toques,llegan,exito,decenas,esquivas);
    caso("mundo: las 400 simulaciones terminan la ruta",llegan==CORRIDAS);
    resumen.media=suma/CORRIDAS; resumen.p95=errores[379]; resumen.exito=exito;
    resumen.toques=toques; resumen.llegan=llegan; resumen.esquivas=esquivas; resumen.decenas=decenas;
    return resumen;
}

static void preparar_avance(int gyro)
{
    Punto ruta[11];
    CalibracionCarro cal;
    int i;
    for(i=0;i<11;i++) { ruta[i].columna=2; ruta[i].fila=10-i; }
    cal_defecto(&cal); cal.ms_por_cm_x10=1000;
    navegador_iniciar(&nav);
    navegador_poner_calibracion(&nav,&cal);
    navegador_poner_giroscopio(&nav,gyro);
    navegador_cargar_ruta(&nav,ruta,11);
    navegador_comenzar(&nav,0);
    navegador_aporte_sens_imu(&nav,1200,1,0,1234,1,0);
    navegador_tick(&nav,0,NULL);
}
static void avanzar_hasta(int desde, int hasta, int angulo, int valida)
{
    int t;
    for(t=desde;t<=hasta;t+=50) {
        navegador_aporte_sens_imu(&nav,1200,1,0,angulo,valida,(uint32_t)t);
        navegador_tick(&nav,(uint32_t)t,NULL);
    }
}
static void pruebas_pose(void)
{
    int x, y, t;
    preparar_avance(1);
    avanzar_hasta(50,1000,1234,1);
    caso("el cero absoluto del IMU no cambia la alineacion inicial",nav.x_mm==400 && nav.y_mm==1900 && nav.rumbo_grados==0);
    preparar_avance(1);
    avanzar_hasta(50,500,1234,1);
    avanzar_hasta(550,1000,1134,1); /* diez grados a la derecha */
    caso("integra solo el avance posterior al giro, sin rotar el historial",nav.x_mm>=406 && nav.x_mm<=410 && nav.y_mm>=1900 && nav.y_mm<=1903 && nav.rumbo_grados==10);
    caso("el rumbo real se conserva para planear la reincorporacion",nav.avance_rumbo_x10==100);
    preparar_avance(0);
    avanzar_hasta(50,1000,1134,1);
    caso("interruptor de giroscopio apagado conserva el comportamiento anterior",nav.x_mm==400 && nav.rumbo_grados==0);
    preparar_avance(1);
    avanzar_hasta(50,1000,100,0);
    caso("IMU caida/calibrando no orienta la pose ni desactiva el laser",nav.x_mm==400 && !nav.sens_angulo_ok && nav.tof_vivo);
    avanzar_hasta(1050,1100,100,1);
    caso("recuperar IMU tras reinicio crea una referencia sin salto",nav.rumbo_grados==0 && nav.x_mm==400);
    preparar_avance(1);
    avanzar_hasta(50,500,1234,1);
    avanzar_hasta(550,600,2000,1);
    caso("un salto mayor de 25 grados al avanzar no inventa desplazamiento",nav.rumbo_grados==0 && nav.x_mm==400);
    preparar_avance(1);
    avanzar_hasta(50,500,1134,1);
    x=nav.x_mm; y=nav.y_mm;
    for(t=550;t<=1000;t+=50) navegador_tick(&nav,(uint32_t)t,NULL);
    caso("lecturas atrasadas mantienen ultimo rumbo, sin salto hacia la pose vieja",nav.x_mm>=x && nav.y_mm<y && nav.rumbo_grados==10);
    preparar_avance(1);
    nav.avance_imu_ref_x10=3590;
    avanzar_hasta(50,500,10,1);
    caso("cruzar 360 grados usa la diferencia corta y el signo del firmware",nav.rumbo_grados==358);
    navegador_cancelar(&nav);
    caso("cancelar conserva STOP con la nueva integracion",navegador_tick(&nav,550,NULL)==NAV_CMD_STOP);
}
int main(void)
{
    int antes, escenario;
    Resumen resumen[2][3];
    pruebas_pose();
    for(antes=1;antes>=0;antes--) {
        resumen[antes][0]=monte_carlo(1,0,antes);
        resumen[antes][1]=monte_carlo(0,0,antes);
        resumen[antes][2]=monte_carlo(0,1,antes);
    }
    caso("B6 base reproduce error lateral de decenas de cm tras esquivar",resumen[1][0].decenas>0 && resumen[1][0].esquivas>=CORRIDAS);
    caso("B6 corregido: al menos 90 por ciento llegan sin toque y a menos de 20 cm",resumen[0][0].exito>=360 && resumen[0][0].toques==0 && resumen[0][0].esquivas>=CORRIDAS);
    caso("B6 corregido mejora media y p95 del error lateral",resumen[0][0].media<resumen[1][0].media && resumen[0][0].p95<resumen[1][0].p95);
    for(escenario=1;escenario<3;escenario++)
        caso("recta/L sin obstaculo: no degradan media, p95 ni terminaciones",resumen[0][escenario].media<=resumen[1][escenario].media && resumen[0][escenario].p95<=resumen[1][escenario].p95 && resumen[0][escenario].llegan>=resumen[1][escenario].llegan);
    printf("%d casos, %d fallos\n",casos,fallos);
    return fallos ? EXIT_FAILURE : EXIT_SUCCESS;
}

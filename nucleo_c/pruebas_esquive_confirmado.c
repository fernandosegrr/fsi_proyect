/* Mundo rectangular independiente: mm, rumbo 0 al norte; solo software. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "simulador_navegador.h"

static Navegador nav;
static Simulador sim;
static int casos, fallos;
static void caso(const char *nombre, int ok) {
    casos++; if(!ok)fallos++;
    printf("%s %s\n",ok ? "OK    " : "FALLO ",nombre);
}
typedef struct { const char *nombre; int ancho, fondo, distancia, ruta, falso, perdida, sin_laser; } Mundo;
static const Mundo mundos[] = {
    {"caja20_laser",200,200,800,2000,0,0,0},
    {"banquito35x45_laser",350,450,800,2000,0,0,0},
    {"falso_sin_objeto",0,0,800,2000,1,0,0},
    {"falso_objeto80_ruta60",350,450,800,600,1,0,0},
    {"patas_sin_eco",350,450,800,2000,0,1,0},
    {"vision_sin_laser",200,200,350,2000,0,0,1},
    {"objeto_cerca_final",200,200,350,600,0,0,0},
    {"objeto_mayor_90cm",350,900,800,2000,0,0,0},
    {"falso_tres_zonas",0,0,800,2000,2,0,0}
};
static double sn, cs;
static void direccion(void) {
    double a = sim_rumbo_cgrados(&sim) * 3.141592653589793 / 18000.0;
    sn = sin(a); cs = cos(a);
}
static int laser(const Mundo *m) {
    int d;
    double frente = 2000 - 120 - m->distancia;
    if (!m->ancho) return 1200;
    for (d=5; d<1200; d+=5) {
        double x=sim_x_mm(&sim)+sn*(120+d), y=sim_y_mm(&sim)-cs*(120+d);
        if (fabs(x-1000)<=m->ancho/2.0 && y<=frente && y>=frente-m->fondo) return d;
    }
    return 1200;
}
/* Proyecta puntos del rectangulo sobre tres tercios del cono de 60 grados.
 * No modela pixeles/iluminacion: son entradas controladas de percepcion. */
static void vision(const Mundo *m, int hay[3], int cerc[3], int celdas[3]) {
    int ix, iy, z;
    double cerca=1e9, izq=1e9, der=-1e9;
    double frente=2000-120-m->distancia;
    memset(hay,0,3*sizeof(int)); memset(cerc,0,3*sizeof(int)); memset(celdas,0,3*sizeof(int));
    if (m->ancho) for(ix=-m->ancho/2;ix<=m->ancho/2;ix+=m->ancho) for(iy=0;iy<=m->fondo;iy+=m->fondo) {
        double dx=1000+ix-sim_x_mm(&sim), dy=frente-iy-sim_y_mm(&sim);
        double a=dx*sn-dy*cs, l=dx*cs+dy*sn;
        if(a<cerca)cerca=a;
        if(l<izq)izq=l;
        if(l>der)der=l;
    }
    if(cerca>0 && cerca<800) for(z=0;z<3;z++) {
        double borde=(cerca+m->fondo/2.0)*0.192;
        double desde=z==0 ? -3*borde : (z==1 ? -borde : borde);
        double hasta=z==0 ? -borde : (z==1 ? borde : 3*borde);
        double solape=fmin(der,hasta)-fmax(izq,desde);
        if(solape>=40) {hay[z]=1;cerc[z]=cerca<=300 ? 3 : (cerca<=500 ? 2 : 1);celdas[z]=30;}
    }
    /* Artefacto persistente hasta que cambia la vista por el giro. */
    if(m->falso && fabs(sn)<0.1 && cs>0 && nav.esquivadas==0) {
        for(z=0;z<3;z++) if(m->falso==2 || z==2) {hay[z]=1;cerc[z]=3;celdas[z]=65;}
    }
}
static void correr(int e, int verificar) {
    const Mundo *m=&mundos[e];
    Punto ruta[11]; CalibracionCarro cal;
    int t,i,n=m->ruta/200+1, disparo=-1, dist=-1, fin=-1, paralelo=0, choque=0;
    cal_defecto(&cal); cal.ms_por_cm_x10=518;
    navegador_iniciar(&nav); navegador_poner_calibracion(&nav,&cal);
    navegador_poner_giroscopio(&nav,1); navegador_poner_sensor_distancia(&nav,!m->sin_laser);
    navegador_poner_autoridad_laser(&nav,1); navegador_poner_requiere_camara(&nav,1);
    navegador_poner_pedir_ayuda(&nav,1);
    for(i=0;i<n;i++){ruta[i].columna=5;ruta[i].fila=10-i;}
    navegador_cargar_ruta(&nav,ruta,n); navegador_comenzar(&nav,0);
    sim_iniciar(&sim,5,10,0); sim.real=cal; sim.fw_giroscopio=1;
    sim.tof_activo=!m->sin_laser; sim.laser_externo=1; sim.periodo_cuadro_ms=100;
    for(t=0;t<90000;t+=NAV_TICK_MS) {
        int hay[3],cerc[3],celdas[3],z,mm;
        direccion(); vision(m,hay,cerc,celdas); mm=laser(m);
        sim.laser_externo_mm=m->perdida ? 1200 : mm; sim.tof_sin_eco=m->perdida;
        sim.n_eventos=0;
        for(z=0;z<3;z++) if(hay[z]) sim_agregar_obstaculo_celdas(&sim,(uint32_t)t,(uint32_t)t+50,z,cerc[z],celdas[z]);
        sim_correr(&sim,&nav,NAV_TICK_MS);
        if(nav.estado==NAV_ESQUIVANDO && disparo<0) {disparo=t;dist=nav.tof_dist_mm;fin=nav.fin_obstaculo_mm;}
        if(nav.paralelo_mm>paralelo) paralelo=nav.paralelo_mm;
        /* Rectangulos orientados: SAT, chasis de 200 x 240 mm. */
        if(m->ancho) {
            double dx=sim_x_mm(&sim)-1000,dy=sim_y_mm(&sim)-(2000-120-m->distancia-m->fondo/2.0);
            double hx=m->ancho/2.0,hy=m->fondo/2.0;
            if(fabs(dx)<hx+120*fabs(sn)+100*fabs(cs) && fabs(dy)<hy+120*fabs(cs)+100*fabs(sn) &&
               fabs(dx*sn-dy*cs)<120+hx*fabs(sn)+hy*fabs(cs) && fabs(dx*cs+dy*sn)<100+hx*fabs(cs)+hy*fabs(sn)) choque=1;
        }
        if(getenv("REPRO_DIAG")) {char s[NAV_DIAG_LARGO];while(navegador_diag_siguiente(&nav,s,sizeof s)>0) printf("%d %s\n",t,s);}
        if(nav.estado==NAV_TERMINADO||nav.estado==NAV_AYUDA||nav.estado==NAV_ERROR) break;
    }
    sim_correr(&sim,&nav,600);
    printf("%s t=%d pose=(%d,%d,%d) estado=%s causa=%d esquivas=%d disparo_ms=%d dist_mm=%d fin_plan=%d paralelo=%d cola_s=%d fin_real=%d paso=%d toque=%d\n",
        m->nombre,t,sim_x_mm(&sim),sim_y_mm(&sim),sim_rumbo_grados(&sim),navegador_texto_estado(nav.estado),nav.error_causa,nav.esquivadas,disparo,dist,fin,paralelo,
        2000-sim_y_mm(&sim)-120,120+m->distancia+m->fondo,m->ancho ? 2000-sim_y_mm(&sim)-120>120+m->distancia+m->fondo : -1,choque);
    if(!verificar)return;
    caso(m->nombre, e==0 ? nav.estado==NAV_TERMINADO && nav.esquivadas==1 && nav.fin_verificado &&
         2000-sim_y_mm(&sim)-120>120+m->distancia+m->fondo : nav.estado==NAV_AYUDA);
    caso("ningun contacto ni movimiento al cerrar o pedir ayuda", !choque && nav.cmd_actual==NAV_CMD_STOP && sim.violaciones_seguridad==0);
    if(e==0 || e==1 || e==7) {
        caso("plan anclado a distancia frontal valida y dentro del maximo", dist>0 && dist<=NAV_REACCION_MAX_MM &&
             fin==2000-nav.salida_y_mm+NAV_SENSOR_FRENTE_MM+dist+NAV_OBSTACULO_PROFUNDO_MM+NAV_PASO_MARGEN_MM);
    } else {
        caso("sin distancia cercana no hay giro de esquive", nav.esquivadas==0 && nav.n_giros_angulo==0);
    }
}

static ResultadoObstaculos libre, ocupado;
static void preparar(void) {
    Punto ruta[11]; int i;
    navegador_iniciar(&nav);
    navegador_poner_sensor_distancia(&nav,1);
    navegador_poner_pedir_ayuda(&nav,1);
    for(i=0;i<11;i++){ruta[i].columna=5;ruta[i].fila=10-i;}
    navegador_cargar_ruta(&nav,ruta,11); navegador_comenzar(&nav,0);
    memset(&libre,0,sizeof libre); memset(&ocupado,0,sizeof ocupado);
    ocupado.zona[ZONA_CENTRO].hay_obstaculo=1;
    ocupado.zona[ZONA_CENTRO].cercania=3;
    ocupado.zona[ZONA_CENTRO].celdas=65;
}
static void limites(void) {
    int t, mm, z;
    /* La misma zona durante muchos cuadros no inventa profundidad. */
    for(mm=500;mm<=501;mm++) {
        preparar();
        for(t=0;t<=1400;t+=100) {
            navegador_aporte_sens(&nav,mm,1,1,0,(uint32_t)t);
            navegador_tick(&nav,(uint32_t)t,&ocupado);
        }
        caso(mm==500 ? "500 mm confirmado: autoriza un esquive" : "501 mm persistente: AYUDA sin esquive",
             mm==500 ? nav.esquivadas==1 : nav.estado==NAV_AYUDA && nav.esquivadas==0);
    }
    preparar();
    for(t=0;t<=1400;t+=100) {
        navegador_aporte_sens(&nav,200,1,0,0,(uint32_t)t);
        navegador_tick(&nav,(uint32_t)t,&ocupado);
    }
    caso("mm cercano con flag invalido no confirma ni da libre",nav.estado==NAV_AYUDA && nav.esquivadas==0 && nav.tof_libre_lecturas==0);
    preparar();
    for(t=0;t<3;t++)navegador_aporte_sens(&nav,300,1,1,0,100);
    caso("SENS duplicado no cuenta como tres medidas",nav.tof_cerca_lecturas==1);
    navegador_aporte_sens(&nav,301,1,1,0,100);
    caso("mismo instante con otro mm tampoco suma confirmaciones",nav.tof_cerca_lecturas==1);
    for(t=500;t<=2100;t+=100)navegador_tick(&nav,(uint32_t)t,&ocupado);
    caso("medida vencida no confirma cuadros persistentes",nav.estado==NAV_AYUDA && nav.esquivadas==0);
    preparar();
    for(t=0;t<=1500;t+=100) {
        memset(&ocupado,0,sizeof ocupado);z=(t/100)%3;
        ocupado.zona[z].hay_obstaculo=1;ocupado.zona[z].cercania=3;ocupado.zona[z].celdas=65;
        navegador_tick(&nav,(uint32_t)t,&ocupado);
    }
    caso("zonas alternadas sin laser no autorizan maniobra",nav.esquivadas==0 && nav.cmd_actual==NAV_CMD_STOP);

    /* Todos los cierres pasan por la comprobacion, incluso al agotar los pasos. */
    for(mm=0;mm<4;mm++) {
        preparar();
        nav.indice_objetivo=nav.n_puntos-1; nav.indice_paso=nav.n_pasos;
        nav.x_mm=nav.base_x_mm=nav.prev_x_mm=1000;
        nav.y_mm=nav.base_y_mm=nav.prev_y_mm=0;
        for(t=0;t<=1500;t+=50) {
            navegador_aporte_sens(&nav,mm==0 || (mm==3 && t<1000) ? 300 : 1200,1,mm!=1,0,(uint32_t)t);
            navegador_tick(&nav,(uint32_t)t,mm==3 && t>100 ? NULL : &libre);
        }
        if(mm==0)caso("obstaculo en el ultimo tick: nunca TERMINADO",nav.estado==NAV_AYUDA && nav.error_causa==NAV_CAUSA_FIN_NO_LIBRE);
        if(mm==1)caso("sin eco al cerrar: AYUDA, no libre",nav.estado==NAV_AYUDA && nav.error_causa==NAV_CAUSA_FIN_NO_LIBRE);
        if(mm==2)caso("laser libre nuevo con carro parado permite cerrar",nav.estado==NAV_TERMINADO && nav.fin_verificado);
        if(mm==3)caso("perdida de video durante cierre no se convierte en exito",nav.estado==NAV_ERROR && nav.error_causa==NAV_CAUSA_SIN_VIDEO);
        caso("verificacion final siempre mantiene STOP",nav.cmd_actual==NAV_CMD_STOP);
    }
    preparar();
    nav.indice_objetivo=nav.n_puntos-1;nav.indice_paso=nav.n_pasos;
    nav.x_mm=nav.base_x_mm=nav.prev_x_mm=1000;
    nav.y_mm=nav.base_y_mm=nav.prev_y_mm=0;
    nav.obstaculo_pendiente=1;nav.fin_obstaculo_mm=2400;
    navegador_aporte_sens(&nav,1200,1,1,0,0);navegador_tick(&nav,0,&libre);
    caso("laser libre no tapa un plan que aun no paso el objeto",nav.estado==NAV_AYUDA && nav.error_causa==NAV_CAUSA_OBSTACULO_NO_PASADO);
}
int main(int argc,char **argv) {
    int i, informe=argc>1 && strcmp(argv[1],"--informe")==0;
    if(argc>1 && !informe) {i=atoi(argv[1]); if(i<0 || i>=9)return 2; correr(i,0);return 0;}
    for(i=0;i<(int)(sizeof mundos/sizeof mundos[0]);i++)correr(i,!informe);
    if(!informe){limites();printf("%d casos, %d fallos\n",casos,fallos);}
    return fallos ? EXIT_FAILURE : EXIT_SUCCESS;
}

/* Mundo rectangular independiente: mm, rumbo 0 al norte; solo software. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "simulador_navegador.h"
#include "fusion.h"

static Navegador nav;
static Simulador sim;
static int casos, fallos;
static void caso(const char *nombre, int ok) {
    casos++; if(!ok)fallos++;
    printf("%s %s\n",ok ? "OK    " : "FALLO ",nombre);
}
typedef struct { const char *nombre; int ancho, fondo, distancia, ruta, falso, perdida, sin_laser, fuente_falsa, laser_libre; } Mundo;
static const Mundo mundos[] = {
    {"caja20_laser",200,200,800,2000,0,0,0,0,0},
    {"banquito35x45_laser",350,450,800,2000,0,0,0,0,0},
    {"objeto_fondo90",350,900,800,2000,0,0,0,0,0},
    {"falso_solo_color",0,0,800,2000,1,0,0,1,0},
    {"falso_solo_detector",0,0,800,2000,1,0,0,2,0},
    {"coincidencia_laser_libre",350,450,800,2000,0,0,0,0,1},
    {"sin_eco_coincidencia",350,450,800,2000,0,1,0,0,0},
    {"sin_eco_solo_color",0,0,800,2000,1,1,0,1,0},
    {"falso_tres_zonas_color",0,0,800,2000,2,0,0,1,0},
    {"ruta_corta_objeto_final",200,200,350,600,0,0,0,0,0},
    {"falso_y_objeto80_ruta60",350,450,800,600,1,0,0,1,0}
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
/* Ejercita fusion.c real: los falsos se inyectan en una sola fuente. */
static void percibir(const Mundo *m, int t, ResultadoObstaculos *cuadro) {
    ResultadoObstaculos color;
    DeteccionesModelo red;
    ResultadoFusion fusion;
    ConfigFusion cfg;
    Mundo real = *m;
    int hay[3], cerc[3], celdas[3], z;
    real.falso = 0;
    vision(&real,hay,cerc,celdas);
    memset(&color,0,sizeof color); memset(&red,0,sizeof red);
    red.marca_ms = (unsigned int)t;
    for(z=0;z<3;z++) {
        int fuentes = hay[z] ? 3 : 0;
        if(m->falso && fabs(sn)<0.1 && cs>0 && nav.esquivadas==0 && (m->falso==2 || z==2)) {
            fuentes |= m->fuente_falsa; cerc[z]=3; celdas[z]=65;
        }
        if(fuentes&1) {color.zona[z].hay_obstaculo=1;color.zona[z].cercania=cerc[z];color.zona[z].celdas=celdas[z];}
        if(fuentes&2) {
            CajaModelo *c=&red.cajas[red.n++];
            c->clase=61;c->confianza_mil=900;c->xmin_mil=z*333+20;c->xmax_mil=z*333+310;
            c->ymax_mil=cerc[z]==3 ? 900 : cerc[z]==2 ? 750 : 550;c->ymin_mil=c->ymax_mil-100;
        }
    }
    fusion_config_defecto(&cfg);
    fusion_decidir(&cfg,&color,&red,(unsigned int)t,&fusion);
    *cuadro=color;
    for(z=0;z<3;z++) {
        cuadro->zona[z]=fusion.zona[z];
#ifndef REPRO_BASE
        cuadro->fuentes_vision[z]=fusion.fuentes_vision[z];
#endif
    }
}

static void correr(int e, int verificar) {
    const Mundo *m=&mundos[e];
    Punto ruta[11]; CalibracionCarro cal; ResultadoObstaculos cuadro;
    int t,i,n=m->ruta/200+1,disparo=-1,dist=-1,fin=-1,choque=0,paradas=0,antes=0;
    cal_defecto(&cal);cal.ms_por_cm_x10=518;
    navegador_iniciar(&nav);navegador_poner_calibracion(&nav,&cal);
    navegador_poner_giroscopio(&nav,1);navegador_poner_sensor_distancia(&nav,!m->sin_laser);
    navegador_poner_autoridad_laser(&nav,1);navegador_poner_requiere_camara(&nav,1);navegador_poner_pedir_ayuda(&nav,1);
    for(i=0;i<n;i++){ruta[i].columna=5;ruta[i].fila=10-i;}
    navegador_cargar_ruta(&nav,ruta,n);navegador_comenzar(&nav,0);
    sim_iniciar(&sim,5,10,0);sim.real=cal;sim.fw_giroscopio=1;
    sim.tof_activo=!m->sin_laser;sim.laser_externo=1;sim.periodo_cuadro_ms=100;sim.vision_externa=&cuadro;
    for(t=0;t<120000;t+=NAV_TICK_MS) {
        int mm;
        direccion();percibir(m,t,&cuadro);mm=laser(m);
        sim.laser_externo_mm=(m->perdida || m->laser_libre) ? 1200 : mm;sim.tof_sin_eco=m->perdida;
        sim_correr(&sim,&nav,NAV_TICK_MS);
        if(antes==NAV_CMD_FORWARD && nav.cmd_actual==NAV_CMD_STOP && nav.estado!=NAV_TERMINADO
#ifndef REPRO_BASE
           && !nav.fin_pendiente
#endif
          )paradas++;
        antes=nav.cmd_actual;
        if(nav.estado==NAV_ESQUIVANDO && disparo<0){disparo=t;dist=nav.tof_dist_mm;fin=nav.fin_obstaculo_mm;}
        if(m->ancho) {
            double dx=sim_x_mm(&sim)-1000,dy=sim_y_mm(&sim)-(2000-120-m->distancia-m->fondo/2.0);
            double hx=m->ancho/2.0,hy=m->fondo/2.0;
            direccion();
            if(fabs(dx)<hx+120*fabs(sn)+100*fabs(cs) && fabs(dy)<hy+120*fabs(cs)+100*fabs(sn) &&
               fabs(dx*sn-dy*cs)<120+hx*fabs(sn)+hy*fabs(cs) && fabs(dx*cs+dy*sn)<100+hx*fabs(cs)+hy*fabs(sn)) {
                if(!choque && getenv("REPRO_DIAG"))printf("CONTACTO t=%d pose=%d,%d,%d estado=%d fase=%d\n",t,sim_x_mm(&sim),sim_y_mm(&sim),sim_rumbo_grados(&sim),nav.estado,nav.reinc_fase);
                choque=1;
            }
        }
        if(getenv("REPRO_DIAG")){char s[NAV_DIAG_LARGO];while(navegador_diag_siguiente(&nav,s,sizeof s)>0)printf("%d %s\n",t,s);}
        if(nav.estado==NAV_TERMINADO||nav.estado==NAV_AYUDA||nav.estado==NAV_ERROR)break;
    }
    sim_correr(&sim,&nav,600);
    printf("%s t=%d pose=(%d,%d,%d) estado=%s causa=%d esquivas=%d dist_mm=%d fin_inicial=%d cola_s=%d fin_real=%d paso=%d toque=%d paradas=%d\n",
        m->nombre,t,sim_x_mm(&sim),sim_y_mm(&sim),sim_rumbo_grados(&sim),navegador_texto_estado(nav.estado),nav.error_causa,nav.esquivadas,dist,fin,
        2000-sim_y_mm(&sim)-120,120+m->distancia+m->fondo,m->ancho ? 2000-sim_y_mm(&sim)-120>120+m->distancia+m->fondo : -1,choque,paradas);
    if(!verificar)return;
    caso(m->nombre,nav.estado==NAV_TERMINADO && (!m->ancho || 2000-sim_y_mm(&sim)-120>120+m->distancia+m->fondo));
#ifndef REPRO_BASE
    caso("cierre confirmado",nav.fin_verificado);
#endif
    caso("sin contactos y STOP al finalizar",!choque && nav.cmd_actual==NAV_CMD_STOP && sim.violaciones_seguridad==0);
    if(!m->ancho)caso("fuente unica no provoca esquives ni ayuda",nav.esquivadas==0 && nav.ayudas==0);
    if(!m->ancho && !m->perdida)caso("laser libre y fuente unica: ninguna parada intermedia",paradas==0);
#ifndef REPRO_BASE
    /* V2: el fondo no se supone (ni los 250 mm de v1.1 ni tramos de 200): el paso en paralelo llega hasta donde el costado ya no puede estar a la vista de la camara,
     * o se alarga porque lo sigue viendo, o por un reintento de borde. */
    if(e==1 || e==2)caso("fondo explorado, sin profundidad fija",nav.borde_reintentos>0 || nav.flanco_vistas>0 || nav.fin_obstaculo_mm>nav.cara_obstaculo_s_mm+450);
    if(e==5 || e==6)caso("coincidencia permite plan visual sin laser",disparo>=0 && dist==0);
#endif
}

#ifndef REPRO_BASE
static void preparar_politica(void) {
    Punto ruta[11]; int i;
    navegador_iniciar(&nav);
    navegador_poner_sensor_distancia(&nav,1);
    navegador_poner_pedir_ayuda(&nav,1);
    for(i=0;i<11;i++){ruta[i].columna=5;ruta[i].fila=10-i;}
    navegador_cargar_ruta(&nav,ruta,11);navegador_comenzar(&nav,0);
}

static void probar_politica(void) {
    ResultadoObstaculos cuadro;
    int fuente,banda,z,t,modo;
    for(modo=0;modo<4;modo++)for(fuente=0;fuente<4;fuente++)for(banda=1;banda<=3;banda++)for(z=0;z<3;z++) {
        int confirmado=modo==1 || (fuente==3 && banda>=2);
        char nombre[100];
        preparar_politica();memset(&cuadro,0,sizeof cuadro);
        cuadro.fuentes_vision[z]=fuente;cuadro.zona[z].hay_obstaculo=fuente==3;
        cuadro.zona[z].cercania=fuente?banda:0;cuadro.zona[z].celdas=30;
        if(modo==3)navegador_aporte_sens(&nav,300,1,1,0,0);
        for(t=500;t<=950;t+=50) {
            /* modo 3: lectura vieja nunca confirma; los demas aportan muestras nuevas. */
            if(modo!=3)navegador_aporte_sens(&nav,modo==1?300:1200,1,modo!=2,0,(uint32_t)t);
            navegador_tick(&nav,(uint32_t)t,&cuadro);
        }
        snprintf(nombre,sizeof nombre,"fusion en navegador laser=%d fuente=%d zona=%d banda=%d",modo,fuente,z,banda);
        caso(nombre,confirmado ? nav.estado==NAV_DETENIDO_OBSTACULO && nav.cmd_actual==NAV_CMD_STOP :
             nav.estado==NAV_SIGUIENDO && nav.esquivadas==0 && nav.ayudas==0);
        if(modo==0 && fuente!=3)caso("laser libre y fuente aislada nunca pausa",nav.cmd_actual==NAV_CMD_FORWARD && !nav.precaucion);
    }
    preparar_politica();memset(&cuadro,0,sizeof cuadro);
    nav.fin_pendiente=1;
    for(t=50;t<=800;t+=50){navegador_aporte_sens(&nav,1200,1,1,0,50);navegador_tick(&nav,t,&cuadro);}
    caso("cierre no reutiliza muestras laser duplicadas",nav.estado!=NAV_TERMINADO && !nav.fin_verificado);
    for(t=850;t<=950;t+=50){navegador_aporte_sens(&nav,1200,1,1,0,t);navegador_tick(&nav,t,&cuadro);}
    caso("cierre acepta tres lecturas nuevas de frente libre",nav.estado==NAV_TERMINADO && nav.fin_verificado);
    preparar_politica();nav.fin_pendiente=1;
    cuadro.fuentes_vision[1]=3;cuadro.zona[1].hay_obstaculo=1;cuadro.zona[1].cercania=3;
    for(t=50;t<=900;t+=50){navegador_aporte_sens(&nav,1200,1,1,0,t);navegador_tick(&nav,t,&cuadro);}
    caso("cierre no ignora consenso visual por haz laser libre",nav.estado!=NAV_TERMINADO && !nav.fin_verificado);
    memset(&cuadro,0,sizeof cuadro);
    preparar_politica();nav.fin_pendiente=1;navegador_poner_sensor_distancia(&nav,0);
    navegador_tick(&nav,50,&cuadro);
    for(t=100;t<650;t+=50)navegador_tick(&nav,t,NULL);
    caso("sin eco tampoco reutiliza el mismo cuadro",nav.estado!=NAV_TERMINADO);
    for(t=650;t<=850;t+=50)navegador_tick(&nav,t,&cuadro);
    caso("sin eco cierra con cinco cuadros nuevos sin coincidencia",nav.estado==NAV_TERMINADO && nav.fin_verificado);
    preparar_politica();nav.fin_pendiente=1;nav.esquivadas=nav.esquivadas_obstaculo=NAV_MAX_ESQUIVADAS;
    /* V2: «sin liberar» es algo a NAV_FIN_HOLGURA_MM o menos (antes bastaba cualquier lectura de 800 mm o menos, y una pared a 60 cm impedía cerrar). */
    for(t=50;t<=1300;t+=50){navegador_aporte_sens(&nav,300,1,1,0,t);navegador_tick(&nav,t,&cuadro);}
    caso("frente sin liberar tras seis intentos: AYUDA, nunca Terminado",nav.estado==NAV_AYUDA && nav.error_causa==NAV_CAUSA_MAX_ESQUIVADAS && !nav.fin_verificado);
    preparar_politica();nav.fin_pendiente=1;nav.obstaculo_pendiente=1;
    for(t=50;t<=900;t+=50){navegador_aporte_sens(&nav,600,1,1,0,t);navegador_tick(&nav,t,&cuadro);}
    caso("V2: algo a 60 cm (una pared, otro mueble) no impide cerrar el rodeo",nav.estado==NAV_TERMINADO && nav.fin_verificado);
    preparar_politica();navegador_poner_requiere_camara(&nav,1);navegador_tick(&nav,50,&cuadro);
    navegador_tick(&nav,50+NAV_MAX_HUECO_VIDEO_MS+1,NULL);
    caso("perdida de vision detiene aunque el laser este libre",nav.estado==NAV_ERROR && nav.cmd_actual==NAV_CMD_STOP);
    preparar_politica();navegador_tick(&nav,0,&cuadro);
    navegador_aporte_tof_paro(&nav,120,50);navegador_tick(&nav,100,&cuadro);
    caso("TOF_PARO queda retenido aunque la camara este vacia",nav.estado==NAV_DETENIDO_OBSTACULO && nav.cmd_actual==NAV_CMD_STOP && nav.tof_dist_mm==120);
    for(t=150;t<=800;t+=50){navegador_aporte_sens(&nav,120,1,1,0,t);navegador_tick(&nav,t,&cuadro);}
    caso("TOF_PARO planea desde la distancia real despues del STOP",nav.estado==NAV_ESQUIVANDO && nav.tof_dist_mm==120 && nav.esquivadas==1);
    /* Bloqueo persistente: esperar cuatro segundos; solo retroceder por espacio recorrido. */
    for(modo=0;modo<2;modo++) {
        preparar_politica();memset(&cuadro,0,sizeof cuadro);
        for(t=0;t<2000;t+=50)navegador_tick(&nav,t,&cuadro);
        for(z=0;z<3;z++){cuadro.fuentes_vision[z]=3;cuadro.zona[z].hay_obstaculo=1;cuadro.zona[z].cercania=3;}
        for(t=2000;t<6000;t+=50){navegador_tick(&nav,t,&cuadro);if(!modo)nav.atras_conocido_mm=0;}
        caso("ambos lados bloqueados: no AYUDA antes de cuatro segundos",nav.estado==NAV_DETENIDO_OBSTACULO);
        /* V2: la coincidencia de las dos fuentes cuenta desde el segundo cuadro seguido, asi que el carro se detiene un cuadro (50 ms) mas tarde y los cuatro
         * segundos de espera se cumplen en 6050. */
        navegador_tick(&nav,6000,&cuadro);
        navegador_tick(&nav,6050,&cuadro);
        caso("ultimo recurso: retroceso conocido antes de ayuda",modo ? nav.cmd_actual==NAV_CMD_REVERSE : nav.estado==NAV_AYUDA && nav.error_causa==NAV_CAUSA_LADOS);
        for(t=6100;t<12000;t+=50)navegador_tick(&nav,t,&cuadro);
        caso("pared persistente sin espacio de retroceso termina quieto en AYUDA",nav.estado==NAV_AYUDA && nav.cmd_actual==NAV_CMD_STOP);
    }
    /* V2: salud del detector de objetos. La ruta cuenta con el y deja de dar resultados vigentes: a los NAV_MODELO_CAIDO_MS el carro se detiene con la causa 17. */
    preparar_politica();memset(&cuadro,0,sizeof cuadro);
    for(t=0;t<=1000;t+=50){navegador_aporte_modelo(&nav,1,1,t);navegador_tick(&nav,t,&cuadro);}
    caso("V2 detector vigente: la ruta avanza",nav.estado==NAV_SIGUIENDO && nav.cmd_actual==NAV_CMD_FORWARD);
    for(t=1050;t<=1000+NAV_MODELO_CAIDO_MS;t+=50){navegador_aporte_modelo(&nav,1,0,t);navegador_tick(&nav,t,&cuadro);}
    caso("V2 detector caido menos del plazo: todavia avanza",nav.estado==NAV_SIGUIENDO);
    for(;t<=1200+NAV_MODELO_CAIDO_MS;t+=50){navegador_aporte_modelo(&nav,1,0,t);navegador_tick(&nav,t,&cuadro);}
    caso("V2 detector caido mas del plazo: STOP y AYUDA con la causa 17",nav.estado==NAV_AYUDA && nav.cmd_actual==NAV_CMD_STOP && nav.error_causa==NAV_CAUSA_SIN_PERCEPCION);
    {
        char frase[200];
        caso("V2 la causa 17 tiene su frase",navegador_texto_error(&nav,frase,(int)sizeof frase)>0 && strstr(frase,"detector")!=NULL);
    }
    preparar_politica();memset(&cuadro,0,sizeof cuadro);
    for(t=0;t<=6000;t+=50){navegador_aporte_modelo(&nav,0,0,t);navegador_tick(&nav,t,&cuadro);}
    caso("V2 si la ruta NO cuenta con el detector (interruptor apagado) no se detiene por eso",nav.estado!=NAV_AYUDA && nav.error_causa!=NAV_CAUSA_SIN_PERCEPCION);
    preparar_politica();memset(&cuadro,0,sizeof cuadro);
    for(t=0;t<=6000;t+=50)navegador_tick(&nav,t,&cuadro);
    caso("V2 si la app nunca informa del detector, nada cambia",nav.estado!=NAV_AYUDA);
    /* Con la red caida, los cuadros no cuentan como «sin coincidencia» para cerrar un rodeo. */
    preparar_politica();memset(&cuadro,0,sizeof cuadro);
    navegador_aporte_modelo(&nav,1,1,0);navegador_tick(&nav,0,&cuadro);navegador_tick(&nav,50,&cuadro);
    z=nav.vision_sin_coincidencia;
    navegador_aporte_modelo(&nav,1,0,100);navegador_tick(&nav,100,&cuadro);navegador_tick(&nav,150,&cuadro);
    caso("V2 con el detector caido los cuadros no suman a «sin coincidencia»",z>=2 && nav.vision_sin_coincidencia==z);
    /* V2: la linea de la caja negra. */
    {
        char linea[NAV_CAJA_NEGRA_MAX];
        int largo;
        preparar_politica();memset(&cuadro,0,sizeof cuadro);
        navegador_aporte_sens(&nav,345,1,1,0,50);navegador_tick(&nav,50,&cuadro);navegador_tick(&nav,100,&cuadro);
        largo=navegador_caja_negra(&nav,100,linea,(int)sizeof linea);
        caso("V2 caja negra: una linea CN con la hora, el estado y el laser, sin saltos y dentro de la capacidad",largo>40 && largo<NAV_CAJA_NEGRA_MAX &&
             strncmp(linea,"CN t=100 e=",11)==0 && strstr(linea," tof=345,1,")!=NULL && strchr(linea,'\n')==NULL);
        caso("V2 caja negra: con poca capacidad corta y termina en cero",navegador_caja_negra(&nav,100,linea,20)==19 && strlen(linea)==19);
        caso("V2 caja negra: argumentos nulos devuelven 0",navegador_caja_negra(NULL,0,linea,100)==0 && navegador_caja_negra(&nav,0,NULL,100)==0);
    }
}

#endif

int main(int argc,char **argv) {
    int i,informe=argc>1 && strcmp(argv[1],"--informe")==0;
    if(argc>1 && !informe){i=atoi(argv[1]);if(i<0||i>=11)return 2;correr(i,0);return 0;}
    for(i=0;i<11;i++)correr(i,!informe);
#ifndef REPRO_BASE
    if(!informe)probar_politica();
#endif
    if(!informe)printf("%d casos, %d fallos\n",casos,fallos);
    return fallos ? EXIT_FAILURE : EXIT_SUCCESS;
}

/*
 * secretos.ejemplo.h — Valores de EJEMPLO para el WiFi del carro (la red que crea la DevKit; la camara se conecta a ella).
 *
 * Para usar tus propios valores: copia este archivo como secretos.h (en esta
 * misma carpeta), cambia los textos y compila. secretos.h está en .gitignore,
 * así que no se sube a GitHub. Si secretos.h no existe, el programa usa este.
 *
 * La contraseña de una red WPA2 debe tener entre 8 y 63 caracteres.
 */
#ifndef SECRETOS_EJEMPLO_H
#define SECRETOS_EJEMPLO_H

#define WIFI_SSID      "CarroFSI"
#define WIFI_PASSWORD  "cambia_esta_clave"

#endif /* SECRETOS_EJEMPLO_H */

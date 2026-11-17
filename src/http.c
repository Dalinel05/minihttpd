#include "http.h"
#include <string.h>
#include <strings.h>
// 1. Buscar el final de una línea HTTP: \r\n
static const char *buscar_crlf(const char *inicio,const char *fin) {
    for (const char *p = inicio; fin - p >= 2; p++) {
        if (p[0] == '\r' && p[1] == '\n') {return p;}
    }
    return NULL;
}
// 2. Copiar un campo sin desbordar su destino
static int copiar_campo(char *destino,size_t capacidad,const char *inicio,size_t cantidad) {

    if (cantidad >= capacidad) {return 0;}
    // Rechazar caracteres de control no permitidos
    for (size_t i = 0; i < cantidad; i++) {
        unsigned char c = (unsigned char)inicio[i];
        if ((c < 32 && c != '\t') || c == 127) {
            return 0;
        }
    }
    memcpy(destino, inicio, cantidad);
    destino[cantidad] = '\0';
    return 1;
}
// 3. Comprobar que un campo no contenga espacios
static int sin_espacios(const char *texto) {
    for (; *texto; texto++) {
        if (*texto == ' ' || *texto == '\t') {return 0;}
    }

    return 1;
}
// 4. Analizar una solicitud HTTP
int http_analizar_solicitud(const char *datos,size_t longitud,HttpSolicitud *solicitud) {
    // 5. Validar los argumentos recibidos
    if (datos == NULL ||solicitud == NULL ||longitud == 0) {return HTTP_BAD_REQUEST;}
    // 6. Inicializar la estructura de salida
    memset(solicitud, 0, sizeof(*solicitud));
    const char *fin = datos + longitud;
    // 7. Buscar el final de la primera línea
    const char *fin_linea = buscar_crlf(datos, fin);
    if (fin_linea == NULL) {return HTTP_BAD_REQUEST;}
    // 8. Buscar los espacios que separan los campos
    const char *espacio1 = memchr(datos,' ',(size_t)(fin_linea - datos));
    if (espacio1 == NULL) {return HTTP_BAD_REQUEST;}

    const char *espacio2 = memchr(espacio1 + 1,' ',(size_t)(fin_linea - espacio1 - 1));

    if (espacio2 == NULL ||memchr(espacio2 + 1, ' ',(size_t)(fin_linea - espacio2 - 1)) != NULL) {
        return HTTP_BAD_REQUEST;
    }
    // 9. Extraer método, ruta y versión
    if (!copiar_campo(solicitud->metodo,sizeof(solicitud->metodo),datos,(size_t)(espacio1 - datos)) ||
        !copiar_campo(solicitud->ruta,sizeof(solicitud->ruta),espacio1 + 1,(size_t)(espacio2 - espacio1 - 1)) ||
        !copiar_campo(solicitud->version,sizeof(solicitud->version),espacio2 + 1,(size_t)(fin_linea - espacio2 - 1))) {
        return HTTP_BAD_REQUEST;
    }
    // 10. Validar la primera línea HTTP
    if (solicitud->metodo[0] == '\0' ||solicitud->ruta[0] != '/' ||!sin_espacios(solicitud->metodo) ||!sin_espacios(solicitud->ruta) ||strcmp(solicitud->version, "HTTP/1.1") != 0) {
        return HTTP_BAD_REQUEST;
    }
    // 11. Recorrer los encabezados
    const char *cursor = fin_linea + 2;
    int host_encontrado = 0;
    while (cursor < fin) {
        const char *fin_cabecera = buscar_crlf(cursor,fin);
        if (fin_cabecera == NULL) {
            return HTTP_BAD_REQUEST;
        }
        // 12. Línea vacía: termina la sección de encabezados
        if (fin_cabecera == cursor) {
            if (!host_encontrado) {
                return HTTP_BAD_REQUEST;
            }
            // Solo implementamos GET
            if (strcmp(solicitud->metodo, "GET") == 0) {
                return HTTP_OK;
            }
            return HTTP_METHOD_NOT_ALLOWED;
        }
        // 13. Separar nombre y valor del encabezado
        const char *dos_puntos = memchr(cursor,':',(size_t)(fin_cabecera - cursor));
        if (dos_puntos == NULL ||dos_puntos == cursor) {
            return HTTP_BAD_REQUEST;
        }
        // 14. Validar el nombre del encabezado
        for (const char *p = cursor;p < dos_puntos;p++) {
            unsigned char c = (unsigned char)*p;
            if (c <= 32 || c >= 127) {
                return HTTP_BAD_REQUEST;
            }
        }
        // 15. Quitar espacios alrededor del valor
        const char *valor = dos_puntos + 1;
        while (valor < fin_cabecera &&(*valor == ' ' || *valor == '\t')) {
            valor++;
        }
        const char *fin_valor = fin_cabecera;
        while (fin_valor > valor &&(fin_valor[-1] == ' ' ||fin_valor[-1] == '\t')) {
            fin_valor--;
        }
        size_t largo_valor =(size_t)(fin_valor - valor);
        // 16. Validar los caracteres del valor
        for (const char *p = valor;p < fin_valor;p++) {
            unsigned char c = (unsigned char)*p;
            if ((c < 32 && c != '\t') || c == 127) {
                return HTTP_BAD_REQUEST;
            }
        }
        size_t largo_nombre = (size_t)(dos_puntos - cursor);
        // 17. Procesar el encabezado Host
        if (largo_nombre == 4 &&
            strncasecmp(cursor, "Host", 4) == 0) {
            if (host_encontrado ||largo_valor == 0 ||!copiar_campo(solicitud->host,sizeof(solicitud->host),valor,largo_valor) ||
                !sin_espacios(solicitud->host)) {
                return HTTP_BAD_REQUEST;
            }
            host_encontrado = 1;
        // 18. Procesar el encabezado Connection
        } else if (largo_nombre == 10 &&strncasecmp(cursor, "Connection", 10) == 0) {
            if (!copiar_campo(solicitud->connection,sizeof(solicitud->connection),valor,largo_valor)) {
                return HTTP_BAD_REQUEST;
            }
        // 19. Procesar el encabezado User-Agent
        } else if (largo_nombre == 10 &&strncasecmp(cursor, "User-Agent", 10) == 0) {
            if (!copiar_campo(solicitud->user_agent,sizeof(solicitud->user_agent),valor,largo_valor)) {
                return HTTP_BAD_REQUEST;
            }
        }
        cursor = fin_cabecera + 2; //Avanzar a la siguiente línea
    }
    return HTTP_BAD_REQUEST; // Solicitud incompleta o mal formada
}
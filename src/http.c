#include "http.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>

//Buscar el final de una línea HTTP: \r\n
static const char *buscar_crlf(const char *inicio,const char *fin) {

    for (const char *p = inicio; fin - p >= 2; p++) {
        if (p[0] == '\r' && p[1] == '\n') {return p;}
    }
    return NULL;

}


static int copiar_campo(char *destino,size_t capacidad,const char *inicio,size_t cantidad) {

    if (cantidad >= capacidad) {return 0;}
    
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

static int sin_espacios(const char *texto) {
    for (; *texto; texto++) {
        if (*texto == ' ' || *texto == '\t') {return 0;}
    }

    return 1;
}

// Analizar una solicitud HTTP --------------------

int http_analizar_solicitud(const char *datos,size_t longitud,HttpSolicitud *solicitud) {
    
    if (datos == NULL ||solicitud == NULL ||longitud == 0) {return HTTP_BAD_REQUEST;}
    
    memset(solicitud, 0, sizeof(*solicitud));
    const char *fin = datos + longitud;
    
    const char *fin_linea = buscar_crlf(datos, fin);
    if (fin_linea == NULL) {return HTTP_BAD_REQUEST;}
    
    const char *espacio1 = memchr(datos,' ',(size_t)(fin_linea - datos));
    if (espacio1 == NULL) {return HTTP_BAD_REQUEST;}

    const char *espacio2 = memchr(espacio1 + 1,' ',(size_t)(fin_linea - espacio1 - 1));

    if (espacio2 == NULL ||memchr(espacio2 + 1, ' ',(size_t)(fin_linea - espacio2 - 1)) != NULL) {
        return HTTP_BAD_REQUEST;
    }
    
    if (!copiar_campo(solicitud->metodo,sizeof(solicitud->metodo),datos,(size_t)(espacio1 - datos)) ||
        !copiar_campo(solicitud->ruta,sizeof(solicitud->ruta),espacio1 + 1,(size_t)(espacio2 - espacio1 - 1)) ||
        !copiar_campo(solicitud->version,sizeof(solicitud->version),espacio2 + 1,(size_t)(fin_linea - espacio2 - 1))) {
        return HTTP_BAD_REQUEST;
    }
    
    if (solicitud->metodo[0] == '\0' ||solicitud->ruta[0] != '/' ||!sin_espacios(solicitud->metodo) ||!sin_espacios(solicitud->ruta) ||strcmp(solicitud->version, "HTTP/1.1") != 0) {
        return HTTP_BAD_REQUEST;
    }
    
    const char *cursor = fin_linea + 2;
    int host_encontrado = 0;
    while (cursor < fin) {
        const char *fin_cabecera = buscar_crlf(cursor,fin);
        if (fin_cabecera == NULL) {
            return HTTP_BAD_REQUEST;
        }
        
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
        
        const char *dos_puntos = memchr(cursor,':',(size_t)(fin_cabecera - cursor));
        if (dos_puntos == NULL ||dos_puntos == cursor) {
            return HTTP_BAD_REQUEST;
        }
        
        for (const char *p = cursor;p < dos_puntos;p++) {
            unsigned char c = (unsigned char)*p;
            if (c <= 32 || c >= 127) {
                return HTTP_BAD_REQUEST;
            }
        }
        
        const char *valor = dos_puntos + 1;
        while (valor < fin_cabecera &&(*valor == ' ' || *valor == '\t')) {
            valor++;
        }
        const char *fin_valor = fin_cabecera;
        while (fin_valor > valor &&(fin_valor[-1] == ' ' ||fin_valor[-1] == '\t')) {
            fin_valor--;
        }
        size_t largo_valor =(size_t)(fin_valor - valor);
        
        for (const char *p = valor;p < fin_valor;p++) {
            unsigned char c = (unsigned char)*p;
            if ((c < 32 && c != '\t') || c == 127) {
                return HTTP_BAD_REQUEST;
            }
        }
        size_t largo_nombre = (size_t)(dos_puntos - cursor);
        
        if (largo_nombre == 4 &&
            strncasecmp(cursor, "Host", 4) == 0) {
            if (host_encontrado ||largo_valor == 0 ||!copiar_campo(solicitud->host,sizeof(solicitud->host),valor,largo_valor) ||
                !sin_espacios(solicitud->host)) {
                return HTTP_BAD_REQUEST;
            }
            host_encontrado = 1;
        
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
// Construir una respuesta HTTP
size_t http_construir_respuesta(char *destino, size_t capacidad, int codigo, off_t tamano_contenido, const char *tipo_mime) {
    if (destino == NULL || capacidad == 0) {
        return 0;
    }
    const char *mensaje;
    switch (codigo) {
        case HTTP_OK:
            mensaje = "OK";
            break;
        case HTTP_BAD_REQUEST:
            mensaje = "Bad Request";
            break;
        case HTTP_FORBIDDEN:
            mensaje = "Forbidden";
            break;
        case HTTP_NOT_FOUND:
            mensaje = "Not Found";
            break;
        case HTTP_METHOD_NOT_ALLOWED:
            mensaje = "Method Not Allowed";
            break;
        case HTTP_INTERNAL_ERROR:
            mensaje = "Internal Server Error";
            break;
        default:
            codigo = HTTP_INTERNAL_ERROR;
            mensaje = "Internal Server Error";
            break;
    }
    const char *allow = (codigo == HTTP_METHOD_NOT_ALLOWED) ? "Allow: GET\r\n" : "";
    const char *tipo = (codigo == HTTP_OK && tipo_mime != NULL) ? tipo_mime : "text/plain; charset=utf-8";
    int escritos = snprintf(
        destino,
        capacidad,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lld\r\n"
        "Connection: close\r\n"
        "%s"
        "\r\n",
        codigo,
        mensaje,
        tipo,
        (long long)tamano_contenido,
        allow
    );

    if (escritos < 0 || (size_t)escritos >= capacidad) {
        return 0;
    }

    return (size_t)escritos;
}


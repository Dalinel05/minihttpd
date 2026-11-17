
#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

// Limites de los campos de la solicitud
#define HTTP_MAX_METODO 16
#define HTTP_MAX_RUTA 2048
#define HTTP_MAX_VERSION 16
#define HTTP_MAX_HOST 256
#define HTTP_MAX_CONNECTION 64
#define HTTP_MAX_USER_AGENT 512

// Codigos de estado HTTP requeridos
#define HTTP_OK 200
#define HTTP_BAD_REQUEST 400
#define HTTP_FORBIDDEN 403
#define HTTP_NOT_FOUND 404
#define HTTP_METHOD_NOT_ALLOWED 405
#define HTTP_INTERNAL_ERROR 500

// Representacion de una solicitud HTTP
typedef struct {

    char metodo[HTTP_MAX_METODO];
    char ruta[HTTP_MAX_RUTA];
    char version[HTTP_MAX_VERSION];

    char host[HTTP_MAX_HOST];
    char connection[HTTP_MAX_CONNECTION];
    char user_agent[HTTP_MAX_USER_AGENT];

} HttpSolicitud;

// Analizar los bytes recibidos de una solicitud HTTP
// Devuelve 200 si es valida o un codigo de error HTTP
int http_analizar_solicitud(
    const char *datos,
    size_t longitud,
    HttpSolicitud *solicitud
);

#endif

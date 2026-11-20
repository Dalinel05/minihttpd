
#include "mime.h"
#include <string.h>
#include <strings.h>

const char *mime_tipo(const char *ruta) {

    if (ruta == NULL) {
        return "application/octet-stream";
    }

    char archivo[2048];

    size_t longitud = strcspn(ruta, "?#");

    if (longitud == 0 || longitud >= sizeof(archivo)) {
        return "application/octet-stream";
    }

    memcpy(archivo, ruta, longitud);
    archivo[longitud] = '\0';

    if (strcmp(archivo, "/") == 0) {
        return "text/html";
    }

    const char *extension = strrchr(archivo, '.');
    const char *ultima_barra = strrchr(archivo, '/');

    if (extension == NULL ||
        (ultima_barra != NULL && extension < ultima_barra)) {
        return "application/octet-stream";
    }

    if (strcasecmp(extension, ".html") == 0 ||
        strcasecmp(extension, ".htm") == 0) {
        return "text/html";
    }

    if (strcasecmp(extension, ".css") == 0) {
        return "text/css";
    }

    if (strcasecmp(extension, ".js") == 0) {
        return "application/javascript";
    }

    if (strcasecmp(extension, ".png") == 0) {
        return "image/png";
    }

    if (strcasecmp(extension, ".jpg") == 0 ||
        strcasecmp(extension, ".jpeg") == 0) {
        return "image/jpeg";
    }

    return "application/octet-stream";
}

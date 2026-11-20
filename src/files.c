
#include "files.h"
#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

int files_abrir(const char *ruta, int *fd_archivo, off_t *tamano) {
    if (ruta == NULL || fd_archivo == NULL || tamano == NULL || ruta[0] != '/') {
        return HTTP_BAD_REQUEST;
    }
    *fd_archivo = -1;
    *tamano = 0;

    char raiz[PATH_MAX];
    char ruta_completa[PATH_MAX];
    char ruta_real[PATH_MAX];

    if (realpath("www", raiz) == NULL) {return HTTP_INTERNAL_ERROR;}

    size_t longitud = strcspn(ruta, "?#");
    const char *recurso = ruta;

    if (longitud == 1 && ruta[0] == '/') {
        recurso = "/index.html";
        longitud = strlen(recurso);
    }

    int escritos = snprintf(ruta_completa,sizeof(ruta_completa),"%s%.*s",raiz,(int)longitud,recurso);

    if (escritos < 0 || (size_t)escritos >= sizeof(ruta_completa)) {
        return HTTP_BAD_REQUEST;
    }

    if (realpath(ruta_completa, ruta_real) == NULL) {
        if (errno == ENOENT || errno == ENOTDIR) {
            return HTTP_NOT_FOUND;
        }
        if (errno == EACCES || errno == ELOOP) {
            return HTTP_FORBIDDEN;
        }
        return HTTP_INTERNAL_ERROR;
    }
    size_t largo_raiz = strlen(raiz);
    if (strncmp(ruta_real, raiz, largo_raiz) != 0 ||ruta_real[largo_raiz] != '/') {
        return HTTP_FORBIDDEN;
    }

    int fd = open(ruta_real, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd == -1) {
        if (errno == EACCES || errno == EPERM) {
            return HTTP_FORBIDDEN;
        }
        if (errno == ENOENT || errno == ENOTDIR) {
            return HTTP_NOT_FOUND;
        }
        return HTTP_INTERNAL_ERROR;
    }
    struct stat informacion;

    if (fstat(fd, &informacion) == -1) {
        close(fd);
        return HTTP_INTERNAL_ERROR;
    }

    if (!S_ISREG(informacion.st_mode)) {
        close(fd);
        return HTTP_FORBIDDEN;
    }
    *fd_archivo = fd;
    *tamano = informacion.st_size;
    return HTTP_OK;
}

#ifndef FILES_H
#define FILES_H
#include <sys/types.h>

int files_abrir(const char *ruta, int *fd_archivo, off_t *tamano);

#endif

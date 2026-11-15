
#include "server.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>

int main(void) {

    // 1. Crear el socket de escucha
    int sd = crear_socket();

    // 2. Comprobar si el socket es válido
    if (sd == -1) {
        return 1;
    }

    // 3. Crear una instancia de epoll
    int epfd = epoll_create1(0);

    // 4. Comprobar si epoll se creó correctamente
    if (epfd == -1) {
        perror("epoll_create1");
        close(sd);
        return 1;
    }

    // 5 y 6. Declarar e inicializar la estructura
    struct epoll_event ev = {0};

    // 7. Detectar disponibilidad de lectura
    ev.events = EPOLLIN;

    // 8. Asociar el descriptor del socket
    ev.data.fd = sd;

    // 9. Registrar el socket en epoll
    int resultado = epoll_ctl(epfd, EPOLL_CTL_ADD, sd, &ev);

    // 10. Comprobar si ocurrió un error
    if (resultado == -1) {
        perror("epoll_ctl");
        close(epfd);
        close(sd);
        return 1;
    }

    // 11. Consultar las opciones actuales del socket
    int flags = fcntl(sd, F_GETFL);

    if (flags == -1) {
        perror("fcntl F_GETFL");
        close(epfd);
        close(sd);
        return 1;
    }

    // 12 y 13. Agregar y aplicar el modo no bloqueante
    if (fcntl(sd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
        close(epfd);
        close(sd);
        return 1;
    }

    // Aquí continuaremos con el bucle de eventos

    close(epfd);
    close(sd);

    return 0;
}

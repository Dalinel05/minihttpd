
#include "server.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <errno.h>

#define MAX_EVENTOS 20

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

    // 14. Crear un arreglo para almacenar eventos
    struct epoll_event eventos[MAX_EVENTOS] = {0};

    // 15. Informar que el servidor está esperando eventos
    printf("MiniHTTPd: esperando conexiones en el puerto 8080...\n");


    // 16. Iniciar el bucle principal del servidor
    while (1) {

        // 17. Esperar eventos
        int num_eventos = epoll_wait(epfd, eventos, MAX_EVENTOS, -1);

        // 18. Comprobar errores
        if (num_eventos == -1) {

            // Si fue interrumpido por una señal, reintentar
            if (errno == EINTR) {
                continue;
            }

            perror("epoll_wait");
            close(epfd);
            close(sd);
            return 1;
        }

        // 19. Recorrer los eventos recibidos
        for (int i = 0; i < num_eventos; i++) {

            // 20. Comprobar si el evento pertenece
            //     al socket de escucha
            if (eventos[i].data.fd == sd) {

                // 21. Aceptar todas las conexiones
                //     pendientes de este evento
                while (1) {

                    int cliente = accept(sd, NULL, NULL);

                    // 22. Comprobar errores de accept
                    if (cliente == -1) {

                        // Ya no quedan conexiones pendientes
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }

                        // La llamada fue interrumpida
                        if (errno == EINTR) {
                            continue;
                        }

                        perror("accept");
                        close(epfd);
                        close(sd);
                        return 1;
                    }

                    // 23. Mostrar el descriptor del cliente
                    printf("Cliente aceptado: descriptor %d\n", cliente);

                    // 24. Por ahora cerrar la conexión
                    close(cliente);
                }
            }
        }
    }

}

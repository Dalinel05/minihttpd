
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_EVENTOS 20
#define MAX_CABECERAS 8192

// Estructura para almacenar el estado de cada cliente
typedef struct {
    int fd;
    size_t usados;
    char buffer[MAX_CABECERAS];
} EstadoCliente;

int main(void) {

    // 1. Crear el socket de escucha
    int sd = crear_socket();

    if (sd == -1) {
        return 1;
    }

    // 2. Crear una instancia de epoll
    int epfd = epoll_create1(0);

    if (epfd == -1) {
        perror("epoll_create1");
        close(sd);
        return 1;
    }

    // 3. Configurar el socket como no bloqueante
    int flags = fcntl(sd, F_GETFL, 0);

    if (flags == -1 ||
        fcntl(sd, F_SETFL, flags | O_NONBLOCK) == -1) {

        perror("fcntl socket de escucha");
        close(epfd);
        close(sd);
        return 1;
    }

    // 4. Registrar el socket de escucha en epoll
    struct epoll_event ev = {0};

    ev.events = EPOLLIN;
    ev.data.ptr = NULL;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sd, &ev) == -1) {
        perror("epoll_ctl escucha");
        close(epfd);
        close(sd);
        return 1;
    }

    // 5. Preparar el arreglo de eventos
    struct epoll_event eventos[MAX_EVENTOS] = {0};

    printf("MiniHTTPd: esperando conexiones en el puerto 8080...\n");

    // 6. Iniciar el bucle principal
    while (1) {

        int num_eventos = epoll_wait(
            epfd,
            eventos,
            MAX_EVENTOS,
            -1
        );

        // 7. Comprobar errores de epoll_wait
        if (num_eventos == -1) {

            if (errno == EINTR) {
                continue;
            }

            perror("epoll_wait");
            close(epfd);
            close(sd);
            return 1;
        }

        // 8. Recorrer los eventos recibidos
        for (int i = 0; i < num_eventos; i++) {

            // 9. Identificar el socket de escucha
            if (eventos[i].data.ptr == NULL) {

                // 10. Aceptar las conexiones pendientes
                while (1) {

                    int cliente = accept(sd, NULL, NULL);

                    // 11. Comprobar errores de accept
                    if (cliente == -1) {

                        if (errno == EAGAIN ||
                            errno == EWOULDBLOCK) {
                            break;
                        }

                        if (errno == EINTR) {
                            continue;
                        }

                        perror("accept");
                        break;
                    }

                    printf(
                        "Cliente aceptado: descriptor %d\n",
                        cliente
                    );

                    // 12. Configurar el cliente como no bloqueante
                    int flags_cliente = fcntl(
                        cliente,
                        F_GETFL,
                        0
                    );

                    if (flags_cliente == -1 ||
                        fcntl(cliente, F_SETFL,
                              flags_cliente | O_NONBLOCK) == -1) {

                        perror("fcntl cliente");
                        close(cliente);
                        continue;
                    }

                    // 13. Reservar memoria para el cliente
                    EstadoCliente *estado = calloc(
                        1,
                        sizeof(EstadoCliente)
                    );

                    if (estado == NULL) {
                        perror("calloc cliente");
                        close(cliente);
                        continue;
                    }

                    // 14. Guardar el descriptor del cliente
                    estado->fd = cliente;

                    // 15. Preparar el evento del cliente
                    struct epoll_event ev_cliente = {0};

                    ev_cliente.events = EPOLLIN;
                    ev_cliente.data.ptr = estado;

                    // 16. Registrar el cliente en epoll
                    if (epoll_ctl(
                            epfd,
                            EPOLL_CTL_ADD,
                            cliente,
                            &ev_cliente
                        ) == -1) {

                        perror("epoll_ctl cliente");
                        close(cliente);
                        free(estado);
                        continue;
                    }

                    printf(
                        "Cliente registrado en epoll: %d\n",
                        cliente
                    );
                }

            } else {

                // 17. Recuperar el estado del cliente
                EstadoCliente *estado = eventos[i].data.ptr;

                int cliente_fd = estado->fd;
                int cerrar_cliente = 0;

                // 18. Leer los datos disponibles
                while (!cerrar_cliente) {

                    // 19. Calcular el espacio libre
                    size_t disponibles =
                        MAX_CABECERAS - estado->usados;

                    // 20. Comprobar el límite del buffer
                    if (disponibles == 0) {

                        printf(
                            "Cliente %d: solicitud demasiado grande\n",
                            cliente_fd
                        );

                        cerrar_cliente = 1;
                        break;
                    }

                    // 21. Leer nuevos bytes del cliente
                    ssize_t bytes = recv(
                        cliente_fd,
                        estado->buffer + estado->usados,
                        disponibles,
                        0
                    );

                    if (bytes > 0) {

                        // 22. Actualizar los bytes acumulados
                        estado->usados += (size_t)bytes;

                        printf(
                            "Cliente %d: %zu bytes acumulados\n",
                            cliente_fd,
                            estado->usados
                        );

                        // 23. Buscar el final de los encabezados
                        int completa = 0;

                        for (size_t j = 0;
                             j + 3 < estado->usados;
                             j++) {

                            if (estado->buffer[j] == '\r' &&
                                estado->buffer[j + 1] == '\n' &&
                                estado->buffer[j + 2] == '\r' &&
                                estado->buffer[j + 3] == '\n') {

                                completa = 1;
                                break;
                            }
                        }

                        // 24. Comprobar si los encabezados están completos
                        if (completa) {

                            printf(
                                "Solicitud HTTP completa del cliente %d:\n",
                                cliente_fd
                            );

                            // Mostrar los bytes acumulados
                            fwrite(
                                estado->buffer,
                                1,
                                estado->usados,
                                stdout
                            );

                            putchar('\n');

                            // Temporal: cerrar hasta integrar http.c
                            cerrar_cliente = 1;
                            break;
                        }

                        // Continuar leyendo si hay datos disponibles
                        continue;

                    } else if (bytes == 0) {

                        // 25. El cliente cerró su lado de envío
                        printf(
                            "Cliente %d desconectado\n",
                            cliente_fd
                        );

                        cerrar_cliente = 1;
                        break;

                    } else {

                        // 26. Comprobar si recv fue interrumpido
                        if (errno == EINTR) {
                            continue;
                        }

                        // 27. No hay más datos disponibles por ahora
                        if (errno == EAGAIN ||
                            errno == EWOULDBLOCK) {
                            break;
                        }

                        // 28. Otro error de recepción
                        perror("recv");

                        cerrar_cliente = 1;
                        break;
                    }
                }

                // 29. Liberar recursos cuando corresponde
                if (cerrar_cliente) {

                    // Retirar el cliente de epoll
                    if (epoll_ctl(
                            epfd,
                            EPOLL_CTL_DEL,
                            cliente_fd,
                            NULL
                        ) == -1) {

                        perror("epoll_ctl DEL cliente");
                    }

                    // Cerrar el socket del cliente
                    close(cliente_fd);

                    // Liberar la memoria del cliente
                    free(estado);
                }
            }
        }
    }
}

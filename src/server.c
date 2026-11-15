#include "server.h"
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

int crear_socket(void) {

    struct addrinfo hints = {0};
    struct addrinfo *res = NULL;

    int s, sd;

    // 1. Configurar las características del socket
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    // 2. Obtener las direcciones para el puerto 8080
    s = getaddrinfo(NULL, "8080", &hints, &res);

    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return -1;
    }

    // 3. Crear el socket TCP
    sd = socket(res->ai_family,
                res->ai_socktype,
                res->ai_protocol);

    if (sd == -1) {
        perror("socket");
        freeaddrinfo(res);
        return -1;
    }

    // 4. Asociar el socket con la dirección y el puerto
    if (bind(sd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("bind");
        close(sd);
        freeaddrinfo(res);
        return -1;
    }

    // 5. Preparar el socket para escuchar conexiones
    if (listen(sd, 20) == -1) {
        perror("listen");
        close(sd);
        freeaddrinfo(res);
        return -1;
    }

    // 6. Liberar la memoria de getaddrinfo
    freeaddrinfo(res);

    // 7. Devolver el descriptor del socket de escucha
    return sd;
}

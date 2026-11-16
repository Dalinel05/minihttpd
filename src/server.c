#include "server.h"
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

int crear_socket(void) {

    struct addrinfo hints = {0};
    struct addrinfo *lista_direcciones = NULL;
    int resultado, socket_escucha;

    // Configurar las características del socket
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    // Obtener las direcciones para el puerto 8080
    resultado = getaddrinfo(NULL, "8080", &hints, &lista_direcciones);
    if (resultado != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(resultado));
        return -1;
    }
    // Crear el socket TCP
    if ( socket(lista_direcciones->ai_family, lista_direcciones->ai_socktype, lista_direcciones ->ai_protocol)== -1) {
        perror("socket");
        freeaddrinfo(lista_direcciones);
        return -1;
    }
    // Asociar el socket con la dirección y el puerto
    if (bind(socket_escucha, lista_direcciones->ai_addr, lista_direcciones->ai_addrlen) == -1) {
        perror("bind");
        close(socket_escucha);
        freeaddrinfo(lista_direcciones);
        return -1;
    }
    // Preparar el socket para escuchar conexiones
    if (listen(socket_escucha, 20) == -1) {
        perror("listen");
        close(socket_escucha);
        freeaddrinfo(lista_direcciones);
        return -1;
    }
    freeaddrinfo(lista_direcciones); // Liberar la memoria de getaddrinfo
    return socket_escucha; //Devolver el descriptor del socket de escucha
}

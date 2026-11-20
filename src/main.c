#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include "http.h"
#include "files.h"

#define MAX_EVENTOS 20
#define MAX_CABECERAS 8192


typedef struct {int fd;size_t usados;size_t enviados;int respondiendo;char buffer[MAX_CABECERAS];} EstadoCliente;
// Estructura para almacenar el estado de cada cliente
int main(void) {
    
    int socket_escucha = crear_socket();// Crear el socket de escucha
    if (socket_escucha == -1) {return 1;}//para controlar si fallo alguna operación
    

    int epfd = epoll_create1(0); // epfd = a epollfiledescriptor CO
    if (epfd == -1) {
        perror("epoll_create1");
        close(socket_escucha);
        return 1;
    }

    int flags = fcntl(socket_escucha, F_GETFL, 0);
    if (flags == -1 || fcntl(socket_escucha, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl socket de escucha");
        close(epfd);
        close(socket_escucha);
        return 1;
    }

    struct epoll_event ev = {0};
    ev.events = EPOLLIN;
    ev.data.ptr = NULL;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, socket_escucha, &ev) == -1) {

        perror("epoll_ctl escucha");
        close(epfd);
        close(socket_escucha);
        return 1;
        
    }
    // Preparar el arreglo de eventos
    struct epoll_event eventos[MAX_EVENTOS] = {0};

    printf("MiniHTTPd: esperando conexiones en el puerto 8080...\n");
    
    while (1) {
        int num_eventos = epoll_wait(epfd,eventos,MAX_EVENTOS,-1);
        
        if (num_eventos == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("epoll_wait");
            close(epfd);

            close(socket_escucha);
            return 1;
        }
        // Recorrer los eventos recibidos
        for (int i = 0; i < num_eventos; i++) {
            // dentificar el socket de escucha
            if (eventos[i].data.ptr == NULL) {
                // Aceptar las conexiones pendientes
                while (1) {
                    int cliente = accept(socket_escucha, NULL, NULL);
                    
                    if (cliente == -1) {
                        if (errno == EAGAIN ||errno == EWOULDBLOCK) {
                            break;
                        }
                        if (errno == EINTR) {
                            continue;
                        }
                        perror("accept");
                        break;
                    }
                    printf("Cliente aceptado: descriptor %d\n",cliente);
                    // Configurar el cliente como no bloqueante
                    int flags_cliente = fcntl(cliente,F_GETFL,0);
                    if (flags_cliente == -1 ||fcntl(cliente, F_SETFL,flags_cliente | O_NONBLOCK) == -1) {
                        perror("fcntl cliente");
                        close(cliente);
                        continue;
                    }
                    // Reservar memoria para el cliente
                    EstadoCliente *estado = calloc(1,sizeof(EstadoCliente));
                    if (estado == NULL) {
                        perror("calloc cliente");
                        close(cliente);
                        continue;
                    }
                    // Guardar el descriptor del cliente
                    estado->fd = cliente;
                    // Preparar el evento del cliente
                    struct epoll_event ev_cliente = {0};
                    ev_cliente.events = EPOLLIN;
                    ev_cliente.data.ptr = estado;
                    // Registrar el cliente en epoll
                    if (epoll_ctl(epfd,EPOLL_CTL_ADD,cliente,&ev_cliente) == -1) {
                        perror("epoll_ctl cliente");
                        close(cliente);
                        free(estado);
                        continue;
                    }
                    printf("Cliente registrado en epoll: %d\n",cliente);
                }
            } else {
                
                EstadoCliente *estado = eventos[i].data.ptr;
                int cliente_fd = estado->fd;
                int cerrar_cliente = 0;
                if (estado->respondiendo) {
                    while (estado->enviados < estado->usados) {
                        ssize_t n = send(cliente_fd,estado->buffer + estado->enviados,estado->usados - estado->enviados,MSG_NOSIGNAL);
                        if (n > 0) {
                            estado->enviados += (size_t)n;
                        } else if (n == -1 && errno == EINTR) {
                            continue;
                        } else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                            break;
                        } else {
                            if (n == -1) {
                                perror("send");
                            }
                            cerrar_cliente = 1;
                            break;
                        }
                    }
                    if (estado->enviados == estado->usados) {
                        cerrar_cliente = 1;
                    }
                } else {
                    while (!cerrar_cliente) {
                        
                        size_t disponibles =MAX_CABECERAS - estado->usados;
                        
                        if (disponibles == 0) {
                            printf("Cliente %d: solicitud demasiado grande\n",cliente_fd);
                            cerrar_cliente = 1;
                            break;
                        }
                        
                        ssize_t bytes = recv(cliente_fd,estado->buffer + estado->usados,disponibles,0);
                        if (bytes > 0) {
                            
                            estado->usados += (size_t)bytes;
                            printf("Cliente %d: %zu bytes acumulados\n",cliente_fd,estado->usados);
                            // Buscar el final de los encabezados
                            int completa = 0;
                            for (size_t j = 0; j + 3 < estado->usados;j++) {
                                if (estado->buffer[j] == '\r' &&estado->buffer[j + 1] == '\n' && estado->buffer[j + 2] == '\r' && estado->buffer[j + 3] == '\n') {
                                    completa = 1;
                                    break;
                                }
                            }
                            // Comprobar si los encabezados están completos
                            if (completa) {
                                printf("Solicitud HTTP completa del cliente %d:\n",cliente_fd);
                                fwrite(estado->buffer,1,estado->usados,stdout);
                                putchar('\n');

                                HttpSolicitud solicitud;
                                int codigo_http = http_analizar_solicitud(estado->buffer, estado->usados, &solicitud);
                                printf("Resultado del analisis HTTP: %d\n", codigo_http);

                                if (codigo_http == HTTP_OK) {
                                    int fd_archivo;
                                    off_t tamano_archivo;
                                    codigo_http = files_abrir(solicitud.ruta, &fd_archivo, &tamano_archivo);
                                    if (codigo_http == HTTP_OK) {
                                        printf("Archivo encontrado: %s\n", solicitud.ruta);
                                        printf("Tamano del archivo: %lld bytes\n", (long long)tamano_archivo);
                                        close(fd_archivo);
                                    }
                                }
                                printf("Codigo de respuesta HTTP: %d\n", codigo_http);

                                size_t longitud_respuesta = http_construir_respuesta(estado->buffer, sizeof(estado->buffer), codigo_http);
                                if (longitud_respuesta == 0) {
                                    cerrar_cliente = 1;
                                    break;
                                }
                                estado->usados = longitud_respuesta;
                                estado->enviados = 0;
                                estado->respondiendo = 1;
                                struct epoll_event ev_respuesta = {0};
                                ev_respuesta.events = EPOLLOUT;
                                ev_respuesta.data.ptr = estado;
                                if (epoll_ctl(epfd, EPOLL_CTL_MOD, cliente_fd, &ev_respuesta) == -1) {
                                    perror("epoll_ctl respuesta");
                                    cerrar_cliente = 1;
                                }
                                break;
                            }
                            continue;// Continuar leyendo si hay datos disponibles
                        } else if (bytes == 0) {
                            
                            printf("Cliente %d desconectado\n",cliente_fd);
                            cerrar_cliente = 1;
                            break;
                        } else {
                            //Comprobar si recv fue interrumpido
                            if (errno == EINTR) {
                                continue;
                            }
                            
                            if (errno == EAGAIN ||errno == EWOULDBLOCK) {
                                break;
                            }
                            
                            perror("recv");
                            cerrar_cliente = 1;
                            break;
                        }
                    }
                }
                // 29. Liberar recursos cuando corresponde
                if (cerrar_cliente) {
                    // Retirar el cliente de epoll
                    if (epoll_ctl(epfd,EPOLL_CTL_DEL,cliente_fd,NULL) == -1) {
                        perror("epoll_ctl DEL cliente");
                    }
                    close(cliente_fd); // Cerrar el socket del cliente
                    free(estado);// Liberar la memoria del cliente
                }   
            }
        }
    }
}

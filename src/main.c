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
#include "mime.h"
#include <sys/sendfile.h>
#include <strings.h>

#define MAX_EVENTOS 20
#define MAX_CABECERAS 8192


typedef struct {int fd;size_t usados;size_t enviados;int respondiendo;char buffer[MAX_CABECERAS];int fd_archivo;off_t tamano_archivo;off_t enviados_archivo;int mantener_conexion;} EstadoCliente;

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

    printf("MiniHTTPd: esperando conexiones en el puerto 8080...\n\n");
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
                    estado->fd_archivo = -1;
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

                    if (!cerrar_cliente && estado->enviados == estado->usados) {
                        while (estado->enviados_archivo < estado->tamano_archivo) {

                            off_t restantes = estado->tamano_archivo - estado->enviados_archivo;
                            size_t bloque = restantes > 65536? 65536 : (size_t)restantes;
                            ssize_t n = sendfile(cliente_fd,estado->fd_archivo,&estado->enviados_archivo,bloque);

                            if (n > 0) {
                                continue;
                            }
                            if (n == -1 && errno == EINTR) {
                                continue;
                            }
                            if (n == -1 &&
                                (errno == EAGAIN || errno == EWOULDBLOCK)) {
                                break;
                            }
                            if (n == -1) {
                                perror("sendfile");
                            } else {
                                fprintf(stderr, "Archivo incompleto\n");
                            }
                            cerrar_cliente = 1;
                            break;
                        }

                    if (!cerrar_cliente &&
                        estado->enviados_archivo == estado->tamano_archivo) {

                        if (estado->mantener_conexion) {

                            if (estado->fd_archivo != -1) {
                                close(estado->fd_archivo);
                                estado->fd_archivo = -1;
                            }
                            estado->usados = 0;
                            estado->enviados = 0;
                            estado->respondiendo = 0;
                            estado->tamano_archivo = 0;
                            estado->enviados_archivo = 0;
                            estado->mantener_conexion = 0;
                            struct epoll_event ev_lectura = {0};

                            ev_lectura.events = EPOLLIN;
                            ev_lectura.data.ptr = estado;
                            if (epoll_ctl(epfd, EPOLL_CTL_MOD,
                                        cliente_fd, &ev_lectura) == -1) {
                                perror("epoll_ctl keep-alive");
                                cerrar_cliente = 1;
                            }
                        } else {
                            cerrar_cliente = 1;
                        }
                    }

                    }

                } else {
                    while (!cerrar_cliente) {
                        
                        size_t disponibles =MAX_CABECERAS - estado->usados;
                        
                        if (disponibles == 0) {

                            char descarte[2048];
                            for (int i = 0; i < 8; i++) {ssize_t n = recv(cliente_fd, descarte, sizeof(descarte), 0);
                                if (n <= 0) break;
                            }
                            estado->mantener_conexion = 0;
                            estado->usados = http_construir_respuesta(estado->buffer,sizeof(estado->buffer),HTTP_BAD_REQUEST,0,"text/plain; charset=utf-8",0);
                            if (estado->usados == 0) {
                                cerrar_cliente = 1;
                                break;
                            }

                            estado->enviados = 0;
                            estado->respondiendo = 1;

                            struct epoll_event ev_error = {0};
                            ev_error.events = EPOLLOUT;
                            ev_error.data.ptr = estado;

                            if (epoll_ctl(epfd, EPOLL_CTL_MOD, cliente_fd, &ev_error) == -1) {
                                perror("epoll_ctl error HTTP");
                                cerrar_cliente = 1;
                            } else {
                                printf("[Cliente %d] Solicitud demasiado grande -> 400 Bad Request\n",
                                    cliente_fd);
                            }
                            break;
                        }                        
                        ssize_t bytes = recv(cliente_fd,estado->buffer + estado->usados,disponibles,0);
                        if (bytes > 0) {
                            
                            estado->usados += (size_t)bytes;
                            // Buscar el final de los encabezados
                            int completa = 0;
                            for (size_t j = 0; j + 3 < estado->usados;j++) {
                                if (estado->buffer[j] == '\r' &&estado->buffer[j + 1] == '\n' && estado->buffer[j + 2] == '\r' && estado->buffer[j + 3] == '\n') {
                                    completa = 1;
                                    break;
                                }
                            }
                            if (completa) {
                                char primera_linea[512];
                                size_t j = 0;
                                while (j < estado->usados &&j < sizeof(primera_linea) - 1 && estado->buffer[j] != '\r' && estado->buffer[j] != '\n') {
                                    primera_linea[j] = estado->buffer[j];
                                    j++;
                                }
                                primera_linea[j] = '\0';
                                char metodo[16] = "?";
                                char ruta_log[256] = "?";
                                sscanf(primera_linea, "%15s %255s", metodo, ruta_log);
                                HttpSolicitud solicitud = {0};
                                int codigo_http = http_analizar_solicitud(estado->buffer, estado->usados, &solicitud);
                                estado->mantener_conexion = 0;

                                if (codigo_http == HTTP_OK) {
                                    if (strcasecmp(solicitud.connection, "close") != 0) {
                                        estado->mantener_conexion = 1;
                                    }
                                }
                                const char *tipo_mime = "application/octet-stream";

                                if (codigo_http == HTTP_OK) {
                                    int fd_archivo;
                                    off_t tamano_archivo;
                                    codigo_http = files_abrir(solicitud.ruta, &fd_archivo, &tamano_archivo);
                                    if (codigo_http == HTTP_OK) {
                                        estado->fd_archivo = fd_archivo;
                                        estado->tamano_archivo = tamano_archivo;
                                        estado->enviados_archivo = 0;

                                        tipo_mime = mime_tipo(solicitud.ruta);
                                    }
                                }
                                
                                const char *mensaje_estado;
                                switch (codigo_http) {
                                    case HTTP_OK:
                                        mensaje_estado = "OK";
                                        break;
                                    case HTTP_BAD_REQUEST:
                                        mensaje_estado = "Bad Request";
                                        break;
                                    case HTTP_FORBIDDEN:
                                        mensaje_estado = "Forbidden";
                                        break;
                                    case HTTP_NOT_FOUND:
                                        mensaje_estado = "Not Found";
                                        break;
                                    case HTTP_METHOD_NOT_ALLOWED:
                                        mensaje_estado = "Method Not Allowed";
                                        break;
                                    case HTTP_INTERNAL_ERROR:
                                        mensaje_estado = "Internal Server Error";
                                        break;
                                    default:
                                        mensaje_estado = "Error";
                                        break;
                                }
                                printf("[Cliente %d] %s %s -> %d %s\n",cliente_fd,metodo,ruta_log,codigo_http,mensaje_estado);
                                
                                size_t longitud_respuesta = http_construir_respuesta(estado->buffer,sizeof(estado->buffer),codigo_http,codigo_http == HTTP_OK ? estado->tamano_archivo : 0,tipo_mime,estado->mantener_conexion);
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
                    if (estado->fd_archivo != -1) {
                        close(estado->fd_archivo);
                    }
                    close(cliente_fd); // Cerrar el socket del cliente
                    free(estado);// Liberar la memoria del cliente
                }   
            }
        }
    }
}

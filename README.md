# MiniHTTPd

Servidor HTTP/1.1 desarrollado en lenguaje C para Linux.

## Características

- Procesamiento de solicitudes HTTP mediante el método GET.
- Atención de múltiples clientes utilizando `epoll`.
- Soporte para conexiones persistentes HTTP/1.1.
- Servicio de archivos estáticos HTML, CSS, JavaScript, PNG y JPG.
- Identificación de tipos MIME.
- Respuestas HTTP: 200, 400, 403, 404, 405 y 500.

## Estructura del proyecto

- `src/main.c`: administra las conexiones y el bucle de eventos con epoll.
- `src/server.c`: crea y configura el socket TCP.
- `src/http.c`: analiza solicitudes y construye respuestas HTTP.
- `src/files.c`: valida las rutas y abre los archivos solicitados.
- `src/mime.c`: identifica los tipos MIME.
- `include/`: contiene los archivos de cabecera.
- `www/`: contiene los archivos estáticos del sitio web.
- `Makefile`: permite compilar y limpiar el proyecto.

## Requisitos

- Linux
- GCC
- Make

## Compilación

Desde la carpeta principal del proyecto, ejecutar:

`make`

Para eliminar el ejecutable compilado:

`make clean`

## Ejecución

Iniciar el servidor desde la carpeta principal:

`./minihttpd`

El servidor escuchará conexiones TCP en el puerto **8080**.

## Pruebas

Con el servidor ejecutándose, abrir otra terminal.

**Solicitar la página principal:**

`curl -i http://127.0.0.1:8080/index.html`

**Solicitar una imagen:**

`curl -sS -D - -o /dev/null http://127.0.0.1:8080/image.png`

**Comprobar un archivo inexistente:**

`curl -i http://127.0.0.1:8080/noexiste`

**Comprobar un método no permitido:**

`curl -i -X POST http://127.0.0.1:8080/index.html`

También se puede acceder desde el navegador mediante:

`http://127.0.0.1:8080/`

## Autora

Dayana Martínez

CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -Iinclude

SRC = src/main.c src/server.c src/http.c src/files.c src/mime.c
TARGET = minihttpd

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC) include/server.h include/http.h include/files.h include/mime.h
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)

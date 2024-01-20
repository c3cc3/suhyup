# Makefile

CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -L/usr/lib/x86_64-linux-gnu/ -lwebsockets

server: websocket_server.c
	$(CC) $(CFLAGS) -o websocket_server websocket_server.c $(LDFLAGS)

clean:
	rm -f server


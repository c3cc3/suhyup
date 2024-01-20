#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "fq_common.h"

void send_sse_response(int client_socket, const char *data) {
    // 서버 응답 헤더 설정
    dprintf(client_socket, "HTTP/1.1 200 OK\r\n");
    dprintf(client_socket, "Content-Type: text/event-stream\r\n");
    dprintf(client_socket, "Cache-Control: no-cache\r\n");
    dprintf(client_socket, "Connection: keep-alive\r\n");
    dprintf(client_socket, "Access-Control-Allow-Origin: *\r\n"); // CORS 설정 추가
    dprintf(client_socket, "\r\n");

    // SSE 데이터 전송
    dprintf(client_socket, "data: %s\r\n\r\n", data);

    printf("sse response end.\n");
}

int main() {
    int server_socket, client_socket;
    socklen_t client_len;
    struct sockaddr_in server_addr, client_addr;

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Set up server address struct
    memset((char *)&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    // Bind socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    listen(server_socket, 5);
    printf("Server listening on port 8080...\n");

    while (1) {
        client_len = sizeof(client_addr);
        // Accept a client connection
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Error accepting connection");
            exit(EXIT_FAILURE);
        }
		printf("accepted.\n");

		while(1) {
			char date[9], time[7];
			get_time(date, time);

			char data[1024];
			sprintf(data, "SSE:current time = %s:%s", date, time);
			send_sse_response(client_socket, data);
			sleep(1);
		}

        // Close the client socket
        close(client_socket);
    }

    // Close the server socket
    close(server_socket);

    return 0;
}


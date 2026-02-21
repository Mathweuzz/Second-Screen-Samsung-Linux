#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 2048

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    while(1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }

        printf("New connection from %s:%d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
        
        int valread = read(new_socket, buffer, BUFFER_SIZE - 1);
        if (valread > 0) {
            buffer[valread] = '\0';
            char *first_line = strtok(buffer, "\r\n");
            if (first_line) {
                printf("Request: %s\n", first_line);
            }
        }

        const char *response = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Connection: close\r\n\r\n"
            "<!DOCTYPE html>"
            "<html>"
            "<head>"
            "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
            "<style>body { font-family: sans-serif; text-align: center; margin-top: 50px; background: #222; color: #fff; }</style>"
            "</head>"
            "<body>"
            "<h1>Connection Established</h1>"
            "<p>Server is running.</p>"
            "</body>"
            "</html>";

        send(new_socket, response, strlen(response), 0);
        close(new_socket);
    }

    return 0;
}

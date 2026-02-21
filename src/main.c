#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PORT 8080
#define BUFFER_SIZE 8192

void send_file(int client_socket, const char *filepath, const char *content_type) {
    int file_fd = open(filepath, O_RDONLY);
    if (file_fd < 0) {
        perror("Failed to open file");
        const char *not_found = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n404 Not Found";
        send(client_socket, not_found, strlen(not_found), 0);
        return;
    }

    struct stat file_stat;
    fstat(file_fd, &file_stat);
    long file_size = file_stat.st_size;

    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n\r\n",
             content_type, file_size);

    send(client_socket, header, strlen(header), 0);

    char file_buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, file_buffer, sizeof(file_buffer))) > 0) {
        send(client_socket, file_buffer, bytes_read, 0);
    }

    close(file_fd);
}

void handle_request(int client_socket, char *request) {
    char method[16], path[256];
    if (sscanf(request, "%15s %255s", method, path) != 2) {
        close(client_socket);
        return;
    }

    printf("Request: %s %s\n", method, path);

    if (strcmp(method, "GET") == 0) {
        if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
            send_file(client_socket, "client/index.html", "text/html");
        } else {
            const char *not_found = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\nFile Not Found";
            send(client_socket, not_found, strlen(not_found), 0);
        }
    }
}

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

        memset(buffer, 0, BUFFER_SIZE);
        int valread = read(new_socket, buffer, BUFFER_SIZE - 1);
        if (valread > 0) {
            handle_request(new_socket, buffer);
        }
        
        close(new_socket);
    }

    return 0;
}

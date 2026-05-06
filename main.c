#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

bool has_http_header_end(const char *request, size_t length) {
    for (size_t i = 0; i + 3 < length; i++) {
        if (request[i] == '\r' 
            && request[i + 1] == '\n' 
            && request[i + 2] == '\r' 
            && request[i + 3] == '\n') {
            return true;
        }
    }
    return false;
}

ssize_t receive_request(int socket_fd, char *request, ssize_t request_capacity) {
    ssize_t total_received = 0;
    while (total_received < request_capacity) {
        ssize_t received = recv(
            socket_fd, 
            request + total_received, 
            request_capacity - total_received, 
            0
        );
        if (received <= 0) {
            return -1;
        }
        total_received += received;
        if (has_http_header_end(request, total_received)) {
            break;
        }
    }
    return total_received;
}

int send_response(int socket_fd, char *buffer, size_t length) {
    size_t total_sent = 0;
    while (total_sent < length) {
        ssize_t sent = send(socket_fd, buffer + total_sent, length - total_sent, 0);
        if (sent == -1) {
            return -1;
        }
        total_sent += (size_t)sent;
    }
    return 0;
}

char *get_reason(int status) {
    switch (status) {
        case 200:
            return "OK";
        case 400:
            return "Bad Request";
        case 404:
            return "Not Found";
        case 405:
            return "Method Not Allowed";
        default:
            return "Internal Server Error";
    }
}

void build_response(char *response, size_t response_size, int status, char *body) {
    snprintf(response, response_size, 
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "%s",
        status,
        get_reason(status),
        strlen(body),
        body
    );
}

// 以下の形式の入力を想定する
// "1", "1+1", "1-1+1"
// TODO: "1+" や "1-" で終わる場合のエラー処理を追加する
bool calc(char *query, int *result) {
    char *p = query;
    *result = strtol(p, &p, 10);
    while (*p) {
        if (*p == '+') {
            p++;
            *result += strtol(p, &p, 10);
            continue;
        }
        if (*p == '-') {
            p++;
            *result -= strtol(p, &p, 10);
            continue;
        }
        return false;
    }
    return true;
}

void route_and_build(char *response, ssize_t response_capacity, char *path) {
    if (strcmp(path, "/") == 0) {
        char *body = "Hello World!\n";
        build_response(response, response_capacity, 200, body);
        return;
    }

    if (strncmp(path, "/calc", 5) == 0) {
        char *query = strstr(path, "query=");
        if (query == NULL) {
            build_response(response, response_capacity, 400, "query is required\n");
            return;
        }

        query += strlen("query=");

        int result;
        if (!calc(query, &result)) {
            build_response(response, response_capacity, 400, "invalid query\n");
            return;
        }

        char body[256];
        snprintf(body, sizeof(body), "%d\n", result);
        build_response(response, response_capacity, 200, body);
        return;
    }

    build_response(response, response_capacity, 404, "404 Not Found\n");   
}

int main() {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("socket failed\n");
        exit(1);
    }

    int opt = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    const uint16_t server_port = 8080;

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(socket_fd, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("bind failed\n");
        exit(1);
    }

    if (listen(socket_fd, 5) == -1) {
        perror("listen failed\n");
        exit(1);
    }

    struct sockaddr_in client_address;
    socklen_t client_length = sizeof(client_address);

    while (1) {
        int connected_fd = accept(
            socket_fd, 
            (struct sockaddr *)&client_address, 
            &client_length
        );
        if (connected_fd == -1) {
            perror("accept failed\n");
            exit(1);
        }

        const ssize_t request_capacity = 1024;
        char *request = malloc(request_capacity);
        if (request == NULL) {
            perror("malloc failed\n");
            close(connected_fd);
            exit(1);
        }

        ssize_t total = receive_request(connected_fd, request, request_capacity);
        if (total == -1) {
            perror("recv failed\n");
            close(connected_fd);
            free(request);
            continue;
        }
        request[total] = '\0';
        char *method = strtok(request, " ");
        char *path = strtok(NULL, " ");

        const ssize_t response_size = 1024;
        char *response = malloc(response_size + 1);
        if (response == NULL) {
            perror("malloc failed\n");
            close(connected_fd);
            free(request);
            exit(1);
        }

        if (strcmp(method, "GET") == 0) {
            route_and_build(response, response_size, path);
        } else {
            build_response(response, response_size, 405, "Only GET method is supported\n");
        }

        if (send_response(connected_fd, response, strlen(response)) == -1) {
            perror("send failed\n");
            close(connected_fd);
            free(request);
            free(response);
            exit(1);
        }

        close(connected_fd);
        free(request);
        free(response);
    }

    close(socket_fd);
    return 0;
}
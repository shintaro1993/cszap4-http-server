#define _POSIX_C_SOURCE 200112L

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void parse_args(int argc, char *argv[], char **method, char **path) {
    if (argc == 2) {
        *method = strtok(argv[1], " ");
        *path = strtok(NULL, " ");
    } else {
        fprintf(stderr, "Usage: %s <request> or %s <method> <path>\n", argv[0], argv[0]);
        exit(1);
    }
}

void send_request(int socket_fd, char *request, size_t length) {
    size_t total_sent = 0;
    while (total_sent < length) {
        ssize_t sent = send(socket_fd, request + total_sent, length - total_sent, 0);
        if (sent == -1) {
            perror("send failed");
            close(socket_fd);
            return;
        }
        total_sent += (size_t)sent;
    }
}

ssize_t receive_response(int socket_fd, char *response, ssize_t request_capacity) {
    ssize_t total_received = 0;
    while (1) {
        ssize_t received = recv(
            socket_fd, 
            response + total_received, 
            request_capacity - total_received, 
            0
        );
        if (received == -1) {
            perror("recv failed\n");
            exit(1);
        }
        if (received == 0) {
            break;
        }
        total_received += received;
    }
    return total_received;
}

int main(int argc, char *argv[]) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    const char *server_ip = "127.0.0.1";
    const char *server_port = "8080";

    struct addrinfo *result;
    if (getaddrinfo(server_ip, server_port, &hints, &result) != 0) {
        fprintf(stderr, "getaddrinfo failed\n");
        exit(1);
    }

    struct addrinfo *rp;
    int socket_fd;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (socket_fd == -1) {
            continue;
        }

        if (connect(socket_fd, rp->ai_addr, rp->ai_addrlen) != -1) {
            break;
        }

        close(socket_fd);
    }

    freeaddrinfo(result);

    if (rp == NULL) {
        fprintf(stderr, "connect failed\n");
        exit(1);
    }

    // 引数のパース
    char *method;
    char *path;
    parse_args(argc, argv, &method, &path);

    // リクエストの送信
    const int message_capacity = 1024;
    char message[message_capacity];
    snprintf(message, sizeof(message), "%s %s HTTP/1.1\r\n\r\n", method, path);
    send_request(socket_fd, message, strlen(message));

    // レスポンスの受信
    const int response_capacity = 1024;
    char response[response_capacity];
    ssize_t received_total = receive_response(socket_fd, response, response_capacity); 

    // レスポンスの出力
    response[received_total] = '\0';
    printf("%s", response);

    close(socket_fd);
    return 0;
}
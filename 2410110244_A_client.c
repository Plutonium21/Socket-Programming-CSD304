//Pranshu Upadhyay - 2410110244
//Computer Networks - Lab Assignment 2
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_PORT 9090
#define MAX_BUF 256

int main(int argc, char *argv[]) {
    const char *server_ip = (argc > 1) ? argv[1] : "127.0.0.1";

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, server_ip, &srv_addr.sin_addr);

    if (connect(sock_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    const char *msg = "hello";
    send(sock_fd, msg, strlen(msg), 0);
    printf("Sent: %s\n", msg);

    char ack_buf[MAX_BUF];
    ssize_t n = recv(sock_fd, ack_buf, sizeof(ack_buf) - 1, 0);
    if (n > 0) {
        ack_buf[n] = '\0';
        printf("Received: %s\n", ack_buf);
    }

    close(sock_fd);
    return 0;
}
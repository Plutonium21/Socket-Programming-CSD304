//Pranshu Upadhyay - 2410110244
//Computer Networks - Lab Assignment 2
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>

#define SERVER_PORT 9090
#define MAX_BUF 256

void cleanup_children(int signum) {
    (void)signum;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void serve_client(int conn_fd, struct sockaddr_in client_addr) {
    char incoming[MAX_BUF];
    ssize_t bytes;

    while ((bytes = recv(conn_fd, incoming, sizeof(incoming) - 1, 0)) > 0) {
        incoming[bytes] = '\0';
        printf("[Child %d] Message from %s:%d -> %s\n",
               getpid(), inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), incoming);

        const char *reply = "ACK";
        send(conn_fd, reply, strlen(reply), 0);
    }

    printf("[Child %d] Connection closed by %s:%d\n",
           getpid(), inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    close(conn_fd);
}

int main() {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int reuse = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = INADDR_ANY;
    srv_addr.sin_port = htons(SERVER_PORT);

    if (bind(listen_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(listen_fd, 8) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    signal(SIGCHLD, cleanup_children);
    printf("Server A running on port %d...\n", SERVER_PORT);

    while (1) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        int conn_fd = accept(listen_fd, (struct sockaddr *)&cli_addr, &cli_len);
        if (conn_fd < 0) continue;

        if (fork() == 0) {
            close(listen_fd);
            serve_client(conn_fd, cli_addr);
            exit(EXIT_SUCCESS);
        }
        close(conn_fd);
    }
    return 0;
}
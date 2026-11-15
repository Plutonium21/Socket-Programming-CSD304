#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#define PORT     8080
#define MSG_SIZE 32

static int ack_delay_ms = 0;

static void reap(int sig) { (void)sig; while (waitpid(-1, NULL, WNOHANG) > 0); }

static void handle_client(int cfd, struct sockaddr_in *cli)
{
    char buf[MSG_SIZE], ack[MSG_SIZE];
    ssize_t n;
    while ((n = recv(cfd, buf, MSG_SIZE, MSG_WAITALL)) == MSG_SIZE) {
        buf[MSG_SIZE - 1] = '\0';
        printf("[pid %d] From %s:%d -> %s\n", getpid(),
               inet_ntoa(cli->sin_addr), ntohs(cli->sin_port), buf);
        fflush(stdout);

        int seq = -1;
        sscanf(buf, "hello %d", &seq);
        memset(ack, 0, MSG_SIZE);
        if (seq >= 0) snprintf(ack, MSG_SIZE, "ACK %d", seq);
        else          snprintf(ack, MSG_SIZE, "ACK");
        if (ack_delay_ms) usleep(ack_delay_ms * 1000);
        send(cfd, ack, MSG_SIZE, 0);
    }
    printf("[pid %d] Client %s:%d disconnected\n", getpid(),
           inet_ntoa(cli->sin_addr), ntohs(cli->sin_port));
    close(cfd);
}

int main(int argc, char *argv[])
{
    if (argc > 1) ack_delay_ms = atoi(argv[1]);
    setvbuf(stdout, NULL, _IONBF, 0);
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) { perror("socket"); exit(1); }
    int opt = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    if (bind(sfd, (struct sockaddr *)&addr, sizeof addr) < 0) { perror("bind"); exit(1); }
    if (listen(sfd, 10) < 0) { perror("listen"); exit(1); }

    signal(SIGCHLD, reap);                       /* avoid zombie children */
    printf("Concurrent server listening on port %d...\n", PORT);

    for (;;) {
        struct sockaddr_in cli; socklen_t len = sizeof cli;
        int cfd = accept(sfd, (struct sockaddr *)&cli, &len);
        if (cfd < 0) continue;                   /* EINTR from SIGCHLD etc. */
        printf("New client %s:%d\n", inet_ntoa(cli.sin_addr), ntohs(cli.sin_port));

        pid_t pid = fork();
        if (pid == 0) {
            close(sfd);
            handle_client(cfd, &cli);
            exit(0);
        }
        close(cfd);
    }
}

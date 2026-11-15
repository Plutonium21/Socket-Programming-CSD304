
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#define PORT     8080
#define MSG_SIZE 32 

int main(int argc, char *argv[])
{
    const char *ip = argc > 1 ? argv[1] : "127.0.0.1";
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in srv = {0};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(PORT);
    inet_pton(AF_INET, ip, &srv.sin_addr);
    if (connect(fd, (struct sockaddr *)&srv, sizeof srv) < 0) { perror("connect"); exit(1); }

    char msg[MSG_SIZE] = "hello", ack[MSG_SIZE];
    send(fd, msg, MSG_SIZE, 0);
    printf("Sent: %s\n", msg);

    printf("Waiting for ACK...\n");
    if (recv(fd, ack, MSG_SIZE, MSG_WAITALL) == MSG_SIZE)
        printf("Received: %s\n", ack);

    close(fd);
    return 0;
}

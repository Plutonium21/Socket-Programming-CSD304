//Pranshu Upadhyay - 2410110244
//Computer Networks - Lab Assignment 2
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_PORT 9090
#define WIN_SIZE 5
#define MSG_COUNT 10
#define MAX_BUF 256

int sock_fd;
sem_t slots_available;

void *ack_listener(void *arg) {
    (void)arg;
    FILE *read_stream = fdopen(dup(sock_fd), "r");
    char line[MAX_BUF];

    for (int count = 0; count < MSG_COUNT; count++) {
        if (fgets(line, sizeof(line), read_stream) == NULL) break;
        line[strcspn(line, "\n")] = '\0';
        printf("  <-- Received: %-16s [Window slides +1]\n", line);
        sem_post(&slots_available); // Free 1 slot in the sliding window
    }

    fclose(read_stream);
    return NULL;
}

int main(int argc, char *argv[]) {
    const char *server_ip = (argc > 1) ? argv[1] : "127.0.0.1";

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
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

    sem_init(&slots_available, 0, WIN_SIZE);

    pthread_t listener_tid;
    pthread_create(&listener_tid, NULL, ack_listener, NULL);

    for (int i = 1; i <= MSG_COUNT; i++) {
        int free_slots;
        sem_getvalue(&slots_available, &free_slots);
        if (free_slots == 0) {
            printf("  [!] Window full (5 unACKed messages) -> Blocking...\n");
        }

        sem_wait(&slots_available); // Blocks when 5 messages are unacknowledged

        char out_msg[MAX_BUF];
        snprintf(out_msg, sizeof(out_msg), "hello %d\n", i);
        send(sock_fd, out_msg, strlen(out_msg), 0);
        printf("--> Sent: hello %d\n", i);
    }

    pthread_join(listener_tid, NULL);
    sem_destroy(&slots_available);
    close(sock_fd);
    printf("All %d messages acknowledged.\n", MSG_COUNT);
    return 0;
}
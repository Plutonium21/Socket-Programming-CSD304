#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#define PORT     8080
#define MSG_SIZE 32

#define WINDOW 5
#define TOTAL  12

static int fd, total;
static pthread_cond_t slid = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int base = 0, next = 0;

static void *receiver(void *arg)
{
    (void)arg;
    char ack[MSG_SIZE];
    for (int i = 0; i < total; i++) {
        if (recv(fd, ack, MSG_SIZE, MSG_WAITALL) != MSG_SIZE) break;
        pthread_mutex_lock(&lock);
        base++;
        printf("   <- Received %-8s | window now [%d..%d]\n",
               ack, base, base + WINDOW - 1);
        pthread_cond_signal(&slid);
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    const char *ip = argc > 1 ? argv[1] : "127.0.0.1";
    total = argc > 2 ? atoi(argv[2]) : TOTAL;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in srv = {0};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(PORT);
    inet_pton(AF_INET, ip, &srv.sin_addr);
    if (connect(fd, (struct sockaddr *)&srv, sizeof srv) < 0) { perror("connect"); exit(1); }

    pthread_t rt;
    pthread_create(&rt, NULL, receiver, NULL);

    char msg[MSG_SIZE];
    for (int i = 0; i < total; i++) {
        pthread_mutex_lock(&lock);
        if (next - base >= WINDOW)
            printf("Window full (5 unACKed) -> client BLOCKED\n");
        while (next - base >= WINDOW)
            pthread_cond_wait(&slid, &lock);
        pthread_mutex_unlock(&lock);

        memset(msg, 0, MSG_SIZE);
        snprintf(msg, MSG_SIZE, "hello %d", i);
        send(fd, msg, MSG_SIZE, 0);
        pthread_mutex_lock(&lock);
        next++;
        printf("-> Sent %-10s | outstanding = %d\n", msg, next - base);
        pthread_mutex_unlock(&lock);
    }

    pthread_join(rt, NULL);
    printf("All %d messages acknowledged.\n", total);
    close(fd);
    return 0;
}

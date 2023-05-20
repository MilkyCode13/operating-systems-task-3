#include "params.h"
#include "utils.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define QUEUE_SIZE 10

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
size_t fade_queue[QUEUE_SIZE];
int fade_queue_index = 0;
sem_t queue_sem;
sem_t queue_space_sem;

sem_t flower_sem[FLOWER_COUNT];

pthread_t flowers[FLOWER_COUNT];

void *run_flower(void *arg) {
    int index = (int) arg;

    srandom(time(NULL) ^ (index << 16));
    while (1) {
        sem_wait(&flower_sem[index]);
        sleep(random() % 40);

        pthread_mutex_lock(&queue_mutex);

        sem_wait(&queue_space_sem);
        fade_queue[fade_queue_index] = index;
        fade_queue_index = (fade_queue_index + 1) % QUEUE_SIZE;
        sem_post(&queue_sem);

        pthread_mutex_unlock(&queue_mutex);

        printf("Flower %d has faded!\n", index);
    }

    return NULL;
}

void *run_garden_in(void *arg) {
    int socket_fd = (int) arg;

    struct message message;
    size_t index = 0;
    while (1) {
        sem_wait(&queue_sem);
        message.flower_num = fade_queue[index];
        sem_post(&queue_space_sem);

        if (send(socket_fd, &message, sizeof(message), MSG_NOSIGNAL) < 0) {
            break;
        }

        index = (index + 1) % QUEUE_SIZE;
    }

    close(socket_fd);
    return NULL;
}

void *run_garden_out(void *arg) {
    int socket_fd = (int) arg;

    struct message message;
    while (1) {
        if (recv(socket_fd, &message, sizeof(message), 0) <= 0) {
            break;
        }

        if (message.flower_num >= FLOWER_COUNT) {
            break;
        }

        sem_post(&flower_sem[message.flower_num]);
        printf("Flower %zu was restored!\n", message.flower_num);
    }

    close(socket_fd);
    return NULL;
}

int main(int argc, char **argv) {
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_address.sin_port = htons(11111);

    if (argc == 3) {
        if (inet_aton(argv[1], &server_address.sin_addr) == 0) {
            fprintf(stderr, "Invalid address\n");
            return 1;
        }
        uint16_t port = strtoul(argv[2], NULL, 10);
        if (port == 0) {
            fprintf(stderr, "Invalid port");
            return 1;
        }
        server_address.sin_port = htons(port);
    } else if (argc != 1) {
        return 1;
    }

    sem_init(&queue_sem, 0, 0);
    sem_init(&queue_space_sem, 0, QUEUE_SIZE);

    int socket_in_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_in_fd < 0) {
        perror("socket");
        close(socket_in_fd);
        return 1;
    }
    if (connect(socket_in_fd, (const struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        perror("connect");
        close(socket_in_fd);
        return 1;
    }

    int client_type = GARDEN_IN_CLIENT;
    if (send(socket_in_fd, &client_type, sizeof(client_type), MSG_NOSIGNAL) <= 0) {
        perror("send");
        close(socket_in_fd);
        return 1;
    }

    int socket_out_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_out_fd < 0) {
        perror("socket");
        close(socket_in_fd);
        close(socket_out_fd);
        return 1;
    }
    if (connect(socket_out_fd, (const struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        perror("connect");
        close(socket_in_fd);
        close(socket_out_fd);
        return 1;
    }

    client_type = GARDEN_OUT_CLIENT;
    if (send(socket_out_fd, &client_type, sizeof(client_type), MSG_NOSIGNAL) <= 0) {
        perror("send");
        close(socket_in_fd);
        close(socket_out_fd);
        return 1;
    }

    for (int i = 0; i < FLOWER_COUNT; ++i) {
        sem_init(&flower_sem[i], 0, 1);
        pthread_create(&flowers[i], NULL, run_flower, (void *) i);
    }

    pthread_t in_thread;
    pthread_create(&in_thread, NULL, run_garden_in, (void *) socket_in_fd);

    pthread_t out_thread;
    pthread_create(&out_thread, NULL, run_garden_out, (void *) socket_out_fd);

    pthread_join(in_thread, NULL);
    pthread_join(out_thread, NULL);

    return 0;
}

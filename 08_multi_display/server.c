#include "utils.h"
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

pthread_mutex_t garden_connection_mutex = PTHREAD_MUTEX_INITIALIZER;

int in_connected = 0;
int out_connected = 0;

struct flowers flowers = {{0}};
pthread_mutex_t flower_mutex[FLOWER_COUNT] = {PTHREAD_MUTEX_INITIALIZER};

#define QUEUE_SIZE 10

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
size_t fix_queue[QUEUE_SIZE];
int fix_queue_index = 0;
sem_t queue_sem;
sem_t queue_space_sem;

int display_socket_fd[10] = {-1};
pthread_mutex_t display_mutex = PTHREAD_MUTEX_INITIALIZER;

void display(char *buffer) {
    printf("%s", buffer);

    pthread_mutex_lock(&display_mutex);
    for (int i = 0; i < 10; ++i) {
        if (display_socket_fd[i] != -1) {
            if (recv(display_socket_fd[i], buffer, 0, MSG_DONTWAIT) <= 0 && errno != EAGAIN) {
                close(display_socket_fd[i]);
                display_socket_fd[i] = -1;
                continue;
            }
            if (send(display_socket_fd[i], buffer, strlen(buffer) + 1, MSG_NOSIGNAL) <= 0) {
                close(display_socket_fd[i]);
                display_socket_fd[i] = -1;
            }
        }
    }
    pthread_mutex_unlock(&display_mutex);
}

#define DISPLAY(fmt, ...)                \
    do {                                 \
        char _buf[128];                  \
        sprintf(_buf, fmt, __VA_ARGS__); \
        display(_buf);                   \
    } while (0);

void *handle_garden_in(void *args) {
    int client_socket_fd = (int) args;

    pthread_mutex_lock(&garden_connection_mutex);

    if (in_connected) {
        close(client_socket_fd);
        pthread_mutex_unlock(&garden_connection_mutex);
        return NULL;
    }

    in_connected = 1;

    pthread_mutex_unlock(&garden_connection_mutex);

    display("Connected garden in\n");

    struct message message;
    while (1) {
        if (recv(client_socket_fd, &message, sizeof(message), 0) <= 0) {
            break;
        }

        if (message.flower_num >= FLOWER_COUNT) {
            break;
        }

        pthread_mutex_lock(&flower_mutex[message.flower_num]);
        flowers.flower[message.flower_num] = 1;
        pthread_mutex_unlock(&flower_mutex[message.flower_num]);

        DISPLAY("Flower %zu has faded!\n", message.flower_num);
    }

    close(client_socket_fd);
    return NULL;
}

void *handle_garden_out(void *args) {
    int client_socket_fd = (int) args;

    pthread_mutex_lock(&garden_connection_mutex);

    if (!in_connected | out_connected) {
        close(client_socket_fd);
        pthread_mutex_unlock(&garden_connection_mutex);
        return NULL;
    }

    out_connected = 1;

    pthread_mutex_unlock(&garden_connection_mutex);

    display("Connected garden out\n");

    struct message message;
    size_t index = 0;
    while (1) {
        sem_wait(&queue_sem);
        message.flower_num = fix_queue[index];
        sem_post(&queue_space_sem);

        if (send(client_socket_fd, &message, sizeof(message), MSG_NOSIGNAL) < 0) {
            break;
        }

        index = (index + 1) % QUEUE_SIZE;
    }

    close(client_socket_fd);
    return NULL;
}

void *handle_gardener(void *args) {
    int client_socket_fd = (int) args;

    display("Connected gardener\n");

    struct message message;
    while (1) {
        if (recv(client_socket_fd, &message, sizeof(message), 0) <= 0) {
            break;
        }

        pthread_mutex_lock(&flower_mutex[message.flower_num]);

        send(client_socket_fd, &flowers.flower[message.flower_num], sizeof(flowers.flower[message.flower_num]), MSG_NOSIGNAL);

        if (flowers.flower[message.flower_num]) {
            flowers.flower[message.flower_num] = 0;

            pthread_mutex_lock(&queue_mutex);
            sem_wait(&queue_space_sem);
            fix_queue[fix_queue_index] = message.flower_num;
            fix_queue_index = (fix_queue_index + 1) % QUEUE_SIZE;
            sem_post(&queue_sem);
            pthread_mutex_unlock(&queue_mutex);

            DISPLAY("Flower %zu has been restored by gardener\n", message.flower_num);
        }

        pthread_mutex_unlock(&flower_mutex[message.flower_num]);
    }

    close(client_socket_fd);
    return NULL;
}

void handle_display(int socket_fd) {
    display("Connecting display\n");

    int good = 0;
    pthread_mutex_lock(&display_mutex);
    for (int i = 0; i < 10; ++i) {
        if (display_socket_fd[i] == -1) {
            display_socket_fd[i] = socket_fd;
            good = 1;
            break;
        }
    }
    pthread_mutex_unlock(&display_mutex);

    if (!good) {
        close(socket_fd);
    }
}

int main(int argc, char **argv) {
    struct sockaddr_in socket_address;
    socket_address.sin_family = AF_INET;
    socket_address.sin_addr.s_addr = INADDR_ANY;
    socket_address.sin_port = htons(11111);

    if (argc == 3) {
        if (inet_aton(argv[1], &socket_address.sin_addr) == 0) {
            fprintf(stderr, "Invalid address\n");
            return 1;
        }
        uint16_t port = strtoul(argv[2], NULL, 10);
        if (port == 0) {
            fprintf(stderr, "Invalid port");
            return 1;
        }
        socket_address.sin_port = htons(port);
    } else if (argc != 1) {
        return 1;
    }

    sem_init(&queue_sem, 0, 0);
    sem_init(&queue_space_sem, 0, QUEUE_SIZE);

    int socket_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_fd < 0) {
        perror("socket");
        close(socket_fd);
        return 1;
    }

    int reuse = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt");
        close(socket_fd);
        return 1;
    }

    if (bind(socket_fd, (const struct sockaddr *) &socket_address, sizeof(socket_address)) < 0) {
        perror("bind");
        close(socket_fd);
        return 1;
    }
    if (listen(socket_fd, 10) < 0) {
        perror("listen");
        close(socket_fd);
        return 1;
    }

    while (1) {
        int client_socket_fd = accept(socket_fd, NULL, NULL);

        int client_type;
        if (recv(client_socket_fd, &client_type, sizeof(client_type), 0) <= 0) {
            close(client_socket_fd);
            continue;
        }

        pthread_t pthread;
        switch (client_type) {
            case GARDEN_IN_CLIENT:
                pthread_create(&pthread, NULL, handle_garden_in, (void *) client_socket_fd);
                break;
            case GARDEN_OUT_CLIENT:
                pthread_create(&pthread, NULL, handle_garden_out, (void *) client_socket_fd);
                break;
            case GARDENER_CLIENT:
                pthread_create(&pthread, NULL, handle_gardener, (void *) client_socket_fd);
                break;
            case DISPLAY_CLIENT:
                handle_display(client_socket_fd);
                break;
            default:
                close(client_socket_fd);
        }
    }

    return 0;
}

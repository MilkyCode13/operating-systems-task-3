#include "utils.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

void run_gardener(int socket_fd, int flower_count) {
    pid_t pid = getpid();
    printf("Gardener %d hi uwu!\n", pid);
    srandom(time(NULL) ^ (pid << 8));

    struct message message;
    while (1) {
        for (int i = 0; i < flower_count; ++i) {
            usleep(random() % 1000000);

            message.flower_num = i;
            send(socket_fd, &message, sizeof(message), MSG_NOSIGNAL);

            int status;
            if (recv(socket_fd, &status, sizeof(status), 0) <= 0) {
                close(socket_fd);
                return;
            }

            if (status) {
                printf("Flower %d has been restored by gardener %d\n", i, pid);
            }
        }
    }
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

    int socket_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    connect(socket_fd, (const struct sockaddr *) &server_address, sizeof(server_address));

    int client_type = GARDENER_CLIENT;
    if (send(socket_fd, &client_type, sizeof(client_type), MSG_NOSIGNAL) <= 0) {
        close(socket_fd);
        return 1;
    }

    run_gardener(socket_fd, FLOWER_COUNT);

    return 0;
}

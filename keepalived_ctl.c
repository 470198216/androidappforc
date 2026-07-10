#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <errno.h>

#define SOCKET_PATH "/dev/socket/keepalived"
#define BUFFER_SIZE 1024

void print_usage(const char *prog) {
    printf("Usage: %s <command> [args]\n", prog);
    printf("\nAvailable commands:\n");
    printf("  PING          - Check service alive\n");
    printf("  STATUS        - Show service status\n");
    printf("  COUNTER       - Get keep-alive counter\n");
    printf("  SET_INTERVAL <n> - Set interval in seconds (1-3600)\n");
    printf("  HELP          - Show this help\n");
}

int main(int argc, char **argv) {
    int sock_fd;
    struct sockaddr_un addr;
    char buffer[BUFFER_SIZE];
    int ret;

    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        fprintf(stderr, "Failed to create socket: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    ret = connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        fprintf(stderr, "Failed to connect to %s: %s\n", SOCKET_PATH, strerror(errno));
        close(sock_fd);
        return EXIT_FAILURE;
    }

    char command[BUFFER_SIZE];
    command[0] = '\0';
    
    for (int i = 1; i < argc; i++) {
        if (i > 1) {
            strcat(command, " ");
        }
        strcat(command, argv[i]);
    }
    strcat(command, "\n");

    ret = write(sock_fd, command, strlen(command));
    if (ret < 0) {
        fprintf(stderr, "Failed to send command: %s\n", strerror(errno));
        close(sock_fd);
        return EXIT_FAILURE;
    }

    memset(buffer, 0, BUFFER_SIZE);
    ret = read(sock_fd, buffer, BUFFER_SIZE - 1);
    if (ret > 0) {
        printf("%s", buffer);
    } else if (ret == 0) {
        printf("Connection closed by server\n");
    } else {
        fprintf(stderr, "Failed to read response: %s\n", strerror(errno));
    }

    close(sock_fd);
    return EXIT_SUCCESS;
}
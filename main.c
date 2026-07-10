#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <android/log.h>

#define LOG_TAG "KeepAliveDaemon"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define KEEP_ALIVE_INTERVAL_DEFAULT 3
#define SOCKET_PATH "/dev/socket/keepalived"
#define MAX_CLIENTS 5
#define BUFFER_SIZE 1024

static int running = 1;
static int keep_alive_interval = KEEP_ALIVE_INTERVAL_DEFAULT;
static int keep_alive_counter = 0;

void signal_handler(int sig) {
    switch (sig) {
        case SIGTERM:
        case SIGINT:
            LOGI("Received signal %d, stopping daemon", sig);
            running = 0;
            break;
        default:
            break;
    }
}

void send_response(int client_fd, const char *format, ...) {
    va_list args;
    char buffer[BUFFER_SIZE];
    
    va_start(args, format);
    vsnprintf(buffer, BUFFER_SIZE - 1, format, args);
    va_end(args);
    
    strcat(buffer, "\n");
    write(client_fd, buffer, strlen(buffer));
}

void handle_command(int client_fd, const char *command) {
    char cmd[BUFFER_SIZE];
    char arg[BUFFER_SIZE];
    
    sscanf(command, "%s %s", cmd, arg);
    
    LOGD("Received command: %s", cmd);
    
    if (strcmp(cmd, "PING") == 0) {
        send_response(client_fd, "PONG");
    } else if (strcmp(cmd, "STATUS") == 0) {
        send_response(client_fd, "STATUS OK - PID: %d, Interval: %ds, Counter: %d", 
                      getpid(), keep_alive_interval, keep_alive_counter);
    } else if (strcmp(cmd, "SET_INTERVAL") == 0) {
        int new_interval = atoi(arg);
        if (new_interval > 0 && new_interval <= 3600) {
            keep_alive_interval = new_interval;
            send_response(client_fd, "SET_INTERVAL OK - New interval: %ds", new_interval);
            LOGI("Interval changed to %d seconds", new_interval);
        } else {
            send_response(client_fd, "SET_INTERVAL ERROR - Invalid interval (1-3600)");
        }
    } else if (strcmp(cmd, "COUNTER") == 0) {
        send_response(client_fd, "COUNTER %d", keep_alive_counter);
    } else if (strcmp(cmd, "HELP") == 0) {
        send_response(client_fd, "Available commands:");
        send_response(client_fd, "  PING          - Check service alive");
        send_response(client_fd, "  STATUS        - Show service status");
        send_response(client_fd, "  COUNTER       - Get keep-alive counter");
        send_response(client_fd, "  SET_INTERVAL <n> - Set interval in seconds (1-3600)");
        send_response(client_fd, "  HELP          - Show this help");
    } else {
        send_response(client_fd, "ERROR - Unknown command: %s", cmd);
    }
}

int create_socket(const char *path) {
    int server_fd;
    struct sockaddr_un addr;
    
    unlink(path);
    
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        LOGE("Failed to create socket: %s", strerror(errno));
        return -1;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOGE("Failed to bind socket to %s: %s", path, strerror(errno));
        close(server_fd);
        return -1;
    }
    
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        LOGE("Failed to listen on socket: %s", strerror(errno));
        close(server_fd);
        return -1;
    }
    
    if (chmod(path, 0666) < 0) {
        LOGW("Failed to set socket permissions: %s", strerror(errno));
    }
    
    LOGI("Socket created at %s, FD: %d", path, server_fd);
    return server_fd;
}

int main(int argc, char **argv) {
    struct sigaction sa;
    int server_fd = -1;
    int client_fd;
    fd_set read_fds;
    struct timeval timeout;
    time_t last_keepalive = 0;
    char buffer[BUFFER_SIZE];
    int ret;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    server_fd = create_socket(SOCKET_PATH);
    if (server_fd < 0) {
        LOGE("Failed to create socket, exiting");
        return EXIT_FAILURE;
    }

    LOGI("Daemon started successfully, PID: %d", getpid());
    LOGI("Keep-alive interval: %d seconds", keep_alive_interval);
    LOGI("Service ready to accept commands");

    last_keepalive = time(NULL);

    while (running) {
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        ret = select(server_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (ret < 0 && errno != EINTR) {
            LOGE("select() failed: %s", strerror(errno));
            continue;
        }

        if (FD_ISSET(server_fd, &read_fds)) {
            client_fd = accept(server_fd, NULL, NULL);
            if (client_fd < 0) {
                LOGE("accept() failed: %s", strerror(errno));
                continue;
            }

            memset(buffer, 0, BUFFER_SIZE);
            ret = read(client_fd, buffer, BUFFER_SIZE - 1);
            if (ret > 0) {
                buffer[ret] = '\0';
                handle_command(client_fd, buffer);
            }

            close(client_fd);
        }

        time_t now = time(NULL);
        if (now - last_keepalive >= keep_alive_interval) {
            keep_alive_counter++;
            LOGD("Keep-alive check #%d - Service is alive and running", keep_alive_counter);
            last_keepalive = now;
        }
    }

    close(server_fd);
    unlink(SOCKET_PATH);
    LOGI("Daemon stopping gracefully");
    return EXIT_SUCCESS;
}
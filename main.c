#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <android/log.h>

#define LOG_TAG "KeepAliveDaemon"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define KEEP_ALIVE_INTERVAL 3

static int running = 1;

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

int main(int argc, char **argv) {
    struct sigaction sa;
    
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    LOGI("Daemon started successfully, PID: %d", getpid());
    LOGI("Keep-alive interval: %d seconds", KEEP_ALIVE_INTERVAL);
    LOGI("Daemon will run in background until SIGTERM/SIGINT received");

    int counter = 0;
    while (running) {
        counter++;
        LOGD("Keep-alive check #%d - Service is alive and running", counter);
        sleep(KEEP_ALIVE_INTERVAL);
    }

    LOGI("Daemon stopping gracefully");
    return EXIT_SUCCESS;
}
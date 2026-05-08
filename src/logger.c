#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include "logger.h"

#define LOGSERVER_HOST "127.0.0.1"
#define LOGSERVER_PORT 9091

static int             log_sock  = -1;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static int connect_logserver(void) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(LOGSERVER_PORT);
    inet_pton(AF_INET, LOGSERVER_HOST, &addr.sin_addr);
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    return sock;
}

void logger_init(const char *filepath) {
    (void)filepath; /* le logserver gère le fichier */
    log_sock = connect_logserver();
    if (log_sock < 0)
        fprintf(stderr, "[logger] logserver non joignable sur %s:%d\n",
                LOGSERVER_HOST, LOGSERVER_PORT);
}

void logger_log(const char *client_ip, const char *first_line, int blocked) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    char line[1024];
    int  llen = snprintf(line, sizeof(line), "[%s] %-7s | %s | %s\n",
                         timestamp,
                         blocked ? "BLOCKED" : "ALLOWED",
                         client_ip,
                         first_line);

    pthread_mutex_lock(&log_mutex);
    /* Reconnexion automatique si la connexion est perdue */
    if (log_sock < 0)
        log_sock = connect_logserver();
    if (log_sock >= 0) {
        if (send(log_sock, line, llen, MSG_NOSIGNAL) <= 0) {
            close(log_sock);
            log_sock = connect_logserver();
            if (log_sock >= 0)
                send(log_sock, line, llen, MSG_NOSIGNAL);
        }
    }
    pthread_mutex_unlock(&log_mutex);
}

void logger_close(void) {
    pthread_mutex_lock(&log_mutex);
    if (log_sock >= 0) {
        close(log_sock);
        log_sock = -1;
    }
    pthread_mutex_unlock(&log_mutex);
}

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

/* Socket TCP partagé entre tous les threads pour envoyer les logs au logserver */
static int             log_sock  = -1;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Tentative de connexion au logserver — retourne le fd ou -1 en cas d'échec */
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

/* Initialisation du logger  */
void logger_init(const char *filepath) {
    (void)filepath;  /* le logserver gère le fichier, ce paramètre est ignoré */
    log_sock = connect_logserver();
    if (log_sock < 0)
        fprintf(stderr, "[logger] logserver non joignable sur %s:%d\n",
                LOGSERVER_HOST, LOGSERVER_PORT);
}

/*
 * logger_log() — formate une entrée de log et l'envoie au logserver via TCP.
 *
 * Le mutex garantit qu'un seul thread écrit sur le socket à la fois.
 * Si la connexion est perdue, on tente une reconnexion .
 */
void logger_log(const char *client_ip, const char *first_line, int blocked) {
    /* Construction du timestamp "YYYY-MM-DD HH:MM:SS" */
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    /* Formatage de la ligne de log complète */
    char line[1024];
    int  llen = snprintf(line, sizeof(line), "[%s] %-7s | %s | %s\n",
                         timestamp,
                         blocked ? "BLOCKED" : "ALLOWED",
                         client_ip,
                         first_line);

    /* Envoi thread-safe : un seul thread à la fois sur le socket */
    pthread_mutex_lock(&log_mutex);

    /* Reconnexion automatique si la connexion au logserver a été perdue */
    if (log_sock < 0)
        log_sock = connect_logserver();

    if (log_sock >= 0) {
        /* MSG_NOSIGNAL : évite le signal SIGPIPE si le logserver a fermé la connexion */
        if (send(log_sock, line, llen, MSG_NOSIGNAL) <= 0) {
            /* Envoi échoué — on reconecte et on réessaie une fois */
            close(log_sock);
            log_sock = connect_logserver();
            if (log_sock >= 0)
                send(log_sock, line, llen, MSG_NOSIGNAL);
        }
    }

    pthread_mutex_unlock(&log_mutex);
}

/* Fermeture propre du socket de log */
void logger_close(void) {
    pthread_mutex_lock(&log_mutex);
    if (log_sock >= 0) {
        close(log_sock);
        log_sock = -1;
    }
    pthread_mutex_unlock(&log_mutex);
}

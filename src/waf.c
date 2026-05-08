#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <pthread.h>
#include "filter.h"
#include "logger.h"

#define WAF_PORT    8080
#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 8888
#define BUF_SIZE    8192

typedef struct {
    int  client_fd;
    char client_ip[INET_ADDRSTRLEN];
} client_args_t;

/* Relay bidirectionnel avec select() jusqu'à fermeture d'un côté */
static void relay(int client_fd, int server_fd) {
    char buf[BUF_SIZE];
    int active = 1;

    while (active) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(client_fd, &rfds);
        FD_SET(server_fd, &rfds);
        int maxfd = (client_fd > server_fd) ? client_fd : server_fd;

        if (select(maxfd + 1, &rfds, NULL, NULL, NULL) < 0) break;

        if (FD_ISSET(client_fd, &rfds)) {
            int n = recv(client_fd, buf, BUF_SIZE, 0);
            if (n <= 0) { active = 0; break; }
            if (send(server_fd, buf, n, 0) < 0) { active = 0; break; }
        }
        if (FD_ISSET(server_fd, &rfds)) {
            int n = recv(server_fd, buf, BUF_SIZE, 0);
            if (n <= 0) { active = 0; break; }
            if (send(client_fd, buf, n, 0) < 0) { active = 0; break; }
        }
    }
}

static void *handle_client(void *arg) {
    client_args_t *ca = (client_args_t *)arg;
    int   client_fd   = ca->client_fd;
    char  client_ip[INET_ADDRSTRLEN];
    strncpy(client_ip, ca->client_ip, INET_ADDRSTRLEN);
    free(ca);

    char request[BUF_SIZE] = {0};
    int  total = 0;

    /* Lire la requête jusqu'à \r\n\r\n ou buffer plein */
    while (total < BUF_SIZE - 1) {
        int n = recv(client_fd, request + total, BUF_SIZE - 1 - total, 0);
        if (n <= 0) goto done;
        total += n;
        request[total] = '\0';
        if (strstr(request, "\r\n\r\n") || strstr(request, "\n\n")) break;
    }

    /* Extraire la première ligne */
    char first_line[512] = {0};
    char *nl = strchr(request, '\n');
    if (nl) {
        int len = (int)(nl - request);
        if (len > 0 && request[len-1] == '\r') len--;
        if (len >= (int)sizeof(first_line)) len = (int)sizeof(first_line) - 1;
        strncpy(first_line, request, len);
        first_line[len] = '\0';
    } else {
        strncpy(first_line, request, sizeof(first_line) - 1);
    }

    if (should_block(request)) {
        const char *resp =
            "HTTP/1.0 403 Forbidden\r\n"
            "Content-Type: text/plain\r\n\r\n"
            "Blocked by WAF\r\n";
        send(client_fd, resp, strlen(resp), 0);
        logger_log(client_ip, first_line, 1);
        goto done;
    }

    /* Connexion au serveur backend */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) goto done;

    struct sockaddr_in srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family      = AF_INET;
    srv_addr.sin_port        = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_HOST, &srv_addr.sin_addr);

    if (connect(server_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
        perror("connect to backend");
        close(server_fd);
        goto done;
    }

    /* Relayer la requête déjà lue puis le reste */
    send(server_fd, request, total, 0);
    logger_log(client_ip, first_line, 0);
    relay(client_fd, server_fd);
    close(server_fd);

done:
    close(client_fd);
    return NULL;
}

int main(void) {
    logger_init("waf.log");

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in waf_addr;
    memset(&waf_addr, 0, sizeof(waf_addr));
    waf_addr.sin_family      = AF_INET;
    waf_addr.sin_addr.s_addr = INADDR_ANY;
    waf_addr.sin_port        = htons(WAF_PORT);

    if (bind(listen_fd, (struct sockaddr *)&waf_addr, sizeof(waf_addr)) < 0) {
        perror("bind"); exit(1);
    }
    if (listen(listen_fd, 10) < 0) { perror("listen"); exit(1); }

    printf("Mini-WAF lance sur le port %d -> %s:%d\n",
           WAF_PORT, SERVER_HOST, SERVER_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) { perror("accept"); continue; }

        client_args_t *ca = malloc(sizeof(client_args_t));
        if (!ca) { close(client_fd); continue; }
        ca->client_fd = client_fd;
        inet_ntop(AF_INET, &client_addr.sin_addr, ca->client_ip, INET_ADDRSTRLEN);

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, ca) != 0) {
            perror("pthread_create");
            free(ca);
            close(client_fd);
            continue;
        }
        pthread_detach(tid);
    }

    logger_close();
    close(listen_fd);
    return 0;
}

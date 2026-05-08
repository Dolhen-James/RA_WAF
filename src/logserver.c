#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>

#define LOG_PORT  9091   /* WAF -> logserver (TCP brut, une ligne par entrée) */
#define HTTP_PORT 9090   /* Navigateur -> logserver (HTTP + SSE)              */
#define LOG_FILE  "waf.log"
#define MAX_SSE   64     /* nombre maximum de navigateurs connectés en simultané */
#define BUF_SIZE  4096

                /* Fichier de log  */

static FILE           *log_fp    = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Écriture d'une ligne dans waf.log - protégée par mutex car plusieurs threads peuvent logger */
static void write_log(const char *line) {
    pthread_mutex_lock(&log_mutex);
    if (log_fp) {
        fprintf(log_fp, "%s\n", line);
        fflush(log_fp);  /* flush immédiat pour ne rien perdre si le processus s'arrête */
    }
    pthread_mutex_unlock(&log_mutex);
}

                /* Clients SSE */

/* Liste des fds des navigateurs en attente d'événements SSE */
static int             sse_fds[MAX_SSE];
static int             sse_count = 0;
static pthread_mutex_t sse_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Enregistrement d'un nouveau client dans la liste */
static void add_sse_client(int fd) {
    pthread_mutex_lock(&sse_mutex);
    if (sse_count < MAX_SSE)
        sse_fds[sse_count++] = fd;
    else
        close(fd);  /* liste pleine */
    pthread_mutex_unlock(&sse_mutex);
}

/*
 * broadcast_sse() - diffuse une ligne de log à tous les navigateurs connectés.
 *
 * Le format SSE est "data: <contenu>\n\n".
 * Si un client a fermé son onglet, send() échoue et on le retire de la liste.
 */
static void broadcast_sse(const char *line) {
    char msg[BUF_SIZE];
    int  mlen = snprintf(msg, sizeof(msg), "data: %s\n\n", line);

    pthread_mutex_lock(&sse_mutex);
    int i = 0;
    while (i < sse_count) {
        if (send(sse_fds[i], msg, mlen, MSG_NOSIGNAL) <= 0) {
            /* Client déconnecté  */
            close(sse_fds[i]);
            sse_fds[i] = sse_fds[--sse_count];
        } else {
            i++;
        }
    }
    pthread_mutex_unlock(&sse_mutex);
}

                /* Réception des logs depuis le WAF (port 9091) */

/*
* handle_log_connection() - lit les lignes envoyées par le WAF et les traite.
 *
 * Le WAF envoie du texte brut ligne par ligne. On reconstruit les lignes
 */
static void *handle_log_connection(void *arg) {
    int fd = *(int *)arg;
    free(arg);

    char buf[BUF_SIZE];
    char line[BUF_SIZE];
    int  llen = 0;
    int  n;

    while ((n = recv(fd, buf, sizeof(buf), 0)) > 0) {
        for (int i = 0; i < n; i++) {
            if (buf[i] == '\n') {
                /* Ligne complète reçue - on l'écrit et on la diffuse */
                line[llen] = '\0';
                if (llen > 0) {
                    write_log(line);
                    broadcast_sse(line);
                }
                llen = 0;  /* reset pour la prochaine ligne */
            } else if (llen < BUF_SIZE - 1) {
                line[llen++] = buf[i];
            }
        }
    }

    /* Le WAF a fermé la connexion */
    close(fd);
    return NULL;
}

/* Thread qui écoute sur le port 9091 et crée un thread par connexion WAF */
static void *log_listener(void *arg) {
    (void)arg;
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(LOG_PORT);
    bind(lfd, (struct sockaddr *)&addr, sizeof(addr));
    listen(lfd, 10);

    printf("Log receiver  : port %d\n", LOG_PORT);

    while (1) {
        int *cfd = malloc(sizeof(int));
        *cfd = accept(lfd, NULL, NULL);
        if (*cfd < 0) { free(cfd); continue; }
        pthread_t tid;
        pthread_create(&tid, NULL, handle_log_connection, cfd);
        pthread_detach(tid);
    }
    return NULL;
}

                /* Interface web (port 9090) */

/* Page HTML servie au navigateur - fond sombre, lignes colorées selon ALLOWED/BLOCKED */
static const char HTML[] =
    "<!DOCTYPE html><html><head>"
    "<meta charset=\"utf-8\">"
    "<title>WAF Logs</title>"
    "<style>"
    "body{background:#0d0d0d;color:#ccc;font-family:monospace;margin:0;padding:16px;}"
    "h2{color:#fff;margin:0 0 12px;}"
    "#log{white-space:pre-wrap;word-break:break-all;line-height:1.5;}"
    ".blocked{color:#ff5555;}"
    ".allowed{color:#50fa7b;}"
    "</style></head><body>"
    "<h2>WAF &mdash; Live Logs</h2>"
    "<div id=\"log\"></div>"
    "<script>"
    /* EventSource ouvre une connexion SSE persistante vers /events */
    "const box=document.getElementById('log');"
    "const es=new EventSource('/events');"
    "es.onmessage=e=>{"
    "  const d=document.createElement('div');"
    "  d.className=e.data.includes('BLOCKED')?'blocked':'allowed';"
    "  d.textContent=e.data;"
    "  box.appendChild(d);"
    "  window.scrollTo(0,document.body.scrollHeight);"
    "};"
    "</script></body></html>";

/*
 * serve_sse() - gère une connexion SSE depuis le navigateur.
 *
 * Étapes :
 * 1. Envoie les headers HTTP qui indiquent au navigateur qu'il s'agit d'un flux SSE.
 * 2. Relit waf.log depuis le début pour envoyer l'historique complet.
 * 3. Enregistre le fd dans la liste SSE pour recevoir les futurs événements en direct.
 */
static void serve_sse(int fd) {
    const char *hdr =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n";
    send(fd, hdr, strlen(hdr), MSG_NOSIGNAL);

    /* Replay de l'historique */
    FILE *f = fopen(LOG_FILE, "r");
    if (f) {
        char line[BUF_SIZE];
        while (fgets(line, sizeof(line), f)) {
            /* Nettoyage du \n final avant de formater le message SSE */
            int len = strlen(line);
            while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
                line[--len] = '\0';
            if (len > 0) {
                char msg[BUF_SIZE];
                int mlen = snprintf(msg, sizeof(msg), "data: %s\n\n", line);
                if (send(fd, msg, mlen, MSG_NOSIGNAL) <= 0) {
                    /* Client parti avant la fin du replay */
                    fclose(f);
                    close(fd);
                    return;
                }
            }
        }
        fclose(f);
    }

    /* Ajout du client dans la liste de diffusion en direct */
    add_sse_client(fd);
    /* fd est maintenant géré par broadcast_sse - ne pas fermer ici */
}

/* Traitement d'une connexion HTTP entrante (navigateur sur le port 9090) */
static void *handle_http_connection(void *arg) {
    int fd = *(int *)arg;
    free(arg);

    char buf[BUF_SIZE] = {0};
    recv(fd, buf, sizeof(buf) - 1, 0);  /* lecture de la requête HTTP */

    if (strstr(buf, "GET /events")) {
        /* Le navigateur ouvre le flux SSE - connexion longue durée */
        serve_sse(fd);
        return NULL;
    }

    if (strstr(buf, "GET / ") || strstr(buf, "GET /\r")) {
        /* Demande de la page principale - on sert le HTML */
        char resp[8192];
        int  len = snprintf(resp, sizeof(resp),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n%s",
            strlen(HTML), HTML);
        send(fd, resp, len, MSG_NOSIGNAL);
    } else {
        /* URL inconnue */
        const char *nf = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(fd, nf, strlen(nf), MSG_NOSIGNAL);
    }

    close(fd);
    return NULL;
}

/* Thread qui écoute sur le port 9090 et crée un thread par connexion navigateur */
static void *http_listener(void *arg) {
    (void)arg;
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(HTTP_PORT);
    bind(lfd, (struct sockaddr *)&addr, sizeof(addr));
    listen(lfd, 10);

    printf("Interface web : http://localhost:%d\n", HTTP_PORT);

    while (1) {
        int *cfd = malloc(sizeof(int));
        *cfd = accept(lfd, NULL, NULL);
        if (*cfd < 0) { free(cfd); continue; }
        pthread_t tid;
        pthread_create(&tid, NULL, handle_http_connection, cfd);
        pthread_detach(tid);
    }
    return NULL;
}

                /* Main */

int main(void) {
    /* Ouverture du fichier de log en mode append */
    log_fp = fopen(LOG_FILE, "a");
    if (!log_fp) { perror("fopen waf.log"); exit(1); }

    /* Lancement des deux listeners dans des threads séparés */
    pthread_t t1, t2;
    pthread_create(&t1, NULL, log_listener, NULL);   /* reçoit les logs du WAF */
    pthread_create(&t2, NULL, http_listener, NULL);  /* sert l'interface web */

    /* On attend indéfiniment - le logserver tourne jusqu'à interruption manuelle */
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    return 0;
}

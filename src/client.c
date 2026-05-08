#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUF_SIZE 8192

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <host> <port> <url> [payload]\n", argv[0]);
        return 2;
    }

    /* Récupération des arguments de la ligne de commande */
    const char *host    = argv[1];
    int         port    = atoi(argv[2]);
    const char *url     = argv[3];
    const char *payload = (argc >= 5) ? argv[4] : NULL; 

    /* Création du socket TCP */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 2;
    }

    /* Préparation de l'adresse de destination (IP + port) */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);  

    /* Conversion de la chaîne IP "127.0.0.1" en  binaire */
    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        /* Échec : l'adresse fournie n'est pas une IPv4 valide */
        fprintf(stderr, "Adresse invalide: %s\n", host);
        return 2;
    }

    /* Demande de connexion au serveur */
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        /* Connexion échouée : serveur absent, port fermé, ou pare-feu bloquant */
        perror("connect");
        return 2;
    }
    /* Connexion établie avec succès */

    /* Construction de la requête HTTP à envoyer */
    char request[BUF_SIZE];
    int  req_len;

    if (payload) {
        /* Requête POST */
        int plen = strlen(payload);
        req_len = snprintf(request, sizeof(request),
            "POST %s HTTP/1.0\r\nHost: %s\r\nContent-Length: %d\r\n\r\n%s",
            url, host, plen, payload);
    } else {
        /* Requête GET */
        req_len = snprintf(request, sizeof(request),
            "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n",
            url, host);
    }

    /* Envoi de la requête HTTP au serveur via le socket */
    send(sock, request, req_len, 0);

    /* Réception de la réponse */
    char response[BUF_SIZE] = {0};
    int  total = 0, n;
    while ((n = recv(sock, response + total, BUF_SIZE - 1 - total, 0)) > 0) {
        total += n;
        if (total >= BUF_SIZE - 1) break;  /* sécurité : ne pas déborder le tampon */
    }
    response[total] = '\0';

    /* Fermeture de la connexion  */
    close(sock);

    /* Affichage de la réponse complète du serveur */
    printf("%s\n", response);

    /* Analyse du code de statut HTTP pour déterminer le code de retour du programme */
    if (strstr(response, "403")) return 1;  /* 403 Forbidden : requête bloquée par le WAF */
    if (strstr(response, "200")) return 0;  /* 200 OK : requête acceptée */
    return 2;                               /* autre code ou erreur réseau */
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8888

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int opt = 1;

    /* Réponse HTTP renvoyée à chaque requête */
    char *response =
        "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n"
        "<html><body><h1>Bienvenue sur le Serveur Securise</h1></body></html>";

    /* Création du socket d'écoute */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(1); }

    /* SO_REUSEADDR : permet de relancer le serveur sans attendre l'expiration du TIME_WAIT */
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* Écoute sur toutes les interfaces réseau disponibles, port PORT */
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind"); exit(1);
    }
    if (listen(server_fd, 5) < 0) { perror("listen"); exit(1); }

    printf("Serveur Web cible lance sur le port %d\n", PORT);

    /* Boucle principale : une connexion traitée à la fois (pas de threads ici) */
    while (1) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (new_socket < 0) { perror("accept"); continue; }

        /* Lecture de la requête — on l'ignore, on répond toujours 200 */
        char buffer[4096] = {0};
        read(new_socket, buffer, sizeof(buffer) - 1);

        /* Envoi de la réponse statique puis fermeture immédiate */
        send(new_socket, response, strlen(response), 0);
        close(new_socket);
    }
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <netdb.h>

#define WAF_PORT 8080      // Port sur lequel le WAF écoute
#define WEB_PORT 8000      // Port du vrai serveur (ex: python3 -m http.server)
#define BUF_SIZE 8192

// 🛡️ Fonction de filtrage (Le cœur du WAF)
int is_malicious(char *buffer) {
    char *patterns[] = {"<script>", "../", "OR 1=1", "admin", "union select"};
    for (int i = 0; i < 5; i++) {
        if (strcasestr(buffer, patterns[i])) { // strcasestr ignore la casse
            return 1; 
        }
    }
    return 0;
}

int main() {
    int listen_sock, client_sock, web_sock;
    struct sockaddr_in waf_addr, web_server_addr;
    fd_set readfds;
    char buffer[BUF_SIZE];

    // 1. Setup du socket d'écoute du WAF (Serveur TCP)
    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    waf_addr.sin_family = AF_INET;
    waf_addr.sin_addr.s_addr = INADDR_ANY;
    waf_addr.sin_port = htons(WAF_PORT);
    bind(listen_sock, (struct sockaddr *)&waf_addr, sizeof(waf_addr));
    listen(listen_sock, 5);

    printf("🛡️ Mini-WAF lancé sur le port %d -> Vers serveur sur %d\n", WAF_PORT, WEB_PORT);

    while (1) {
        // Pour ce projet (2-5 clients), on simplifie : on traite un tunnel à la fois
        // ou on utilise un tableau de structures pour gérer plusieurs couples de sockets.
        
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &addr_len);

        // 2. Connexion immédiate au vrai serveur Web (Client TCP)
        web_sock = socket(AF_INET, SOCK_STREAM, 0);
        web_server_addr.sin_family = AF_INET;
        web_server_addr.sin_port = htons(WEB_PORT);
        web_server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (connect(web_sock, (struct sockaddr *)&web_server_addr, sizeof(web_server_addr)) < 0) {
            perror("Erreur : Serveur Web introuvable");
            close(client_sock);
            continue;
        }

        // 3. Boucle de transfert Multiplexée (Select)
        int tunnel_active = 1;
        while (tunnel_active) {
            FD_ZERO(&readfds);
            FD_SET(client_sock, &readfds);
            FD_SET(web_sock, &readfds);

            int max_fd = (client_sock > web_sock) ? client_sock : web_sock;
            select(max_fd + 1, &readfds, NULL, NULL, NULL);

            // A. Données venant du CLIENT (Requête HTTP)
            if (FD_ISSET(client_sock, &readfds)) {
                memset(buffer, 0, BUF_SIZE);
                int n = recv(client_sock, buffer, BUF_SIZE, 0);
                if (n <= 0) { tunnel_active = 0; break; }

                // --- INSPECTION WAF ---
                if (is_malicious(buffer)) {
                    printf("🚫 ATTAQUE BLOQUÉE ! Contenu suspect détecté.\n");
                    char *error = "HTTP/1.1 403 Forbidden\r\nContent-Type: text/plain\r\n\r\nAccès refusé par le Mini-WAF.\n";
                    send(client_sock, error, strlen(error), 0);
                    tunnel_active = 0;
                } else {
                    // Si OK, on transmet au serveur web
                    send(web_sock, buffer, n, 0);
                }
            }

            // B. Données venant du SERVEUR WEB (Réponse HTML)
            if (FD_ISSET(web_sock, &readfds)) {
                memset(buffer, 0, BUF_SIZE);
                int n = recv(web_sock, buffer, BUF_SIZE, 0);
                if (n <= 0) { tunnel_active = 0; break; }
                
                // On retransmet la page au client sans modification
                send(client_sock, buffer, n, 0);
            }
        }
        close(client_sock);
        close(web_sock);
    }
    return 0;
}
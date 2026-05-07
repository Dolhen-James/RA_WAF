#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8000 // Le port ciblé par le WAF

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char *response = "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<html><body><h1>Bienvenue sur le Serveur Securise</h1></body></html>";

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 3);

    printf("🌐 Serveur Web cible lancé sur le port %d...\n", PORT);

    while(1) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        // On lit la requête (juste pour vider le buffer)
        char buffer[1024] = {0};
        read(new_socket, buffer, 1024);
        
        // On envoie la réponse
        send(new_socket, response, strlen(response), 0);
        close(new_socket);
    }
    return 0;
}
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

    const char *host    = argv[1];
    int         port    = atoi(argv[2]);
    const char *url     = argv[3];
    const char *payload = (argc >= 5) ? argv[4] : NULL;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 2; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        fprintf(stderr, "Adresse invalide: %s\n", host);
        return 2;
    }

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 2;
    }

    char request[BUF_SIZE];
    int  req_len;

    if (payload) {
        int plen = strlen(payload);
        req_len = snprintf(request, sizeof(request),
            "POST %s HTTP/1.0\r\nHost: %s\r\nContent-Length: %d\r\n\r\n%s",
            url, host, plen, payload);
    } else {
        req_len = snprintf(request, sizeof(request),
            "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n",
            url, host);
    }

    send(sock, request, req_len, 0);

    char response[BUF_SIZE] = {0};
    int  total = 0, n;
    while ((n = recv(sock, response + total, BUF_SIZE - 1 - total, 0)) > 0) {
        total += n;
        if (total >= BUF_SIZE - 1) break;
    }
    response[total] = '\0';
    close(sock);

    printf("%s\n", response);

    /* Retourne 1 si 403, 0 si 200, 2 sinon */
    if (strstr(response, "403")) return 1;
    if (strstr(response, "200")) return 0;
    return 2;
}

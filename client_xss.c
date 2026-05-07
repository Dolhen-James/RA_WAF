#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {AF_INET, htons(8080), inet_addr("127.0.0.1")};

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;

    char *req = "GET /?search=<script>alert('HACK')</script> HTTP/1.1\r\n\r\n";
    send(sock, req, strlen(req), 0);

    char buf[1024] = {0};
    read(sock, buf, 1024);
    printf("🚫 [Client XSS] Réponse du WAF :\n%s\n", buf);

    close(sock);
    return 0;
}
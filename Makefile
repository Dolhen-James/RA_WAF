CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGETS = waf webserver c_ok c_xss c_sql

.PHONY: all clean

all: $(TARGETS)

waf: mini_waf.c
	$(CC) $(CFLAGS) $< -o $@

webserver: serveur_web.c
	$(CC) $(CFLAGS) $< -o $@

c_ok: client_ok.c
	$(CC) $(CFLAGS) $< -o $@

c_xss: client_xss.c
	$(CC) $(CFLAGS) $< -o $@

c_sql: client_sql.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGETS)

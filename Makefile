CC     = gcc
CFLAGS = -Wall -Wextra -pthread
TARGETS = waf server client logserver

# Anciens binaires du prototype (conservés)
LEGACY = c_ok c_xss c_sql webserver

.PHONY: all legacy clean cleanlog

all: $(TARGETS)

waf: src/waf.c src/filter.c src/logger.c
	$(CC) $(CFLAGS) -Isrc -o $@ $^

server: src/server.c
	$(CC) $(CFLAGS) -o $@ $^

client: src/client.c
	$(CC) $(CFLAGS) -o $@ $^

logserver: src/logserver.c
	$(CC) $(CFLAGS) -o $@ $^

# Cibles legacy du prototype initial
legacy: $(LEGACY)

webserver: serveur_web.c
	$(CC) $(CFLAGS) -o $@ $^

c_ok: client_ok.c
	$(CC) $(CFLAGS) -o $@ $^

c_xss: client_xss.c
	$(CC) $(CFLAGS) -o $@ $^

c_sql: client_sql.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGETS) $(LEGACY)

cleanlog:
	rm -f waf.log

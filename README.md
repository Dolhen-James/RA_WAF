# Projet : Mini Web Application Firewall (WAF)

Proxy de sécurité TCP écrit en C. Intercepte les requêtes HTTP pour filtrer les attaques applicatives (XSS, SQLi, path traversal, command injection) avant qu'elles n'atteignent le serveur web cible.

## Architecture

```
[Client HTTP] ──► [WAF :8080] ──► [Serveur web :8888]
                      │
                      │ TCP :9091
                      ▼
               [LogServer :9091]──► waf.log
                      │
                      │ HTTP :9090
                      ▼
               [Navigateur — logs en direct]
```

```
mini-waf/
├── Makefile
├── src/
│   ├── waf.c           ← proxy principal (pthreads + relay select)
│   ├── filter.c / .h   ← moteur de filtrage (patterns + règles externes)
│   ├── logger.c / .h   ← envoi des logs au logserver via TCP
│   ├── logserver.c     ← serveur de log : écrit waf.log + interface web SSE
│   ├── server.c        ← serveur web minimal (port 8888)
│   └── client.c        ← client HTTP avec args CLI
├── rules/
│   ├── simple_patterns.txt   ← mots-clés supplémentaires
│   └── blocked_urls.txt      ← URLs interdites
├── tests/
│   ├── test_legitimate.sh
│   ├── test_attacks.sh
│   └── test_concurrent.sh
└── rapport/
    └── rapport_mini_waf.md
```

## Fonctionnalités

- **Filtrage simple** : 8 patterns codés en dur (XSS, SQLi, traversal, command injection), insensibles à la casse.
- **Filtrage avancé** : chargement de règles depuis `rules/*.txt` (lignes commençant par `#` ignorées).
- **Concurrence** : un thread par connexion client (`pthread_create` + `pthread_detach`).
- **Relay bidirectionnel** : `select()` pour transférer les données client ↔ serveur sans blocage.
- **Journalisation centralisée** : le WAF envoie chaque entrée au logserver via TCP (port 9091) ; le logserver écrit dans `waf.log` en append — le fichier n'est jamais réécrit.
- **Interface web en direct** : ouvrir `http://localhost:9090` pour observer les logs en temps réel (Server-Sent Events).
- **Réponse 403** : `HTTP/1.0 403 Forbidden` envoyée au client en cas de blocage.

## Compilation

```bash
make          # compile waf, server, client, logserver
make clean    # supprime les binaires et waf.log
```

Flags : `gcc -Wall -Wextra -pthread` — aucun warning.

## Utilisation

```bash
# 1. Démarrer dans cet ordre (logserver en premier)
./logserver &   # port 9091 (logs) + port 9090 (web)
./server &      # port 8888
./waf &         # port 8080
sleep 1

# 2. Ouvrir l'interface web (optionnel)
# http://localhost:9090  ← logs en direct dans le navigateur

# 3. Lancer les tests
bash tests/test_legitimate.sh
bash tests/test_attacks.sh
bash tests/test_concurrent.sh

# 4. Consulter le log
cat waf.log
```

### Client en ligne de commande

```bash
# Requête GET
./client <host> <port> <url>

# Requête POST
./client <host> <port> <url> <payload>

# Exemples
./client 127.0.0.1 8080 /                     # → 200
./client 127.0.0.1 8080 "/?q=<script>alert(1)"# → 403
./client 127.0.0.1 8080 /form "user=alice"     # → 200
```

Code de retour : `0` = 200 OK, `1` = 403 Forbidden, `2` = erreur réseau.

## Règles de filtrage

### Patterns codés en dur (`filter_simple`)

| Motif | Attaque |
|---|---|
| `<script` | XSS |
| `../` | Path traversal |
| `OR 1=1` | SQL Injection |
| `DROP TABLE` | SQL Injection |
| `UNION SELECT` | SQL Injection |
| `alert(` | XSS |
| `javascript:` | XSS |
| `exec(` | Command injection |

### Règles externes (`filter_advanced`)

- `rules/simple_patterns.txt` : `passwd`, `/etc/shadow`, `base64_decode`, `eval(`
- `rules/blocked_urls.txt` : `/admin`, `/phpmyadmin`, `/.env`, `/wp-admin`

Pour ajouter une règle : éditer le fichier correspondant et relancer le WAF.

## Ports par défaut

| Composant | Port | Rôle |
|---|---|---|
| WAF | 8080 | Proxy (clients → WAF) |
| Serveur web | 8888 | Backend HTTP |
| LogServer (log) | 9091 | Réception des entrées de log (TCP brut) |
| LogServer (web) | 9090 | Interface web temps réel (HTTP + SSE) |

## Concepts réseau appliqués

- **TCP (SOCK_STREAM)** : connexions fiables client → WAF → serveur.
- **Double rôle** : le WAF est serveur vis-à-vis du client et client vis-à-vis du backend.
- **I/O Multiplexing** : `select()` dans la fonction `relay()` pour surveiller les deux sockets sans blocage.
- **Pthreads** : un thread par connexion, détaché pour libération automatique des ressources.
- **Mutex** : protection des écritures concurrentes dans `waf.log`.

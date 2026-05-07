# Projet : Mini Web Application Firewall (WAF)

Ce projet est un proxy de sécurité TCP écrit en C. Il intercepte les requêtes HTTP pour filtrer les attaques applicatives avant qu'elles n'atteignent le serveur web cible.

## 🧱 Architecture
* **WAF (Proxy)** : Écoute sur le port `8080`.
* **Serveur Web (Cible)** : Fonctionne sur le port `8000`.
* **Flux** : `Client` ➔ `WAF (Analyse)` ➔ `Serveur Web`.



## 🚀 Fonctionnalités
* **Filtrage de contenu** : Blocage automatique des patterns suspects : `<script>`, `../`, `OR 1=1`, `admin`.
* **Multiplexage (`select`)** : Gestion simultanée des flux entrant (client) et sortant (serveur).
* **Réponse de sécurité** : Envoi d'une page `403 Forbidden` en cas d'attaque détectée.

---

## 🛠️ Compilation
```bash
make
```

Pour recompiler un binaire précis :
```bash
make waf
make webserver
make c_ok
make c_xss
make c_sql
```

Pour nettoyer les exécutables :
```bash
make clean
```
---

## 🏃 Guide d'utilisation

### 1. Démarrer l'infrastructure
Ouvrez au moins trois terminaux :
1. **Terminal 1** : `./webserver` (Le serveur cible)
2. **Terminal 2** : `./waf` (Le pare-feu)

### 2. Lancer les tests
**Option A : Via les clients de test dédiés**
Dans le **Terminal 3**, lancez un client de test :
- `./c_ok` pour une requête valide
- `./c_xss` pour tester la détection XSS
- `./c_sql` pour tester la détection d'injection SQL

Les clients peuvent être lancés dans n'importe quel ordre et plusieurs terminaux sont possibles.

**Exemple de démo :**
1. Lancer `./c_ok` : Le WAF affiche "Relais vers serveur" et le client reçoit la page.
2. Lancer `./c_xss` : Le WAF affiche "🚫 ATTAQUE BLOQUÉE" et le client reçoit un `403 Forbidden`.
3. Lancer les trois quasiment en même temps : Grâce à la boucle `while(1)` et aux sockets, le WAF traite chaque demande indépendamment.


**Option B : Via le navigateur**
* **Normal** : `http://localhost:8080`
* **Attaque** : `http://localhost:8080/?search=<script>`

---

## 📊 Concepts Réseaux Appliqués
* **Mode Connecté (TCP)** : Sockets `SOCK_STREAM`.
* **Multi-rôle** : Le WAF est à la fois Serveur (accept) et Client (connect).
* **I/O Multiplexing** : Utilisation de `select()` pour surveiller les deux sockets de chaque tunnel sans bloquer le programme.

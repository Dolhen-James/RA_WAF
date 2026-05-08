#!/bin/bash
# Requêtes légitimes — chacune doit recevoir un HTTP 200 (code retour 0)

PASS=0; FAIL=0

check() {
    local desc="$1"; shift
    if "$@"; then
        echo "[PASS] $desc"
        PASS=$((PASS+1))
    else
        echo "[FAIL] $desc"
        FAIL=$((FAIL+1))
    fi
}

echo "=== Tests requetes legitimes ==="

check "GET /"              ./client 127.0.0.1 8080 /
check "GET /index.html"    ./client 127.0.0.1 8080 /index.html
check "GET /about?name=John" ./client 127.0.0.1 8080 "/about?name=John"
check "POST avec donnees simples" ./client 127.0.0.1 8080 /form "username=alice&password=secret"

echo ""
echo "Resultat : $PASS PASS, $FAIL FAIL"
[ $FAIL -eq 0 ]

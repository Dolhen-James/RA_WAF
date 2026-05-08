#!/bin/bash
# Requêtes malveillantes — chacune doit être bloquée avec HTTP 403 (code retour 1)

PASS=0; FAIL=0

check_blocked() {
    local desc="$1"; shift
    "$@"
    local code=$?
    if [ $code -eq 1 ]; then
        echo "[PASS] $desc bloque (403)"
        PASS=$((PASS+1))
    else
        echo "[FAIL] $desc non bloque (code=$code)"
        FAIL=$((FAIL+1))
    fi
}

echo "=== Tests requetes malveillantes ==="

# XSS
check_blocked "XSS <script>"        ./client 127.0.0.1 8080 "/?q=<script>alert(1)</script>"
check_blocked "XSS alert("          ./client 127.0.0.1 8080 "/?q=alert(xss)"
check_blocked "XSS javascript:"     ./client 127.0.0.1 8080 "/?url=javascript:void(0)"

# SQL Injection
check_blocked "SQLi OR 1=1"         ./client 127.0.0.1 8080 "/?id=1 OR 1=1"
check_blocked "SQLi DROP TABLE"     ./client 127.0.0.1 8080 "/?q=DROP TABLE users"
check_blocked "SQLi UNION SELECT"   ./client 127.0.0.1 8080 "/?q=UNION SELECT * FROM users"

# Path traversal
check_blocked "Path traversal ../"  ./client 127.0.0.1 8080 "/../../etc/passwd"

# Command injection
check_blocked "Command injection exec(" ./client 127.0.0.1 8080 "/?cmd=exec(ls)"

# URLs bloquées (fichier de règles)
check_blocked "/admin"              ./client 127.0.0.1 8080 /admin
check_blocked "/phpmyadmin"         ./client 127.0.0.1 8080 /phpmyadmin
check_blocked "/.env"               ./client 127.0.0.1 8080 /.env
check_blocked "/wp-admin"           ./client 127.0.0.1 8080 /wp-admin

# Mots-clés fichier de règles
check_blocked "passwd"              ./client 127.0.0.1 8080 "/?file=passwd"
check_blocked "base64_decode"       ./client 127.0.0.1 8080 "/?q=base64_decode(xyz)"
check_blocked "eval("               ./client 127.0.0.1 8080 "/?q=eval(code)"

echo ""
echo "Resultat : $PASS PASS, $FAIL FAIL"
[ $FAIL -eq 0 ]

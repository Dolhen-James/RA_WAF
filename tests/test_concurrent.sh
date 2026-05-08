#!/bin/bash
# 5 clients simultanés envoyant GET / — vérifie 5 entrées ALLOWED dans waf.log

echo "=== Test concurrence (5 clients simultanees) ==="

# Supprimer les entrées précédentes pour compter proprement
> waf.log 2>/dev/null || true

for i in $(seq 1 5); do
    ./client 127.0.0.1 8080 / > /dev/null 2>&1 &
done
wait

sleep 0.2  # laisse le temps aux threads WAF d'écrire dans waf.log

COUNT=$(grep -c "ALLOWED" waf.log 2>/dev/null || echo 0)

if [ "$COUNT" -ge 5 ]; then
    echo "[PASS] $COUNT entrees ALLOWED trouvees dans waf.log (>= 5)"
else
    echo "[FAIL] Seulement $COUNT entrees ALLOWED dans waf.log (attendu >= 5)"
    exit 1
fi

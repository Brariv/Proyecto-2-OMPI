#!/usr/bin/env bash
# Inciso 2a: tiempo para encontrar la clave 123456 con -np 4 (estrategia
# base ciclica). La clave es pequena, asi que se busca desde 0.
source "$(dirname "$0")/comun.sh"
echo "===== INCISO 2a: clave 123456, estrategia ciclica (base), -np $NP ====="
echo "Texto:  $(cat "$TEXTO")"
echo "Frase:  \"$FRASE\""; echo
"${MPIRUN[@]}" -np "$NP" "$MPI" -s "$K_PEQUENA" -b 24 -f "$TEXTO" -p "$FRASE" -e ciclica

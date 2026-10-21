#!/usr/bin/env bash
# Inciso 2c: tiempo para encontrar la clave 2^56/4 + 1 = 18014398509481985
# con -np 4. Misma estructura que 2b. Con bloques es la SEGUNDA clave del
# bloque del proceso 1, asi que se encuentra en la segunda prueba.
source "$(dirname "$0")/comun.sh"
INI="$(python3 -c "print($K_POT2_MAS1 - (1 << ($VENTANA_BITS - 1)))")"
echo "===== INCISO 2c: clave 2^56/4+1 = $K_POT2_MAS1, -np $NP ====="
echo "Texto:  $(cat "$TEXTO")"; echo "Frase:  \"$FRASE\""; echo

echo "--- (1) Estrategia base ciclica, ventana centrada 2^$VENTANA_BITS ---"
"${MPIRUN[@]}" -np "$NP" "$MPI" -s "$K_POT2_MAS1" -b "$VENTANA_BITS" -S "$INI" -f "$TEXTO" -p "$FRASE" -e ciclica
echo
echo "--- (2) Estrategia por bloques, espacio completo 2^56 ---"
"${MPIRUN[@]}" -np "$NP" "$MPI" -s "$K_POT2_MAS1" -b 56 -f "$TEXTO" -p "$FRASE" -e bloques

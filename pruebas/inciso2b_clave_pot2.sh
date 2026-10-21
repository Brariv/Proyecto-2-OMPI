#!/usr/bin/env bash
# Inciso 2b: tiempo para encontrar la clave 2^56/4 = 18014398509481984
# con -np 4. Sobre el espacio completo la busqueda ciclica es inviable
# (proyeccion de anios); se muestran dos vistas:
#   1) estrategia base ciclica sobre ventana centrada (para obtener un tiempo),
#   2) estrategia por bloques sobre el espacio completo (la encuentra de inmediato).
source "$(dirname "$0")/comun.sh"
INI="$(python3 -c "print($K_POT2 - (1 << ($VENTANA_BITS - 1)))")"
echo "===== INCISO 2b: clave 2^56/4 = $K_POT2, -np $NP ====="
echo "Texto:  $(cat "$TEXTO")"; echo "Frase:  \"$FRASE\""; echo

echo "--- (1) Estrategia base ciclica, ventana centrada 2^$VENTANA_BITS ---"
"${MPIRUN[@]}" -np "$NP" "$MPI" -s "$K_POT2" -b "$VENTANA_BITS" -S "$INI" -f "$TEXTO" -p "$FRASE" -e ciclica
echo
echo "--- (2) Estrategia por bloques, espacio completo 2^56 ---"
"${MPIRUN[@]}" -np "$NP" "$MPI" -s "$K_POT2" -b 56 -f "$TEXTO" -p "$FRASE" -e bloques

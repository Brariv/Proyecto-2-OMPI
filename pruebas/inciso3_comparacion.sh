#!/usr/bin/env bash
# Inciso 3: compara la estrategia base (ciclica) con la estrategia
# alternativa (heuristica + bloques) para las tres claves, con -np 4.
# Muestra lado a lado el tiempo y la fase que encuentra la clave.
source "$(dirname "$0")/comun.sh"

echo "===== INCISO 3: comparacion de estrategias, -np $NP ====="
echo "Texto: $(cat "$TEXTO")   Frase: \"$FRASE\""; echo

corre() {  # <clave> <etiqueta> <estrategia> <bits> <inicio>
    local salida
    salida="$("${MPIRUN[@]}" -np "$NP" "$MPI" -s "$1" -b "$4" -S "$5" \
              -f "$TEXTO" -p "$FRASE" -e "$3" 2>/dev/null)"
    local t fase
    t="$(printf '%s\n' "$salida" | awk '/Tiempo de busqueda/ {print $4}')"
    fase="$(printf '%s\n' "$salida" | awk -F': ' '/^Fase/ {print $2}')"
    printf '  %-11s | %-24s | t=%9ss | %s\n' "$2" "$3" "$t" "$fase"
}

INI_P2="$(python3 -c "print($K_POT2 - (1 << ($VENTANA_BITS - 1)))")"
INI_P2b="$(python3 -c "print($K_POT2_MAS1 - (1 << ($VENTANA_BITS - 1)))")"

echo "Clave 123456 (pequena):"
corre "$K_PEQUENA" "123456" ciclica    56 0
corre "$K_PEQUENA" "123456" bloques    56 0
corre "$K_PEQUENA" "123456" heuristica 56 0
echo "Clave 2^56/4 (potencia de dos):"
corre "$K_POT2" "2^56/4" ciclica    "$VENTANA_BITS" "$INI_P2"    # ventana centrada (completo = inviable)
corre "$K_POT2" "2^56/4" bloques    56 0
corre "$K_POT2" "2^56/4" heuristica 56 0
echo "Clave 2^56/4+1 (vecina de potencia):"
corre "$K_POT2_MAS1" "2^56/4+1" ciclica    "$VENTANA_BITS" "$INI_P2b"
corre "$K_POT2_MAS1" "2^56/4+1" bloques    56 0
corre "$K_POT2_MAS1" "2^56/4+1" heuristica 56 0
echo
echo "Nota: la ciclica con claves gigantes usa ventana centrada 2^$VENTANA_BITS;"
echo "sobre el espacio completo 2^56 seria inviable (proyeccion de anios)."

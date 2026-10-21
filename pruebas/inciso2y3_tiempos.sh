#!/usr/bin/env bash
# Inciso 2 y 3: mide los tiempos de busqueda para las tres claves del
# enunciado y genera los CSV que alimentan las tablas del reporte.
#
#   - Inciso 2 (a,b,c): tiempo de la estrategia base (ciclica) con -np 4.
#   - Inciso 3: comparacion ciclica vs bloques vs heuristica con -np 4.
#   - Speedup: secuencial vs -np 2 vs -np 4 (estrategia ciclica).
#
# Para las claves gigantes (2^56/4 y +1) la estrategia ciclica es inviable
# sobre el espacio completo [0, 2^56): se mide sobre una ventana centrada
# de 2^VENTANA_BITS candidatas alrededor del valor objetivo (concesion
# documentada). Bloques y heuristica si operan sobre el espacio completo,
# porque encuentran estas claves de inmediato.
source "$(dirname "$0")/comun.sh"

CSV_EST="$RESULTADOS/estrategias.csv"
CSV_SPD="$RESULTADOS/speedup.csv"
LOG="$RESULTADOS/corridas.log"
: > "$LOG"

echo "clave,etiqueta,estrategia,np,bits,inicio,espacio,tiempo_s,candidatas,fase" > "$CSV_EST"
echo "clave,etiqueta,np,bits,inicio,tiempo_s" > "$CSV_SPD"

# Ventana centrada alrededor de una clave: inicio = clave - 2^(bits-1).
inicio_centrado() {
    python3 -c "print($1 - (1 << ($VENTANA_BITS - 1)))"
}

# corre_y_registra <clave> <etiqueta> <estrategia> <np> <bits> <inicio> <espacio>
# Ejecuta una vez para capturar candidatas/fase y mide la mediana de tiempo.
corre_y_registra() {
    local clave="$1" etiq="$2" est="$3" np="$4" bits="$5" ini="$6" espacio="$7"
    local -a cmd
    if [ "$np" -eq 1 ]; then
        cmd=("$SEQ" -s "$clave" -b "$bits" -S "$ini" -f "$TEXTO" -p "$FRASE" -e "$est")
    else
        cmd=("${MPIRUN[@]}" -np "$np" "$MPI" -s "$clave" -b "$bits" -S "$ini" -f "$TEXTO" -p "$FRASE" -e "$est")
    fi

    local salida cand fase t
    salida="$("${cmd[@]}" 2>/dev/null)"
    cand="$(printf '%s\n' "$salida" | awk -F': ' '/Candidatas probadas/ {print $2}')"
    [ -z "$cand" ] && cand="$(printf '%s\n' "$salida" | awk -F': ' '/Candidatas probadas/ {print $NF}')"
    fase="$(printf '%s\n' "$salida" | awk -F': ' '/^Fase/ {print $2}')"
    t="$(medir_tiempo "${cmd[@]}")"

    {
        echo "### clave=$clave ($etiq) estrategia=$est np=$np bits=$bits inicio=$ini espacio=$espacio"
        printf '%s\n\n' "$salida"
    } >> "$LOG"

    echo "$clave,$etiq,$est,$np,$bits,$ini,$espacio,$t,${cand// /},${fase:-NA}" >> "$CSV_EST"
    printf '  %-26s np=%d  %-11s  t=%ss\n' "$etiq/$est" "$np" "[$espacio]" "$t"
}

echo "=========== Midiendo (NP=$NP, REPS=$REPS, VENTANA=2^$VENTANA_BITS) ==========="
echo ">> Esto toma varios minutos por las repeticiones. Hardware:"
echo "   cores=$(nproc)  $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2 | xargs)"
echo

# --------- INCISO 3: comparacion de estrategias (-np 4) ---------
# 123456: las tres estrategias sobre el espacio completo (ciclica lo
#         resuelve rapido porque la clave es pequena).
echo "[123456]"
corre_y_registra "$K_PEQUENA" "123456" ciclica    "$NP" "$BITS_COMPLETO" 0 "completo 2^56"
corre_y_registra "$K_PEQUENA" "123456" bloques    "$NP" "$BITS_COMPLETO" 0 "completo 2^56"
corre_y_registra "$K_PEQUENA" "123456" heuristica "$NP" "$BITS_COMPLETO" 0 "completo 2^56"

# 2^56/4 y +1: bloques y heuristica sobre el espacio completo; ciclica
#              sobre ventana centrada (inviable sobre el completo).
for pareja in "$K_POT2:2^56/4" "$K_POT2_MAS1:2^56/4+1"; do
    clave="${pareja%%:*}"; etiq="${pareja##*:}"
    ini="$(inicio_centrado "$clave")"
    echo "[$etiq]"
    corre_y_registra "$clave" "$etiq" bloques    "$NP" "$BITS_COMPLETO" 0     "completo 2^56"
    corre_y_registra "$clave" "$etiq" heuristica "$NP" "$BITS_COMPLETO" 0     "completo 2^56"
    corre_y_registra "$clave" "$etiq" ciclica    "$NP" "$VENTANA_BITS"  "$ini" "ventana 2^$VENTANA_BITS centrada"
done

# --------- SPEEDUP: ciclica, np = 1 (seq), 2, 4, sobre la misma ventana ---------
echo; echo "[speedup ciclica: np=1,2,4 sobre ventana comun por clave]"
# 123456 sobre [0, 2^VENTANA_BITS); claves gigantes sobre ventana centrada.
for pareja in "$K_PEQUENA:123456:0" \
              "$K_POT2:2^56/4:C" \
              "$K_POT2_MAS1:2^56/4+1:C"; do
    clave="$(echo "$pareja" | cut -d: -f1)"
    etiq="$(echo "$pareja" | cut -d: -f2)"
    modo="$(echo "$pareja" | cut -d: -f3)"
    if [ "$modo" = "C" ]; then ini="$(inicio_centrado "$clave")"; else ini=0; fi
    for np in 1 2 4; do
        t="$(medir_tiempo $([ "$np" -eq 1 ] && echo "$SEQ" || echo "${MPIRUN[@]} -np $np $MPI") \
             -s "$clave" -b "$VENTANA_BITS" -S "$ini" -f "$TEXTO" -p "$FRASE" -e ciclica)"
        echo "$clave,$etiq,$np,$VENTANA_BITS,$ini,$t" >> "$CSV_SPD"
        printf '  %-12s np=%d  t=%ss\n' "$etiq" "$np" "$t"
    done
done

echo; echo "CSV de estrategias : $CSV_EST"
echo "CSV de speedup     : $CSV_SPD"
echo "Log de corridas    : $LOG"

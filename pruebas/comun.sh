#!/usr/bin/env bash
# Definiciones compartidas por los scripts de prueba del Proyecto 02.
# Cada script hace:  source "$(dirname "$0")/comun.sh"
set -euo pipefail

# Raiz del proyecto (un nivel arriba de pruebas/).
PRUEBAS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RAIZ="$(cd "$PRUEBAS_DIR/.." && pwd)"

BIN="$RAIZ/bin"
TEXTOS="$PRUEBAS_DIR/textos"
RESULTADOS="$PRUEBAS_DIR/resultados"
mkdir -p "$RESULTADOS"

SEQ="$BIN/busqueda_secuencial"
MPI="$BIN/busqueda_mpi"
ARCHIVO="$BIN/aes_archivo"

TEXTO="$TEXTOS/inciso2_texto.txt"     # "Esta es una prueba de proyecto 2"
FRASE="es una prueba de"               # frase de busqueda del enunciado

# Claves del enunciado (inciso 2).
K_PEQUENA=123456
K_POT2=18014398509481984              # 2^56 / 4  = 2^54
K_POT2_MAS1=18014398509481985        # 2^56 / 4 + 1

# Parametros de medicion.
NP=${NP:-4}                            # procesos MPI (enunciado: 4)
REPS=${REPS:-5}                        # repeticiones por medicion (se toma la mediana)
VENTANA_BITS=${VENTANA_BITS:-22}       # tamano de ventana para claves gigantes / speedup
BITS_COMPLETO=56                       # espacio conceptual completo [0, 2^56)

# Permite ejecutar mpirun como root (entornos de contenedor); inofensivo
# en una Mac o Linux de escritorio con usuario normal.
if [ "$(id -u)" = "0" ]; then
    export OMPI_ALLOW_RUN_AS_ROOT=1 OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1
fi
MPIRUN=(mpirun --oversubscribe)        # --oversubscribe por si hay menos cores que procesos

# ---------------------------------------------------------------------
# medir_tiempo "<comando...>"  -> imprime la mediana de "Tiempo de busqueda"
# Ejecuta el comando REPS veces, extrae el tiempo de busqueda de cada
# corrida y devuelve la mediana (robusta ante ruido del sistema).
# ---------------------------------------------------------------------
medir_tiempo() {
    local -a tiempos=()
    local salida t
    for ((i = 0; i < REPS; i++)); do
        salida="$("$@" 2>/dev/null)"
        t="$(printf '%s\n' "$salida" | awk '/Tiempo de busqueda/ {print $4}')"
        tiempos+=("$t")
    done
    printf '%s\n' "${tiempos[@]}" | sort -n | awk '
        { v[NR] = $1 }
        END { if (NR % 2) print v[(NR+1)/2]; else printf "%.6f\n", (v[NR/2] + v[NR/2+1]) / 2 }'
}

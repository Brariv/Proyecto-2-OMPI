#!/usr/bin/env bash
# Ejecuta toda la bateria de pruebas del Proyecto 02 y genera las tablas.
set -euo pipefail
AQUI="$(cd "$(dirname "$0")" && pwd)"
RAIZ="$(cd "$AQUI/.." && pwd)"

echo ">> Compilando..."
make -C "$RAIZ"

echo; echo ">> Inciso 1 (cifrado/descifrado de archivo)"
bash "$AQUI/inciso1_cifrado.sh" | tee "$RAIZ/pruebas/resultados/inciso1.log"

echo; echo ">> Inciso 2 y 3 (tiempos y speedup)"
bash "$AQUI/inciso2y3_tiempos.sh"

echo; echo ">> Generando tablas Markdown"
VENTANA_BITS="${VENTANA_BITS:-22}" python3 "$AQUI/generar_tablas.py"

echo; echo ">> Listo. Revisa pruebas/resultados/ (tablas.md, *.csv, *.log)."

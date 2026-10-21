#!/usr/bin/env bash
# Inciso 1: cargar un .txt, cifrarlo con AES-128 y una clave dada, y
# descifrarlo con la misma clave hacia un archivo distinto. Muestra el
# texto original, el cifrado (hex) y el recuperado, y verifica que el
# recuperado coincida con el original.
source "$(dirname "$0")/comun.sh"

OUT="$RESULTADOS/inciso1"; mkdir -p "$OUT"
ENTRADA="$TEXTOS/inciso1_mensaje.txt"
CIFRADO="$OUT/mensaje_cifrado.bin"
RECUP="$OUT/mensaje_recuperado.txt"
CLAVE=20240926

echo "================ INCISO 1: cifrado y descifrado de archivo ================"
echo "Clave de cifrado: $CLAVE"; echo
echo "----- Texto original ($ENTRADA) -----"; cat "$ENTRADA"; echo; echo

echo "----- Cifrando -----"
"$ARCHIVO" -e -k "$CLAVE" -i "$ENTRADA" -o "$CIFRADO"; echo

echo "----- Texto cifrado (hex, primeros 64 bytes; empieza con el IV) -----"
od -A d -t x1 -N 64 "$CIFRADO"; echo

echo "----- Descifrando (misma clave, archivo de salida distinto) -----"
"$ARCHIVO" -d -k "$CLAVE" -i "$CIFRADO" -o "$RECUP"; echo

echo "----- Texto recuperado ($RECUP) -----"; cat "$RECUP"; echo; echo

echo "----- Verificacion -----"
if diff -q "$ENTRADA" "$RECUP" >/dev/null; then
    echo "OK: el texto recuperado es identico al original."
else
    echo "ERROR: el texto recuperado difiere del original."; exit 1
fi

echo; echo "----- Prueba negativa: descifrar con clave incorrecta -----"
if "$ARCHIVO" -d -k 111111 -i "$CIFRADO" -o "$OUT/basura.txt" 2>/dev/null; then
    echo "ERROR: deberia haber fallado con clave incorrecta."; exit 1
else
    echo "OK: con clave incorrecta el descifrado se rechaza (relleno PKCS#7 invalido)."
    rm -f "$OUT/basura.txt"
fi

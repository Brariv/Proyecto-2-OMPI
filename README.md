# Proyecto 02 — Búsqueda paralela de una clave AES-128 con Open MPI
CC3069 Computación Paralela y Distribuida — Universidad del Valle de Guatemala

Optimización de un programa paralelo de fuerza bruta sobre AES-128: cifrado/descifrado de
archivos, búsqueda de la clave con Open MPI y comparación de estrategias de distribución del
trabajo.

## Estructura

```
proyecto02/
├── Makefile
├── src/
│   ├── aes_comun.h            # núcleo común: config, make_key, cifrado, try_candidate
│   ├── diccionario.h          # diccionario de claves probables (fase heurística)
│   ├── aes_archivo.c          # inciso 1: cifrar/descifrar un .txt a archivo
│   ├── busqueda_secuencial.c  # búsqueda secuencial (línea base del speedup)
│   └── busqueda_mpi.c         # búsqueda MPI: 3 estrategias (ciclica, bloques, heuristica)
└── pruebas/
    ├── comun.sh               # parámetros y utilidades compartidas
    ├── textos/                # un .txt por inciso
    ├── inciso1_cifrado.sh     # inciso 1
    ├── inciso2a_clave123456.sh
    ├── inciso2b_clave_pot2.sh
    ├── inciso2c_clave_pot2_mas1.sh
    ├── inciso3_comparacion.sh
    ├── inciso2y3_tiempos.sh   # batería completa -> CSV + tablas
    ├── generar_tablas.py      # CSV -> tablas Markdown
    ├── run_all.sh             # ejecuta todo
    └── resultados/            # logs, CSV y tablas.md (evidencia)
```

## Requisitos
- `gcc`, Open MPI (`mpicc`, `mpirun`), OpenSSL 3 (`libssl-dev` / `openssl`).
- En Ubuntu/Debian: `sudo apt-get install -y build-essential openmpi-bin libopenmpi-dev libssl-dev`
- En macOS (Homebrew): `brew install open-mpi openssl` (puede requerir
  `export LDFLAGS=-L$(brew --prefix openssl)/lib CPPFLAGS=-I$(brew --prefix openssl)/include`).

## Compilación
```bash
make            # genera bin/aes_archivo, bin/busqueda_secuencial, bin/busqueda_mpi
make clean
```

### Sin make (compilación manual)
Equivale exactamente a lo que hace el Makefile. Primero se crea la carpeta de
salida y luego se compila cada programa:
```bash
mkdir -p bin

# Inciso 1: herramienta de cifrado/descifrado de archivos (gcc + OpenSSL)
gcc -std=c11 -O2 -Wall -Wextra src/aes_archivo.c -o bin/aes_archivo -lcrypto

# Búsqueda secuencial (línea base del speedup)
gcc -std=c11 -O2 -Wall -Wextra src/busqueda_secuencial.c -o bin/busqueda_secuencial -lcrypto

# Búsqueda paralela MPI (se compila con mpicc, no con gcc)
mpicc -std=c11 -O2 -Wall -Wextra src/busqueda_mpi.c -o bin/busqueda_mpi -lcrypto
```
> `busqueda_mpi` depende de `src/aes_comun.h` y `src/diccionario.h`; no hace falta
> nombrarlos en la línea de compilación porque se incluyen desde el `.c`.
> En macOS, si OpenSSL es de Homebrew, agregar las rutas:
> `gcc ... -I$(brew --prefix openssl)/include -L$(brew --prefix openssl)/lib -lcrypto`.

Limpiar manualmente (equivalente a `make clean`):
```bash
rm -rf bin
```

## Uso

### Inciso 1 — cifrar/descifrar un archivo
La clave se da como parámetro (`-k`) y se convierte a 128 bits. Entrada y salida son
archivos distintos; el archivo cifrado lleva el IV (16 bytes) al inicio.
```bash
./bin/aes_archivo -e -k 20240926 -i mensaje.txt   -o cifrado.bin      # cifrar
./bin/aes_archivo -d -k 20240926 -i cifrado.bin   -o recuperado.txt   # descifrar
```

### Búsqueda de la clave (secuencial / MPI)
```bash
# Secuencial (línea base)
./bin/busqueda_secuencial -s 123456 -b 24 -f pruebas/textos/inciso2_texto.txt -p "es una prueba de"

# Paralela con 4 procesos
mpirun -np 4 ./bin/busqueda_mpi -s 123456 -b 24 \
    -f pruebas/textos/inciso2_texto.txt -p "es una prueba de" -e ciclica
```

Opciones (ambas búsquedas):

| Flag | Significado | Defecto |
|---|---|---|
| `-s <clave>` | clave que cifra el mensaje (la que se debe encontrar) | 12345 |
| `-b <bits>`  | tamaño de la ventana de búsqueda: 2^bits candidatas (1..56) | 24 |
| `-S <inicio>`| primera candidata de la ventana (para centrar en claves grandes) | 0 |
| `-f <archivo>` | `.txt` con el texto a cifrar | texto fijo |
| `-p <frase>` | frase de búsqueda (fragmento conocido) | "es una prueba de" |
| `-e <estrategia>` | `ciclica` \| `bloques` \| `heuristica` (solo MPI; seq acepta ciclica/heuristica) | ciclica |
| `-t <seg>` | umbral para pasar de heurística a bloques | 10 |

### Reproducir toda la evidencia
```bash
cd pruebas && bash run_all.sh         # compila, corre incisos 1–3 y genera tablas
# Ajustes: REPS=7 VENTANA_BITS=22 NP=4 bash run_all.sh
```
Las tablas quedan en `pruebas/resultados/tablas.md` y los CSV en el mismo directorio.

## Estrategias de distribución

**Ciclica (base, del Laboratorio 03).** Las claves consecutivas se reparten uno-a-uno entre
los procesos: el proceso `p` prueba `start + (i·size + p)`. Todos avanzan juntos por el inicio
del rango, así que es ideal para claves pequeñas. Para una clave muy grande, el proceso que la
posee debe recorrer ~`clave/size` candidatas: inviable sobre 2^56.

**Bloques contiguos.** La ventana se parte en `size` bloques iguales; el proceso `p` recorre
`[start + p·(2^bits/size), …)`. Con el espacio completo [0, 2^56) y 4 procesos, el bloque del
proceso 1 empieza exactamente en 2^56/4, de modo que esa clave se encuentra en la **primera**
prueba y 2^56/4+1 en la **segunda**. A cambio, una clave pequeña la busca solo el proceso 0
(≈4× más lento que la cíclica).

**Heurística + bloques (estrategia alternativa propuesta).**
- *Fase 1 (diccionario):* los procesos prueben, repartido cíclicamente, un conjunto de claves
  "probables" — números cortos, potencias de dos y sus vecinas, fechas, secuencias. Las tres
  claves del enunciado caen aquí (123456 es secuencia corta; 2^56/4 es potencia de dos;
  2^56/4+1 es su vecina), así que se hallan casi al instante.
- *Fase 2 (fallback):* si la clave no aparece o se supera el umbral de tiempo `-t`, se continúa
  con la distribución por bloques sobre la ventana. La salida indica el motivo: `Fase: bloques
  (fallback por umbral de tiempo)` o `Fase: bloques (fallback, diccionario agotado)`.

Coordinación (las tres): tras cada lote, `MPI_Allreduce(MPI_MIN)` combina el menor hallazgo;
si alguno encontró la clave, todos se detienen.

### Pseudocódigo de la estrategia alternativa (inciso 4)
```
cada proceso p de size:
  # Fase 1: diccionario de claves probables (rondas iguales en todos los procesos)
  rondas = ceil(ceil(|dicc| / size) / BATCH)
  para r = 0 .. rondas-1:
      para i en la tanda r (hasta BATCH entradas): j = i·size + p
          si descifra(dicc[j]) contiene frase: local = dicc[j]; cortar tanda
      global = Allreduce_MIN(local)          # todos, una vez por ronda
      si global encontrada: terminar (fase "heuristica")
      parar = (rank 0 y tiempo > umbral)
      Bcast(parar)                            # todos, una vez por ronda
      si parar: salir de fase 1 (fallback por umbral)
  # (si se agotan las rondas sin hallazgo: fallback, diccionario agotado)
  # Fase 2: bloques contiguos sobre la ventana
  base = start + p·(2^bits / size)
  para key = base .. base + (2^bits/size):
      si descifra(key) contiene frase: local = key
      cada BATCH pruebas: global = Allreduce_MIN(local); si global: terminar
```

## Nota sobre las claves gigantes
2^56/4 = 18 014 398 509 481 984. A ~4 M candidatas/s, la búsqueda cíclica tardaría ~décadas, así
que **no es alcanzable por fuerza bruta real**. Por eso:
- **bloques** y **heurística** operan sobre el espacio completo 2^56 y la encuentran de inmediato;
- **cíclica**, para dar un tiempo medible, se corre sobre una **ventana centrada** de 2^22
  candidatas alrededor del objetivo (concesión documentada; en la práctica equivale a "ya saber
  dónde buscar", que es justo lo que la cíclica no resuelve por sí sola).

## Catálogo de funciones (resumen para el Anexo 1)

`src/aes_comun.h`
- `read_text_file(path,len)` — lee un `.txt` a memoria (sin saltos finales).
- `parse_config / free_config` — lee y valida flags (getopt); reserva/libera el buffer de texto.
- `make_key(candidate,key)` — construye la clave AES de 16 bytes desde un entero (→128 bits).
- `fetch_aes_cbc / new_decrypt_ctx` — obtienen el algoritmo y un contexto de descifrado reusable.
- `encrypt_message(aes,secret,text,msg)` — cifra con AES-128-CBC, IV aleatorio, PKCS#7.
- `try_candidate(ctx,cand,msg,frase,plain,len)` — prueba una candidata con filtro por el último
  bloque (descarta ~255/256 claves sin descifrar todo) y verifica la frase.
- `contains / get_time` — búsqueda de subcadena en bytes / reloj monótono.

`src/diccionario.h`
- `construir_diccionario(n)` — genera la lista de claves probables (fase heurística).

`src/aes_archivo.c`
- `cifrar / descifrar` — cifran/descifran un buffer completo a/desde archivo (IV + texto cifrado).

`src/busqueda_mpi.c`
- `buscar_ciclica / buscar_bloques / buscar_heuristica` — una por estrategia.
- `sincronizar(local)` — `MPI_Allreduce(MPI_MIN)` del hallazgo de la ronda.

## Hardware de referencia de las mediciones incluidas
Intel Xeon @ 2.10 GHz, 2 núcleos con AES-NI, Open MPI 4.1.6, OpenSSL 3. Con solo 2 núcleos
físicos y `-np 4` hay sobre-suscripción, por lo que el speedup se satura cerca de 2×; en una
máquina con 4+ núcleos físicos el speedup con 4 procesos será mayor. Vuelve a correr
`run_all.sh` en tu equipo para obtener las cifras de tu máquina.

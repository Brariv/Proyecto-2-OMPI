/*----------------------------------------------------------------------
 * UNIVERSIDAD DEL VALLE DE GUATEMALA
 * Facultad de Ingenieria - Departamento de Ciencias de la Computacion
 * Curso:       CC3069 - Computacion Paralela y Distribuida
 * Proyecto 02 - Nucleo comun para la busqueda de una clave AES-128
 *
 * Reune todo lo que comparten la version secuencial, la version MPI y
 * la herramienta de cifrado/descifrado de archivos, para que las tres
 * construyan la clave, cifren y verifiquen las candidatas exactamente
 * de la misma forma.
 *
 * Contenido:
 *   - Configuracion por linea de comandos (getopt): clave, ventana de
 *     busqueda, archivo de texto, frase de busqueda, estrategia y
 *     umbral de la estrategia alternativa.
 *   - Construccion de la clave AES de 128 bits a partir de un entero.
 *   - Cifrado AES-128-CBC con IV aleatorio y relleno PKCS#7.
 *   - Verificacion rapida de candidatas (filtro por el ultimo bloque).
 *   - Lectura de un texto desde un archivo .txt.
 *----------------------------------------------------------------------*/

#ifndef AES_COMUN_H
#define AES_COMUN_H

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <getopt.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#define AES_BLOCK        16
#define AES_KEY_BYTES    16          /* AES-128: clave de 128 bits (16 bytes) */
#define MAX_TEXT_LEN     4096        /* texto maximo a cifrar en la busqueda  */
#define MAX_CIPHER_LEN   (MAX_TEXT_LEN + AES_BLOCK)

/* Identificadores de la estrategia de distribucion (version MPI). */
#define EST_CICLICA      0           /* estrategia base del Laboratorio 03   */
#define EST_BLOQUES      1           /* bloques contiguos por proceso        */
#define EST_HEURISTICA   2           /* heuristica y luego bloques (umbral)  */

/* Valores por defecto del ejercicio. */
#define DEFAULT_SECRET    UINT64_C(12345)
#define DEFAULT_BITS      24
#define DEFAULT_TEXT      "Esta es una prueba de proyecto 2"
#define DEFAULT_KEYWORD   "es una prueba de"
#define DEFAULT_THRESHOLD 10.0       /* segundos antes de pasar a bloques    */

/*
 * Configuracion de una ejecucion de busqueda.
 *
 * La busqueda recorre la ventana  [start, start + 2^bits).  El parametro
 * start permite centrar la ventana alrededor de una clave conocida muy
 * grande (p. ej. 2^54), de modo que la estrategia ciclica pueda medirse
 * en un tiempo razonable; con start = 0 la ventana es el prefijo normal
 * del espacio de claves.
 */
typedef struct {
    uint64_t secret;        /* clave real que cifra el mensaje (a encontrar) */
    int      bits;          /* se exploran 2^bits candidatas                 */
    uint64_t total_keys;    /* = 2^bits                                      */
    uint64_t start;         /* primera candidata de la ventana               */
    const char *text;       /* texto original a cifrar                       */
    char    *text_buf;      /* buffer propio si el texto vino de un archivo  */
    const char *keyword;    /* fragmento conocido para verificar             */
    int      strategy;      /* EST_CICLICA | EST_BLOQUES | EST_HEURISTICA     */
    double   threshold;     /* seg. antes del fallback a bloques             */
} config_t;

/* Mensaje cifrado: IV publico + texto cifrado (CBC + PKCS#7). */
typedef struct {
    unsigned char iv[AES_BLOCK];
    unsigned char data[MAX_CIPHER_LEN];
    int len;                /* multiplo de 16 */
} cipher_msg_t;

/* Muestra el error de OpenSSL y termina el programa. */
static inline void fail(const char *description)
{
    fprintf(stderr, "%s\n", description);
    ERR_print_errors_fp(stderr);
    exit(EXIT_FAILURE);
}

/*----------------------------------------------------------------------
 * Lectura de archivos
 *---------------------------------------------------------------------*/

/*
 * Lee todo el contenido de un archivo de texto y lo devuelve en un
 * buffer terminado en '\0' reservado con malloc (el llamador lo libera).
 * Devuelve NULL si el archivo no se puede abrir o esta vacio.
 */
static inline char *read_text_file(const char *path, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "No se pudo abrir el archivo: %s\n", path);
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    rewind(f);

    char *buf = (char *)malloc((size_t)sz + 1);
    if (buf == NULL) { fclose(f); fail("Sin memoria al leer el archivo."); }

    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';

    /* Se descartan saltos de linea finales para que la frase de busqueda
     * se verifique sin sorpresas por un '\n' al final del archivo. */
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
        buf[--n] = '\0';
    }
    if (n == 0) { free(buf); fprintf(stderr, "El archivo esta vacio: %s\n", path); return NULL; }
    if (out_len) *out_len = n;
    return buf;
}

/*----------------------------------------------------------------------
 * Configuracion por linea de comandos (getopt)
 *---------------------------------------------------------------------*/

static inline void usage(const char *prog)
{
    fprintf(stderr,
"Uso: %s [opciones]\n"
"  -s <clave>     Clave secreta que cifra el mensaje (se convierte a 128 bits).\n"
"                 Es la clave que la busqueda debe encontrar.   (defecto %" PRIu64 ")\n"
"  -b <bits>      Tamano de la ventana de busqueda: 2^bits candidatas, 1..56.\n"
"                                                                (defecto %d)\n"
"  -S <inicio>    Primera candidata de la ventana (para centrar).(defecto 0)\n"
"  -f <archivo>   Archivo .txt con el texto a cifrar.            (defecto texto fijo)\n"
"  -p <frase>     Frase de busqueda (fragmento conocido).        (defecto \"%s\")\n"
"  -e <estrategia> ciclica | bloques | heuristica.              (defecto ciclica)\n"
"  -t <seg>       Umbral en segundos para pasar de heuristica a bloques.\n"
"                                                                (defecto %.1f)\n",
        prog, DEFAULT_SECRET, DEFAULT_BITS, DEFAULT_KEYWORD, DEFAULT_THRESHOLD);
}

/* Convierte una cadena a entero sin signo validando errores. */
static inline int parse_u64(const char *s, uint64_t *out)
{
    char *end = NULL;
    errno = 0;
    unsigned long long v = strtoull(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || s[0] == '-') return 0;
    *out = (uint64_t)v;
    return 1;
}

/* Traduce el nombre de la estrategia a su identificador. */
static inline int parse_strategy(const char *s, int *out)
{
    if (strcmp(s, "ciclica")    == 0) { *out = EST_CICLICA;    return 1; }
    if (strcmp(s, "bloques")    == 0) { *out = EST_BLOQUES;    return 1; }
    if (strcmp(s, "heuristica") == 0) { *out = EST_HEURISTICA; return 1; }
    return 0;
}

static inline const char *strategy_name(int s)
{
    switch (s) {
        case EST_CICLICA:    return "ciclica (base)";
        case EST_BLOQUES:    return "bloques contiguos";
        case EST_HEURISTICA: return "heuristica + bloques";
        default:             return "desconocida";
    }
}

/*
 * Lee y valida los argumentos. Devuelve 1 si son validos. Si quiet != 0
 * no imprime mensajes (util para los procesos MPI con rank != 0).
 *
 * Al leer un texto desde archivo reserva cfg->text_buf; conviene llamar
 * a free_config() al terminar.
 */
static inline int parse_config(int argc, char *argv[], config_t *cfg, int quiet)
{
    uint64_t bits = DEFAULT_BITS;
    const char *file = NULL;
    int opt;

    cfg->secret    = DEFAULT_SECRET;
    cfg->start     = 0;
    cfg->text      = DEFAULT_TEXT;
    cfg->text_buf  = NULL;
    cfg->keyword   = DEFAULT_KEYWORD;
    cfg->strategy  = EST_CICLICA;
    cfg->threshold = DEFAULT_THRESHOLD;

    optind = 1;                     /* permite re-parsear en la misma ejecucion */
    opterr = 0;
    while ((opt = getopt(argc, argv, "s:b:S:f:p:e:t:h")) != -1) {
        switch (opt) {
        case 's':
            if (!parse_u64(optarg, &cfg->secret)) {
                if (!quiet) fprintf(stderr, "Clave invalida: %s\n", optarg);
                return 0;
            }
            break;
        case 'b':
            if (!parse_u64(optarg, &bits)) {
                if (!quiet) fprintf(stderr, "bits invalido: %s\n", optarg);
                return 0;
            }
            break;
        case 'S':
            if (!parse_u64(optarg, &cfg->start)) {
                if (!quiet) fprintf(stderr, "inicio invalido: %s\n", optarg);
                return 0;
            }
            break;
        case 'f': file = optarg; break;
        case 'p': cfg->keyword = optarg; break;
        case 'e':
            if (!parse_strategy(optarg, &cfg->strategy)) {
                if (!quiet) fprintf(stderr, "Estrategia invalida: %s\n", optarg);
                return 0;
            }
            break;
        case 't':
            cfg->threshold = strtod(optarg, NULL);
            if (cfg->threshold <= 0.0) cfg->threshold = DEFAULT_THRESHOLD;
            break;
        case 'h':
        default:
            if (!quiet) usage(argv[0]);
            return 0;
        }
    }

    if (bits < 1 || bits > 56) {
        if (!quiet) fprintf(stderr, "bits debe estar entre 1 y 56.\n");
        return 0;
    }
    cfg->bits = (int)bits;
    cfg->total_keys = UINT64_C(1) << bits;

    /* Texto desde archivo, si se indico uno. */
    if (file != NULL) {
        size_t tlen = 0;
        cfg->text_buf = read_text_file(file, &tlen);
        if (cfg->text_buf == NULL) return 0;
        if (tlen > MAX_TEXT_LEN) {
            if (!quiet) fprintf(stderr, "El texto del archivo supera %d bytes.\n", MAX_TEXT_LEN);
            free(cfg->text_buf); cfg->text_buf = NULL;
            return 0;
        }
        cfg->text = cfg->text_buf;
    }

    /* Validacion de texto y frase de busqueda. */
    size_t tlen = strlen(cfg->text), klen = strlen(cfg->keyword);
    if (tlen == 0 || tlen > MAX_TEXT_LEN) {
        if (!quiet) fprintf(stderr, "El texto debe tener entre 1 y %d bytes.\n", MAX_TEXT_LEN);
        return 0;
    }
    if (klen == 0 || klen > tlen || strstr(cfg->text, cfg->keyword) == NULL) {
        if (!quiet) fprintf(stderr, "La frase de busqueda debe ser un fragmento del texto.\n");
        return 0;
    }
    return 1;
}

/* Libera la memoria reservada por parse_config (si la hubo). */
static inline void free_config(config_t *cfg)
{
    if (cfg->text_buf != NULL) { free(cfg->text_buf); cfg->text_buf = NULL; }
}

/*----------------------------------------------------------------------
 * Construccion de la clave y cifrado
 *---------------------------------------------------------------------*/

/*
 * Construye la clave AES de 16 bytes a partir de un entero candidato.
 * Los 8 bytes bajos contienen el entero en big-endian y los 8 altos
 * quedan en cero. Asi, el valor 123456 o 2^54 se "convierte a 128 bits"
 * tal como pide el enunciado.
 */
static inline void make_key(uint64_t candidate, unsigned char key[AES_KEY_BYTES])
{
    memset(key, 0, AES_KEY_BYTES);
    for (int i = 0; i < 8; i++) {
        key[15 - i] = (unsigned char)((candidate >> (8 * i)) & 0xFF);
    }
}

/* El algoritmo AES-128-CBC se obtiene una sola vez (OpenSSL 3). */
static inline EVP_CIPHER *fetch_aes_cbc(void)
{
    EVP_CIPHER *c = EVP_CIPHER_fetch(NULL, "AES-128-CBC", NULL);
    if (c == NULL) fail("No se pudo obtener AES-128-CBC.");
    return c;
}

/*
 * Crea un contexto de descifrado ya asociado al algoritmo. En cada
 * candidata solo se cambian la clave y el IV (cipher = NULL), sin volver
 * a buscar ni reinicializar el algoritmo completo.
 */
static inline EVP_CIPHER_CTX *new_decrypt_ctx(const EVP_CIPHER *aes)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) fail("No se pudo crear el contexto de OpenSSL.");
    if (EVP_DecryptInit_ex(ctx, aes, NULL, NULL, NULL) != 1) {
        fail("Error al inicializar AES.");
    }
    return ctx;
}

/*
 * Cifra el texto con AES-128-CBC, IV aleatorio y relleno PKCS#7.
 * Deja el IV y el texto cifrado en 'msg'.
 */
static inline void encrypt_message(const EVP_CIPHER *aes, uint64_t secret,
                                   const char *text, cipher_msg_t *msg)
{
    unsigned char key[AES_KEY_BYTES];
    int n1 = 0, n2 = 0;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

    if (ctx == NULL) fail("No se pudo crear el contexto de OpenSSL.");
    if (RAND_bytes(msg->iv, AES_BLOCK) != 1) fail("No se pudo generar el IV.");

    make_key(secret, key);
    if (EVP_EncryptInit_ex(ctx, aes, NULL, key, msg->iv) != 1 ||
        EVP_EncryptUpdate(ctx, msg->data, &n1,
                          (const unsigned char *)text, (int)strlen(text)) != 1 ||
        EVP_EncryptFinal_ex(ctx, msg->data + n1, &n2) != 1) {
        fail("Error al cifrar el mensaje.");
    }
    msg->len = n1 + n2;
    EVP_CIPHER_CTX_free(ctx);
}

/* Busca un fragmento dentro de un bloque de bytes (no requiere '\0'). */
static inline int contains(const unsigned char *hay, int hlen, const char *needle)
{
    int nlen = (int)strlen(needle);
    for (int i = 0; i + nlen <= hlen; i++) {
        if (memcmp(hay + i, needle, (size_t)nlen) == 0) return 1;
    }
    return 0;
}

/*
 * Prueba una candidata. Devuelve 1 si descifra el mensaje y el resultado
 * contiene la frase de busqueda; en ese caso deja el texto descifrado en
 * 'plain' y su longitud en 'plain_len'.
 *
 * Rendimiento: primero se descifra solo el ultimo bloque (en CBC basta
 * usar el bloque anterior como IV). Si el relleno PKCS#7 no es valido, la
 * candidata se descarta sin descifrar todo el mensaje, lo que elimina
 * ~255 de cada 256 claves incorrectas. El descifrado completo solo ocurre
 * para las pocas candidatas que pasan ese filtro.
 */
static inline int try_candidate(EVP_CIPHER_CTX *ctx,
                                uint64_t candidate, const cipher_msg_t *msg,
                                const char *keyword,
                                unsigned char *plain, int *plain_len)
{
    unsigned char key[AES_KEY_BYTES];
    unsigned char tmp[2 * AES_BLOCK];
    int n1 = 0, n2 = 0;

    make_key(candidate, key);

    /* Filtro rapido: ultimo bloque + verificacion del relleno. */
    const unsigned char *last_iv = (msg->len == AES_BLOCK)
        ? msg->iv : msg->data + msg->len - 2 * AES_BLOCK;

    if (EVP_DecryptInit_ex(ctx, NULL, NULL, key, last_iv) != 1) fail("Error al inicializar AES.");
    if (EVP_DecryptUpdate(ctx, tmp, &n1, msg->data + msg->len - AES_BLOCK, AES_BLOCK) != 1) {
        fail("Error al descifrar el bloque.");
    }
    if (EVP_DecryptFinal_ex(ctx, tmp + n1, &n2) != 1) {
        ERR_clear_error();          /* relleno invalido: clave incorrecta */
        return 0;
    }

    /* Descifrado completo solo para las candidatas que pasan el filtro. */
    if (EVP_DecryptInit_ex(ctx, NULL, NULL, key, msg->iv) != 1) fail("Error al inicializar AES.");
    if (EVP_DecryptUpdate(ctx, plain, &n1, msg->data, msg->len) != 1) {
        fail("Error al descifrar el mensaje.");
    }
    if (EVP_DecryptFinal_ex(ctx, plain + n1, &n2) != 1) {
        ERR_clear_error();
        return 0;
    }
    *plain_len = n1 + n2;
    return contains(plain, *plain_len, keyword);
}

/* Reloj monotono, en segundos. */
static inline double get_time(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) {
        perror("Error al consultar el reloj");
        exit(EXIT_FAILURE);
    }
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

#endif /* AES_COMUN_H */

/*----------------------------------------------------------------------
 * UNIVERSIDAD DEL VALLE DE GUATEMALA
 * Curso:  CC3069 - Computacion Paralela y Distribuida
 * Proyecto 02 - Inciso 1: cifrado y descifrado de un archivo con AES-128
 *
 * Carga un texto desde un archivo, lo cifra con AES-128-CBC usando una
 * clave entregada como parametro (convertida a 128 bits) y escribe el
 * resultado en otro archivo. Con la misma clave realiza la operacion
 * inversa. El archivo de entrada y el de salida son distintos en ambos
 * casos, de modo que se conservan el original, el cifrado y el
 * recuperado.
 *
 * Formato del archivo cifrado:  [ IV (16 bytes) | texto cifrado (CBC) ].
 * El IV es publico y aleatorio; se guarda al inicio para poder descifrar.
 *
 * Compilacion:
 *   gcc -std=c11 -O2 -Wall -Wextra aes_archivo.c -o aes_archivo -lcrypto
 * Ejecucion:
 *   ./aes_archivo -e -k <clave> -i entrada.txt -o cifrado.bin
 *   ./aes_archivo -d -k <clave> -i cifrado.bin -o recuperado.txt
 *----------------------------------------------------------------------*/

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#define AES_BLOCK     16
#define AES_KEY_BYTES 16

/* Muestra el error y termina. */
static void fail(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    ERR_print_errors_fp(stderr);
    exit(EXIT_FAILURE);
}

/* Construye la clave AES de 16 bytes a partir del entero (igual que la
 * busqueda: el enunciado pide "convertir la clave a 128 bits"). */
static void make_key(uint64_t candidate, unsigned char key[AES_KEY_BYTES])
{
    memset(key, 0, AES_KEY_BYTES);
    for (int i = 0; i < 8; i++) {
        key[15 - i] = (unsigned char)((candidate >> (8 * i)) & 0xFF);
    }
}

/* Lee un archivo completo a memoria. Devuelve el buffer y su tamano. */
static unsigned char *read_all(const char *path, long *out_len)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) { fprintf(stderr, "No se pudo abrir: %s\n", path); exit(EXIT_FAILURE); }
    if (fseek(f, 0, SEEK_END) != 0) fail("Error al posicionar en el archivo.");
    long sz = ftell(f);
    if (sz < 0) fail("Error al medir el archivo.");
    rewind(f);

    unsigned char *buf = (unsigned char *)malloc((size_t)sz + 1);
    if (buf == NULL) fail("Sin memoria.");
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    *out_len = (long)n;
    return buf;
}

/* Escribe un bloque de bytes a un archivo. */
static void write_all(const char *path, const unsigned char *buf, long len)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) { fprintf(stderr, "No se pudo crear: %s\n", path); exit(EXIT_FAILURE); }
    if (fwrite(buf, 1, (size_t)len, f) != (size_t)len) fail("Error al escribir el archivo.");
    fclose(f);
}

/*
 * Cifra 'in' (len bytes) con AES-128-CBC. Escribe en 'out' el IV seguido
 * del texto cifrado y devuelve la longitud total escrita.
 */
static long cifrar(uint64_t clave, const unsigned char *in, long len, unsigned char *out)
{
    unsigned char key[AES_KEY_BYTES];
    int n1 = 0, n2 = 0;

    make_key(clave, key);
    if (RAND_bytes(out, AES_BLOCK) != 1) fail("No se pudo generar el IV.");

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) fail("No se pudo crear el contexto.");
    if (EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, out) != 1 ||
        EVP_EncryptUpdate(ctx, out + AES_BLOCK, &n1, in, (int)len) != 1 ||
        EVP_EncryptFinal_ex(ctx, out + AES_BLOCK + n1, &n2) != 1) {
        fail("Error al cifrar.");
    }
    EVP_CIPHER_CTX_free(ctx);
    return AES_BLOCK + n1 + n2;
}

/*
 * Descifra 'in' (IV + texto cifrado, len bytes) con AES-128-CBC. Escribe
 * el texto claro en 'out' y devuelve su longitud, o -1 si el relleno es
 * invalido (clave incorrecta o archivo corrupto).
 */
static long descifrar(uint64_t clave, const unsigned char *in, long len, unsigned char *out)
{
    unsigned char key[AES_KEY_BYTES];
    int n1 = 0, n2 = 0;

    if (len <= AES_BLOCK || (len - AES_BLOCK) % AES_BLOCK != 0) {
        fprintf(stderr, "Archivo cifrado con longitud invalida.\n");
        return -1;
    }
    make_key(clave, key);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) fail("No se pudo crear el contexto.");
    if (EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, in) != 1) {
        fail("Error al inicializar el descifrado.");
    }
    if (EVP_DecryptUpdate(ctx, out, &n1, in + AES_BLOCK, (int)(len - AES_BLOCK)) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    if (EVP_DecryptFinal_ex(ctx, out + n1, &n2) != 1) {
        ERR_clear_error();
        EVP_CIPHER_CTX_free(ctx);
        return -1;              /* relleno invalido: clave equivocada */
    }
    EVP_CIPHER_CTX_free(ctx);
    return n1 + n2;
}

static void uso(const char *prog)
{
    fprintf(stderr,
        "Uso:\n"
        "  %s -e -k <clave> -i <entrada.txt> -o <cifrado.bin>   (cifrar)\n"
        "  %s -d -k <clave> -i <cifrado.bin> -o <recuperado.txt> (descifrar)\n"
        "    -k  clave numerica, se convierte a 128 bits\n",
        prog, prog);
}

int main(int argc, char *argv[])
{
    int modo_cifrar = -1;            /* 1 cifrar, 0 descifrar */
    uint64_t clave = 0;
    int tiene_clave = 0;
    const char *entrada = NULL, *salida = NULL;
    int opt;

    while ((opt = getopt(argc, argv, "edk:i:o:h")) != -1) {
        switch (opt) {
        case 'e': modo_cifrar = 1; break;
        case 'd': modo_cifrar = 0; break;
        case 'k': {
            char *end = NULL; errno = 0;
            clave = strtoull(optarg, &end, 10);
            if (errno != 0 || *end != '\0' || optarg[0] == '-') {
                fprintf(stderr, "Clave invalida: %s\n", optarg); return EXIT_FAILURE;
            }
            tiene_clave = 1; break;
        }
        case 'i': entrada = optarg; break;
        case 'o': salida  = optarg; break;
        case 'h': default: uso(argv[0]); return EXIT_FAILURE;
        }
    }

    if (modo_cifrar == -1 || !tiene_clave || entrada == NULL || salida == NULL) {
        uso(argv[0]);
        return EXIT_FAILURE;
    }

    long in_len = 0;
    unsigned char *in = read_all(entrada, &in_len);
    if (modo_cifrar == 1 && in_len == 0) {
        fprintf(stderr, "El archivo de entrada esta vacio.\n");
        free(in); return EXIT_FAILURE;
    }

    /* El resultado nunca excede la entrada mas un bloque de relleno y el IV. */
    unsigned char *out = (unsigned char *)malloc((size_t)in_len + 2 * AES_BLOCK);
    if (out == NULL) fail("Sin memoria.");

    if (modo_cifrar == 1) {
        long out_len = cifrar(clave, in, in_len, out);
        write_all(salida, out, out_len);
        printf("Cifrado AES-128-CBC completado.\n");
        printf("  Clave (128 bits) a partir de : %" PRIu64 "\n", clave);
        printf("  Entrada  : %s (%ld bytes)\n", entrada, in_len);
        printf("  Salida   : %s (%ld bytes, incluye IV de 16 bytes)\n", salida, out_len);
    } else {
        long out_len = descifrar(clave, in, in_len, out);
        if (out_len < 0) {
            fprintf(stderr, "No se pudo descifrar: clave incorrecta o archivo corrupto.\n");
            free(in); free(out); return EXIT_FAILURE;
        }
        write_all(salida, out, out_len);
        printf("Descifrado AES-128-CBC completado.\n");
        printf("  Clave (128 bits) a partir de : %" PRIu64 "\n", clave);
        printf("  Entrada  : %s (%ld bytes)\n", entrada, in_len);
        printf("  Salida   : %s (%ld bytes)\n", salida, out_len);
    }

    free(in); free(out);
    return EXIT_SUCCESS;
}

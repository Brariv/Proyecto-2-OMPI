/*----------------------------------------------------------------------
 * UNIVERSIDAD DEL VALLE DE GUATEMALA
 * Curso:  CC3069 - Computacion Paralela y Distribuida
 * Proyecto 02 - Busqueda secuencial de la clave AES (linea base)
 *
 * Un unico proceso prueba las claves de la ventana [start, start+2^bits)
 * en orden y se detiene en la primera candidata valida. Sirve como linea
 * base para calcular el speedup de las versiones paralelas.
 *
 * Si se elige la estrategia "heuristica", primero prueba el diccionario
 * de claves probables y, si no la encuentra, continua con el barrido
 * lineal de la ventana.
 *
 * Compilacion:
 *   gcc -std=c11 -O2 -Wall -Wextra busqueda_secuencial.c \
 *       -o busqueda_secuencial -lcrypto
 * Ejecucion:
 *   ./busqueda_secuencial -s <clave> -b <bits> [-S <inicio>] \
 *       [-f texto.txt] [-p "frase"] [-e ciclica|heuristica]
 *----------------------------------------------------------------------*/

#include "aes_comun.h"
#include "diccionario.h"

int main(int argc, char *argv[])
{
    double t_total = get_time();
    config_t cfg;

    if (!parse_config(argc, argv, &cfg, 0)) return EXIT_FAILURE;

    EVP_CIPHER *aes = fetch_aes_cbc();
    EVP_CIPHER_CTX *ctx = new_decrypt_ctx(aes);

    /* Preparacion del ejercicio: se cifra el mensaje con la clave secreta. */
    cipher_msg_t msg;
    encrypt_message(aes, cfg.secret, cfg.text, &msg);

    unsigned char plain[MAX_CIPHER_LEN];
    int plain_len = 0;
    uint64_t found = UINT64_MAX;
    uint64_t tested = 0;
    const char *fase = "barrido lineal";

    double start = get_time();

    /* Fase heuristica opcional: diccionario de claves probables. */
    if (cfg.strategy == EST_HEURISTICA) {
        size_t dn = 0;
        uint64_t *dicc = construir_diccionario(&dn);
        for (size_t j = 0; j < dn && found == UINT64_MAX; j++) {
            tested++;
            if (try_candidate(ctx, dicc[j], &msg, cfg.keyword, plain, &plain_len)) {
                found = dicc[j];
                fase = "heuristica (diccionario)";
            }
        }
        free(dicc);
    }

    /* Barrido lineal de la ventana [start, start + total_keys). */
    for (uint64_t off = 0; off < cfg.total_keys && found == UINT64_MAX; off++) {
        uint64_t key = cfg.start + off;
        tested++;
        if (try_candidate(ctx, key, &msg, cfg.keyword, plain, &plain_len)) {
            found = key;
        }
    }

    double elapsed = get_time() - start;

    if (found != UINT64_MAX) {
        printf("Clave encontrada: %" PRIu64 "\n", found);
        printf("Mensaje: %.*s\n", plain_len, (const char *)plain);
        printf("Fase: %s\n", fase);
    } else {
        printf("No se encontro la clave en la ventana [%" PRIu64 ", %" PRIu64 ").\n",
               cfg.start, cfg.start + cfg.total_keys);
    }
    printf("Ejecucion: secuencial (%s)\n", strategy_name(cfg.strategy));
    printf("Ventana explorada: 2^%d = %" PRIu64 " candidatas desde %" PRIu64 "\n",
           cfg.bits, cfg.total_keys, cfg.start);
    printf("Candidatas probadas: %" PRIu64 "\n", tested);
    printf("Tiempo de busqueda: %.6f segundos\n", elapsed);
    printf("Tiempo total: %.6f segundos\n", get_time() - t_total);

    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(aes);
    free_config(&cfg);
    return EXIT_SUCCESS;
}

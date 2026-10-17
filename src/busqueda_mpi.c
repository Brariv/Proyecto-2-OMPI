/*----------------------------------------------------------------------
 * UNIVERSIDAD DEL VALLE DE GUATEMALA
 * Curso:  CC3069 - Computacion Paralela y Distribuida
 * Proyecto 02 - Busqueda paralela de una clave AES con Open MPI
 *
 * Tres estrategias de distribucion seleccionables con -e:
 *
 *   ciclica  (base, del Laboratorio 03)
 *       En la ronda r el proceso p prueba las candidatas
 *           start + (r*BATCH + i)*size + p,   i = 0..BATCH-1.
 *       El trabajo queda entrelazado: las claves consecutivas se reparten
 *       uno-a-uno entre los procesos. Es excelente para claves pequenas
 *       (todos avanzan por el inicio del rango a la vez).
 *
 *   bloques
 *       La ventana se parte en 'size' bloques contiguos iguales y cada
 *       proceso recorre el suyo en orden:
 *           proceso p -> [start + p*per_proc, start + (p+1)*per_proc).
 *       Una clave que cae al inicio del bloque de un proceso se encuentra
 *       de inmediato (p. ej. 2^56/4 es la primera clave del proceso 1),
 *       pero una clave pequena la busca solo el proceso 0.
 *
 *   heuristica
 *       Fase 1: todos los procesos prueban, repartido ciclicamente, un
 *       diccionario de claves "probables" (numeros cortos, potencias de
 *       dos y vecinas, fechas...). Fase 2: si la clave no aparece o se
 *       supera el umbral de tiempo (-t), se continua con la estrategia
 *       por bloques sobre la ventana. Muy rapida para claves elegidas por
 *       personas; sin ventaja frente a claves aleatorias.
 *
 * Coordinacion (las tres estrategias): al terminar cada tanda los
 * procesos combinan su hallazgo con MPI_Allreduce(MPI_MIN); si alguno
 * encontro la clave, todos se detienen.
 *
 * Compilacion:
 *   mpicc -std=c11 -O2 -Wall -Wextra busqueda_mpi.c -o busqueda_mpi -lcrypto
 * Ejecucion:
 *   mpirun -np 4 ./busqueda_mpi -s <clave> -b <bits> [-S <inicio>] \
 *       [-f texto.txt] [-p "frase"] [-e ciclica|bloques|heuristica] [-t seg]
 *----------------------------------------------------------------------*/

#include "aes_comun.h"
#include "diccionario.h"
#include <mpi.h>

#define BATCH UINT64_C(4096)   /* candidatas por proceso entre sincronizaciones */

/* Estado compartido por las rutinas de busqueda de cada estrategia. */
typedef struct {
    EVP_CIPHER_CTX *ctx;
    const cipher_msg_t *msg;
    const config_t *cfg;
    unsigned char *plain;
    int *plain_len;
    uint64_t *tested;
    int rank, size;
} ctx_busqueda_t;

/* Combina el hallazgo local de la ronda; devuelve el menor hallazgo global. */
static uint64_t sincronizar(uint64_t local)
{
    uint64_t global = UINT64_MAX;
    MPI_Allreduce(&local, &global, 1, MPI_UINT64_T, MPI_MIN, MPI_COMM_WORLD);
    return global;
}

/*
 * Estrategia ciclica (base). Recorre la ventana entrelazando las claves
 * entre los procesos y sincroniza cada BATCH candidatas por proceso.
 */
static uint64_t buscar_ciclica(ctx_busqueda_t *s)
{
    const config_t *cfg = s->cfg;
    uint64_t per_proc = (cfg->total_keys + (uint64_t)s->size - 1) / (uint64_t)s->size;
    uint64_t rounds   = (per_proc + BATCH - 1) / BATCH;
    uint64_t local = UINT64_MAX, global = UINT64_MAX;

    for (uint64_t r = 0; r < rounds && global == UINT64_MAX; r++) {
        for (uint64_t i = r * BATCH; i < (r + 1) * BATCH; i++) {
            uint64_t idx = i * (uint64_t)s->size + (uint64_t)s->rank;
            if (idx >= cfg->total_keys) break;
            uint64_t key = cfg->start + idx;
            (*s->tested)++;
            if (try_candidate(s->ctx, key, s->msg, cfg->keyword, s->plain, s->plain_len)) {
                local = key; break;
            }
        }
        global = sincronizar(local);
    }
    return global;
}

/*
 * Estrategia por bloques contiguos. Cada proceso recorre su bloque en
 * orden; se sincroniza cada BATCH candidatas para detenerse en cuanto
 * cualquier proceso encuentra la clave.
 */
static uint64_t buscar_bloques(ctx_busqueda_t *s)
{
    const config_t *cfg = s->cfg;
    uint64_t per_proc = (cfg->total_keys + (uint64_t)s->size - 1) / (uint64_t)s->size;
    uint64_t base = cfg->start + (uint64_t)s->rank * per_proc;
    uint64_t fin  = base + per_proc;
    uint64_t tope = cfg->start + cfg->total_keys;
    if (fin > tope) fin = tope;

    uint64_t local = UINT64_MAX, global = UINT64_MAX;
    uint64_t rounds = (per_proc + BATCH - 1) / BATCH;

    for (uint64_t r = 0; r < rounds && global == UINT64_MAX; r++) {
        uint64_t desde = base + r * BATCH;
        uint64_t hasta = desde + BATCH;
        if (hasta > fin) hasta = fin;
        for (uint64_t key = desde; key < hasta; key++) {
            (*s->tested)++;
            if (try_candidate(s->ctx, key, s->msg, cfg->keyword, s->plain, s->plain_len)) {
                local = key; break;
            }
        }
        global = sincronizar(local);
    }
    return global;
}

/*
 * Estrategia heuristica. Fase 1: diccionario repartido ciclicamente, con
 * corte por umbral de tiempo. Fase 2: bloques sobre la ventana. Escribe
 * en *fase la etiqueta de la fase que encontro la clave.
 *
 * La fase 1 avanza por rondas iguales para todos los procesos (mismo
 * numero de rondas, calculado a partir del tamano del diccionario), y en
 * cada ronda TODOS ejecutan exactamente un Allreduce y un Bcast. Asi las
 * operaciones colectivas siempre coinciden, aunque la clave aparezca a
 * mitad de una ronda o al final del diccionario.
 */
static uint64_t buscar_heuristica(ctx_busqueda_t *s, double t0, const char **fase)
{
    const config_t *cfg = s->cfg;
    uint64_t local = UINT64_MAX, global = UINT64_MAX;
    int por_umbral = 0;

    /* --- Fase 1: diccionario de claves probables --- */
    size_t dn = 0;
    uint64_t *dicc = construir_diccionario(&dn);

    uint64_t per_proc = ((uint64_t)dn + (uint64_t)s->size - 1) / (uint64_t)s->size;
    uint64_t rounds   = (per_proc + BATCH - 1) / BATCH;

    for (uint64_t r = 0; r < rounds; r++) {
        /* Tanda local: entradas j = i*size + rank del diccionario. */
        for (uint64_t i = r * BATCH; i < (r + 1) * BATCH; i++) {
            uint64_t j = i * (uint64_t)s->size + (uint64_t)s->rank;
            if (j >= (uint64_t)dn) break;
            (*s->tested)++;
            if (try_candidate(s->ctx, dicc[j], s->msg, cfg->keyword, s->plain, s->plain_len)) {
                local = dicc[j]; break;
            }
        }
        /* Fin de ronda: todos combinan su hallazgo... */
        global = sincronizar(local);
        if (global != UINT64_MAX) break;

        /* ...y rank 0 decide si se supero el umbral de tiempo. */
        int parar = (s->rank == 0 && (MPI_Wtime() - t0) > cfg->threshold) ? 1 : 0;
        MPI_Bcast(&parar, 1, MPI_INT, 0, MPI_COMM_WORLD);
        if (parar) { por_umbral = 1; break; }   /* pasar a fase 2 */
    }
    free(dicc);

    if (global != UINT64_MAX) { *fase = "heuristica (diccionario)"; return global; }

    /* --- Fase 2: bloques contiguos sobre la ventana --- */
    *fase = por_umbral ? "bloques (fallback por umbral de tiempo)"
                       : "bloques (fallback, diccionario agotado)";
    return buscar_bloques(s);
}

int main(int argc, char *argv[])
{
    int rank, size;

    MPI_Init(&argc, &argv);
    double t_total = MPI_Wtime();
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    config_t cfg;
    if (!parse_config(argc, argv, &cfg, rank != 0)) {
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    EVP_CIPHER *aes = fetch_aes_cbc();
    EVP_CIPHER_CTX *ctx = new_decrypt_ctx(aes);

    /* Solo rank 0 conoce la clave y cifra; el mensaje se difunde al resto. */
    cipher_msg_t msg;
    if (rank == 0) encrypt_message(aes, cfg.secret, cfg.text, &msg);
    MPI_Bcast(&msg, (int)sizeof(msg), MPI_BYTE, 0, MPI_COMM_WORLD);

    unsigned char plain[MAX_CIPHER_LEN];
    int plain_len = 0;
    uint64_t tested = 0;
    const char *fase = "barrido";

    ctx_busqueda_t s = {
        .ctx = ctx, .msg = &msg, .cfg = &cfg,
        .plain = plain, .plain_len = &plain_len,
        .tested = &tested, .rank = rank, .size = size
    };

    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();

    uint64_t global_found;
    switch (cfg.strategy) {
        case EST_BLOQUES:    global_found = buscar_bloques(&s);     fase = "bloques"; break;
        case EST_HEURISTICA: global_found = buscar_heuristica(&s, start, &fase); break;
        case EST_CICLICA:
        default:             global_found = buscar_ciclica(&s);     fase = "ciclica"; break;
    }

    double elapsed = MPI_Wtime() - start;

    /* Tiempo de busqueda = el del proceso mas lento; candidatas = suma. */
    double max_elapsed = 0.0;
    uint64_t total_tested = 0;
    MPI_Reduce(&elapsed, &max_elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&tested, &total_tested, 1, MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);

    /* El proceso que encontro la clave envia el mensaje descifrado a rank 0. */
    if (global_found != UINT64_MAX) {
        int owner_flag = (global_found != UINT64_MAX &&
                          try_candidate(ctx, global_found, &msg, cfg.keyword, plain, &plain_len))
                         ? rank : -1;
        int owner = -1;
        MPI_Allreduce(&owner_flag, &owner, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
        if (owner > 0) {
            if (rank == owner) {
                MPI_Send(&plain_len, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
                MPI_Send(plain, plain_len, MPI_UNSIGNED_CHAR, 0, 1, MPI_COMM_WORLD);
            } else if (rank == 0) {
                MPI_Recv(&plain_len, 1, MPI_INT, owner, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(plain, plain_len, MPI_UNSIGNED_CHAR, owner, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        }
    }

    if (rank == 0) {
        if (global_found != UINT64_MAX) {
            printf("Clave encontrada: %" PRIu64 "\n", global_found);
            printf("Mensaje: %.*s\n", plain_len, (const char *)plain);
            printf("Fase: %s\n", fase);
        } else {
            printf("No se encontro la clave en la ventana [%" PRIu64 ", %" PRIu64 ").\n",
                   cfg.start, cfg.start + cfg.total_keys);
        }
        printf("Ejecucion: paralela con %d procesos, estrategia %s\n",
               size, strategy_name(cfg.strategy));
        printf("Ventana explorada: 2^%d = %" PRIu64 " candidatas desde %" PRIu64 "\n",
               cfg.bits, cfg.total_keys, cfg.start);
        printf("Candidatas probadas (suma): %" PRIu64 "\n", total_tested);
        printf("Tiempo de busqueda: %.6f segundos\n", max_elapsed);
        printf("Tiempo total: %.6f segundos\n", MPI_Wtime() - t_total);
    }

    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(aes);
    free_config(&cfg);
    MPI_Finalize();
    return EXIT_SUCCESS;
}

/*----------------------------------------------------------------------
 * UNIVERSIDAD DEL VALLE DE GUATEMALA
 * Curso:  CC3069 - Computacion Paralela y Distribuida
 * Proyecto 02 - Diccionario de claves "probables" (fase heuristica)
 *
 * Genera una lista de claves candidatas que una persona elegiria con
 * mayor probabilidad que una clave aleatoria:
 *   - Numeros cortos y secuencias (0..N, 123456, 111111, ...).
 *   - Potencias de dos y sus vecinas: 2^k, 2^k - 1, 2^k + 1.
 *   - Fechas en formatos AAAAMMDD y DDMMAAAA.
 *   - Patrones de digitos repetidos y "redondos".
 *
 * La fase heuristica prueba primero estas claves. Es muy rapida para
 * claves elegidas por humanos (y para las tres claves de prueba del
 * enunciado, que son una secuencia corta y una potencia de dos con su
 * vecina), pero no aporta nada frente a una clave realmente aleatoria;
 * por eso, si se agota o se supera el umbral de tiempo, la busqueda pasa
 * a la estrategia por bloques.
 *----------------------------------------------------------------------*/

#ifndef DICCIONARIO_H
#define DICCIONARIO_H

#include <stdint.h>
#include <stdlib.h>

/* Cuantos numeros pequenos consecutivos entran al diccionario (cubre 123456). */
#define DICC_NUMEROS_CORTOS  UINT64_C(200000)

/* Agrega un valor al arreglo dinamico, creciendo si hace falta. */
static inline void dicc_push(uint64_t **arr, size_t *n, size_t *cap, uint64_t v)
{
    if (*n == *cap) {
        *cap = (*cap == 0) ? 1024 : *cap * 2;
        *arr = (uint64_t *)realloc(*arr, *cap * sizeof(uint64_t));
        if (*arr == NULL) { perror("realloc diccionario"); exit(EXIT_FAILURE); }
    }
    (*arr)[(*n)++] = v;
}

/*
 * Construye el diccionario de claves probables. Devuelve el arreglo (el
 * llamador lo libera con free) y escribe su tamano en *out_n.
 *
 * El orden importa: primero lo mas probable (numeros cortos), luego
 * potencias de dos y vecinas, luego fechas y patrones. Las tres claves
 * de prueba del enunciado caen en las primeras dos categorias.
 */
static inline uint64_t *construir_diccionario(size_t *out_n)
{
    uint64_t *d = NULL;
    size_t n = 0, cap = 0;

    /* 1. Numeros cortos y consecutivos: cubre 123456 y similares. */
    for (uint64_t v = 0; v < DICC_NUMEROS_CORTOS; v++) dicc_push(&d, &n, &cap, v);

    /* 2. Potencias de dos y sus vecinas: cubre 2^54 y 2^54 + 1. */
    for (int k = 0; k <= 56; k++) {
        uint64_t p = UINT64_C(1) << k;
        if (p >= 2) dicc_push(&d, &n, &cap, p - 1);
        dicc_push(&d, &n, &cap, p);
        dicc_push(&d, &n, &cap, p + 1);
    }

    /* 3. Secuencias y repeticiones tipicas. */
    static const uint64_t comunes[] = {
        1234567, 12345678, 123456789, 1234567890,
        111111, 222222, 333333, 444444, 555555,
        666666, 777777, 888888, 999999, 1111111111,
        100000, 1000000, 10000000, 100000000,
        987654, 654321, 696969, 420420, 101010, 121212
    };
    for (size_t i = 0; i < sizeof(comunes) / sizeof(comunes[0]); i++)
        dicc_push(&d, &n, &cap, comunes[i]);

    /* 4. Fechas AAAAMMDD y DDMMAAAA para anios 1950..2030. */
    for (int anio = 1950; anio <= 2030; anio++) {
        for (int mes = 1; mes <= 12; mes++) {
            for (int dia = 1; dia <= 31; dia++) {
                dicc_push(&d, &n, &cap,
                    (uint64_t)anio * 10000 + (uint64_t)mes * 100 + (uint64_t)dia);
                dicc_push(&d, &n, &cap,
                    (uint64_t)dia * 1000000 + (uint64_t)mes * 10000 + (uint64_t)anio);
            }
        }
    }

    *out_n = n;
    return d;
}

#endif /* DICCIONARIO_H */

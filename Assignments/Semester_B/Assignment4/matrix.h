#ifndef MATRIX_H
#define MATRIX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int N;        // N x N
    int *data;    // row-major: data[i*N + j]
} IMatrix;

/**
 * Allocate an N x N int matrix (uninitialized).
 * Returns {0,NULL} on failure.
 */
IMatrix imatrix_alloc(int N);

/** Free matrix memory (safe to call multiple times). */
void imatrix_free(IMatrix *M);

/**
 * Fill matrix with deterministic pseudo-random integers in [0, max_value].
 * Same seed => same matrix.
 */
void imatrix_fill_random(IMatrix *M, uint64_t seed, int max_value);

/** Get / Set */
int  imatrix_get(const IMatrix *M, int i, int j);
void imatrix_set(IMatrix *M, int i, int j, int v);

/** Print (recommended for small N). */
void imatrix_print(const IMatrix *M, const char *name);

/** Simple checksum (sum of all entries). */
long long imatrix_checksum(const IMatrix *M);

#ifdef __cplusplus
}
#endif

#endif // MATRIX_H

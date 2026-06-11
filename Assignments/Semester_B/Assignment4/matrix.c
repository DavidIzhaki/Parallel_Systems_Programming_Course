#include "matrix.h"
#include <stdlib.h>
#include <stdio.h>

static inline uint64_t splitmix64(uint64_t *x) {
    uint64_t z = (*x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

IMatrix imatrix_alloc(int N) {
    IMatrix M;
    M.N = 0;
    M.data = NULL;

    if (N <= 0) return M;

    size_t count = (size_t)N * (size_t)N;
    M.data = (int*)malloc(count * sizeof(int));
    if (!M.data) return M;

    M.N = N;
    return M;
}

void imatrix_free(IMatrix *M) {
    if (!M) return;
    free(M->data);
    M->data = NULL;
    M->N = 0;
}

void imatrix_fill_random(IMatrix *M, uint64_t seed, int max_value) {
    if (!M || !M->data || M->N <= 0) return;

    if (max_value < 0) max_value = 0;

    uint64_t state = seed;
    int N = M->N;
    int range = max_value + 1; // values 0..max_value inclusive

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            uint64_t r = splitmix64(&state);
            int v = (range == 0) ? 0 : (int)(r % (uint64_t)range);
            M->data[i * N + j] = v;
        }
    }
}

int imatrix_get(const IMatrix *M, int i, int j) {
    return M->data[i * M->N + j];
}

void imatrix_set(IMatrix *M, int i, int j, int v) {
    M->data[i * M->N + j] = v;
}

void imatrix_print(const IMatrix *M, const char *name) {
    if (!M || !M->data) return;
    int N = M->N;

    if (name) printf("%s =\n", name);
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            printf("%4d%s", M->data[i * N + j], (j + 1 < N) ? " " : "");
        }
        printf("\n");
    }
}

long long imatrix_checksum(const IMatrix *M) {
    if (!M || !M->data || M->N <= 0) return 0LL;
    size_t count = (size_t)M->N * (size_t)M->N;

    long long s = 0;
    for (size_t k = 0; k < count; k++) s += (long long)M->data[k];
    return s;
}

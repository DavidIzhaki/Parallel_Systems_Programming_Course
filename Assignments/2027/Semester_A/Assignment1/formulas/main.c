#include "formulas.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define NUM_TESTS   100
#define MAX_LENGTH  10000
#define MAX_ERROR   0.0001f
#define MIN_VAL     0.00001f
#define MAX_VAL     0.01f

int is_close(float f1, float f2) {
    return fabsf(f1 - f2) <= MAX_ERROR;
}

float formula1_test(float *x, unsigned int length) {
    float sum = 0.0f;
    float product = 1.0f;

    for (unsigned int k = 0; k < length; k++) {
        sum += sqrtf(x[k]);
        product *= (x[k] * x[k] + 1.0f);
    }

    return sqrtf(1.0f + cbrtf(sum) / product);
}

int main(void) {
    srand((unsigned int)time(NULL));

    float *x = malloc(sizeof(float) * MAX_LENGTH);
    if (!x) {
        perror("malloc failed");
        return 1;
    }

    for (int i = 0; i < NUM_TESTS; i++) {
        unsigned int length = ((rand() % MAX_LENGTH) / 4) * 4;
        if (length == 0) length = 4;  // ensure it's at least 4

        for (unsigned int k = 0; k < length; k++) {
            x[k] = ((float)rand() / RAND_MAX) * (MAX_VAL - MIN_VAL) + MIN_VAL;
        }

        float result = formula1(x, length);
        float expected = formula1_test(x, length);

        if (!is_close(result, expected)) {
            printf("❌ Test failed on iteration %d\n", i);
            printf("Expected: %f\nGot     : %f\n", expected, result);
            free(x);
            return 1;
        }

        printf("✅ Test %d passed\n", i + 1);
    }

    printf("🎉 All tests completed successfully!\n");
    free(x);
    return 0;
}

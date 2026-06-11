#include <stdio.h>

long decode_c_version(long x, long y, long z); 
long decode(long x, long y, long z);           

#define decode_c decode_c_version

int main() {
    long test_cases[5][3] = {
        {5, 10, 3},
        {-2, 7, 5},
        {0, 123456, 123456},
        {-10, -20, -5},
        {9999999, -1234567, 888888}
    };

    for (int i = 0; i < 5; i++) {
        long x = test_cases[i][0];
        long y = test_cases[i][1];
        long z = test_cases[i][2];

        long res_c = decode_c(x, y, z);
        long res_asm = decode(x, y, z);

        printf("Test %d: decode(%ld, %ld, %ld)\n", i + 1, x, y, z);
        printf("  C   : %ld\n", res_c);
        printf("  ASM : %ld\n", res_asm);
        printf("  => %s\n\n", (res_c == res_asm ? "PASS" : "FAIL"));
    }

    return 0;
}

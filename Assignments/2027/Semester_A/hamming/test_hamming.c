#include <stdio.h>
#include <string.h>

#define MAX_STR 256

int hamming_dist(char str1[MAX_STR], char str2[MAX_STR]);

void run_test(const char *s1, const char *s2, int expected, int test_num) {
    char str1[MAX_STR] = {0};
    char str2[MAX_STR] = {0};

    strncpy(str1, s1, MAX_STR - 1);
    str1[MAX_STR - 1] = '\0';

    strncpy(str2, s2, MAX_STR - 1);
    str2[MAX_STR - 1] = '\0';

    int result = hamming_dist(str1, str2);
    printf("Test %d:\n", test_num);
    printf("  str1: \"%s\"\n", str1);
    printf("  str2: \"%s\"\n", str2);
    printf("  Hamming Distance = %d (expected %d) => %s\n\n",
        result, expected, (result == expected ? "PASS ✅" : "FAIL ❌"));
}


int main() {
    run_test("hello world", "hxllo worl!", 2, 1);                  
    run_test("abcd", "abcd", 0, 2);                                
    run_test("short", "shorter", 2, 3);                             
    run_test("ABCdef123456", "abcDEF123457", 7, 4);
    run_test("ABCdef1234562212222", "abcDEF123457221", 11, 5);

    return 0;
}

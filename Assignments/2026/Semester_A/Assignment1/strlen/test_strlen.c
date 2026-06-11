#include <stddef.h>   
#include <stdio.h>    
#include "strlen_sse42.h"  

size_t strlen_c(const char *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

int main() {
    const char *tests[] = {
        "",
        "a",
        "hello",
        "1234567890123456",
        "12345678901234567",
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ",
        NULL
    };

    for (int i = 0; tests[i] != NULL; i++) {
        const char *s = tests[i];
        size_t asm_len = strlen_sse42(s);  
        size_t c_len = strlen_c(s);        

        printf("Test %d: \"%s\"\n", i + 1, s);
        printf("  ASM strlen : %zu\n", asm_len);
        printf("  C   strlen : %zu\n", c_len);
        printf("  => %s\n\n", (asm_len == c_len ? "PASS ✅" : "FAIL ❌"));
    }

    return 0;
}

#include "tester.h"

// ====================================================================
// TEST_21
// Summary: ELF: fix permissions of ELF pages
// ====================================================================

char *test_name = "TEST_21";

char *str = "You can't change a character!";
int main() {
    printf(1, "\n\n%s\n", test_name);
    str[1] = 'O';
    printf(1, "%s\n", str);
    return 0;
}

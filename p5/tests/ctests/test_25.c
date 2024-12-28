#include "tester.h"

// ====================================================================
// TEST_25
// Summary: COW+STRESS: static parent arr 50000 pages, can't fork child without COW
// ====================================================================

char *test_name = "TEST_25";

#define N_PAGES 40000
char arr[N_PAGES * PGSIZE];

int sum(char *arr, int n) {
    int s = 0;
    for (int i = 0; i < n; i++) {
        s += arr[i];
    }
    return s;
}

int main(int argc, char *argv[]) {
    printf(1, "\n\n%s\n", test_name);

    int n = N_PAGES * PGSIZE;
    printf(1, "Parent: sum = %d\n", sum(arr, n));

    int pid = fork();
    if (pid < 0) {
        printerr("fork() failed\n");
        failed();
    } else if (pid == 0) {
        printf(1, "Child: sum = %d\n", sum(arr, n));
        exit();
    } else {
        wait();
        success();
    }
}
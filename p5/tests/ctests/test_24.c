#include "tester.h"

// ====================================================================
// TEST_24
// Summary: COW: kfreeing child's page does not affect parent's page in COW
// ====================================================================

char *test_name = "TEST_24";

int sum(char *arr, int n) {
    int s = 0;
    for (int i = 0; i < n; i++) {
        s += arr[i];
    }
    return s;
}

int main(int argc, char *argv[]) {
    printf(1, "\n\n%s\n", test_name);

    int N_PAGES = 5;
    int n = N_PAGES * PGSIZE;
    char *arr = malloc(n);
    for (int i = 0; i < n; i++) {
        arr[i] = i % 100;
    }
    int parent_sum = sum(arr, n);
    printf(1, "Parent: sum = %d\n", parent_sum);

    int parent_pa = va2pa((uint)arr);
    if (parent_pa == FAILED) {
        printerr("va2pa(0x%x) failed\n", arr);
        failed();
    }
    printf(1, "INFO: Parent: arr pa = 0x%x\n", parent_pa);

    int n_child = 10;
    for (int i = 0; i < n_child; i++) {
        int pid = fork();
        if (pid < 0) {
            printerr("fork() failed\n");
            failed();
        } else if (pid == 0) {
            int child_sum = sum(arr, n);
            if (parent_sum != child_sum) {
                printerr("Parent sum %d != child sum %d\n", parent_sum, child_sum);
                failed();
            }
            printf(1, "Child: sum = %d\n", sum(arr, n));
            int child_pa = va2pa((uint)arr);
            if (child_pa == FAILED) {
                printerr("va2pa(0x%x) failed\n", arr);
                failed();
            }
            printf(1, "INFO: Child: arr pa = 0x%x\n", child_pa);
            if (parent_pa != child_pa) {
                printerr("Parent and child have different physical addresses\n");
                failed();
            }

            // modify the array
            for (int i = 0; i < n; i++) {
                arr[i] = i % 50;
            }
            printf(1, "Child: sum = %d\n", sum(arr, n));
            int new_child_pa = va2pa((uint)arr);
            if (new_child_pa == FAILED) {
                printerr("va2pa(0x%x) failed\n", arr);
                failed();
            }
            printf(1, "INFO: Child: arr new_pa = 0x%x\n", new_child_pa);
            if (parent_pa == new_child_pa) {
                printerr("Parent and child have same pa after modification\n");
                failed();
            }
            free(arr);
            int ret = (int)sbrk(-n);
            if (ret == FAILED) {
                printerr("sbrk(-%d) failed\n", n);
                failed();
            }
            printf(1, "INFO: Child sz 0x%x\n", ret);
            exit();
        }
    }
    while (wait() >= 0)
        ;

    int new_parent_pa = va2pa((uint)arr);
    if (new_parent_pa == FAILED) {
        printerr("va2pa(0x%x) failed after forking\n", arr);
        failed();
    }
    if (parent_pa != new_parent_pa) {
        printerr("Parent has different pa after forking\n");
        failed();
    }
    int new_parent_sum = sum(arr, n);
    if (new_parent_sum != parent_sum) {
        printerr("Parent sum changed after forking\n");
        failed();
    }
    success();
}
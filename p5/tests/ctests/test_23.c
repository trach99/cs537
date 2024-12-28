#include "tester.h"

// ====================================================================
// TEST_23
// Summary: COW: allocated array has diff pa in parent and child after modification
//
// Checks whether the child process sees old contents of the mapping
// Checks whether the parent process sees the old contents of the mapping after child
// unmaps Does not check for deallocation of pages Does not check for lazy allocation
// of pages
// ====================================================================

char *test_name = "TEST_23";

int sum(char *arr, int n) {
    int s = 0;
    for (int i = 0; i < n; i++) {
        s += arr[i];
    }
    return s;
}

int main(int argc, char *argv[]) {
    printf(1, "\n\n%s\n", test_name);

    // create an array of 5 pages in parent
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
            // 
            // Child process
            //
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
            printf(1, "Child: modified sum = %d\n", sum(arr, n));
            int new_child_pa = va2pa((uint)arr);
            if (new_child_pa == FAILED) {
                printerr("va2pa(0x%x) failed\n", arr);
                failed();
            }
            printf(1, "INFO: Child: arr new_pa = 0x%x\n", new_child_pa);
            if (parent_pa == new_child_pa) {
                printerr("Parent and child have the same pa after modification\n");
                failed();
            }
            exit();
        }
    }
    // wait for all child processes to finish
    while (wait() >= 0)
        ;

    // verify that parent arr is intacts
    int new_parent_pa = va2pa((uint)arr);
    if (new_parent_pa == FAILED) {
        printerr("va2pa(0x%x) failed after forking\n", arr);
        failed();
    }
    if (parent_pa != new_parent_pa) {
        printerr("Parent has different pa after forking\n");
        failed();
    }
    if (sum(arr, n) != parent_sum) {
        printerr("Parent sum changed after forking\n");
        failed();
    }
    success();
}
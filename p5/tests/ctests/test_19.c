#include "tester.h"

// ====================================================================
// TEST_19
// Summary: Fork+EDIT: Both parent and child can see each other's edits in anonymous
// shared mappings
// ====================================================================

char *test_name = "TEST_19";

int main() {
    printf(1, "\n\n%s\n", test_name);
    validate_initial_state();

    //
    // Place map
    //
    int anon = MAP_FIXED | MAP_ANONYMOUS | MAP_SHARED;
    int addr = MMAPBASE;
    int length = PGSIZE * 10;
    uint map = wmap(addr, length, anon, 0);
    if (map != addr) {
        printerr("wmap() returned %d\n", (int)map);
        failed();
    }
    struct wmapinfo winfo;
    get_n_validate_wmap_info(&winfo, 1);   // 1 map exists
    map_exists(&winfo, map, length, TRUE); // map 2 exists
    printf(1, "INFO: Placed map 1 at 0x%x with length %d. \tOkay.\n", map, length);

    //
    // Access map
    //
    char val = 100;
    char *arr = (char *)map;
    for (int i = 0; i < length; i++) {
        arr[i] = val;
    }

    char newval_parent = val + 5;
    char newval_child = val + 10;
    int pid = fork();
    if (pid < 0) {
        printerr("fork() failed\n");
        failed();
    } else if (pid == 0) {
        sleep(500); // let parent edit the data in the meantime

        get_n_validate_wmap_info(&winfo, 1);
        map_exists(&winfo, map, length, TRUE);
        // validate contents of map 1
        for (int i = 1; i < length; i++) {
            if (arr[i] != val) {
                printerr("Child: addr 0x%x contains %d, expected %d\n", map + i,
                         arr[i], val);
                failed();
            }
        }
        printf(1, "INFO: Child process sees Map 1. \tOkay\n");

        // verify parent's modification
        if (arr[0] != newval_parent) {
            printerr("Child could not see the parent's modification\n");
            failed();
        }
        printf(1, "INFO: Child sees the parent's modification. \tOkay\n");

        // modify the data
        arr[0] = newval_child;

        // child process exits
        exit();
    } else {
        arr[0] = newval_parent;
        wait();

        get_n_validate_wmap_info(&winfo, 1);
        map_exists(&winfo, map, length, TRUE);
        // validate contents of map 1
        for (int i = 1; i < length; i++) {
            if (arr[i] != val) {
                printerr("Parent: addr 0x%x contains %d, expected %d\n", map + i,
                         arr[i], val);
                failed();
            }
        }
        printf(1, "INFO: Parent process sees Map 1. \tOkay\n");

        // verify child's modification
        if (arr[0] != newval_child) {
            printerr("Parent could not see the child's modification\n");
            failed();
        }
        printf(1, "INFO: Parent sees the child's modification. \tOkay\n");

        // parent process exits
        success();
    }
}

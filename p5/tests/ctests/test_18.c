#include "tester.h"

// ====================================================================
// TEST_18
// Summary: Fork: Same map contents exist in both parent and multiple childs
// ====================================================================

char *test_name = "TEST_18";

int main() {
    printf(1, "\n\n%s\n", test_name);
    validate_initial_state();

    char *filename = "big.txt";
    int N_PAGES = 2;
    char val = 113;
    int filelength = create_big_file(filename, N_PAGES, val);

    //
    // Place map 1 (fixed and filebacked)
    //
    int filebacked = MAP_FIXED | MAP_SHARED;
    uint addr = MMAPBASE;
    uint length1 = filelength;
    int fd = open_file(filename, filelength);
    uint map1 = wmap(addr, length1, filebacked, fd);
    if (map1 != addr) {
        printerr("wmap() returned %d\n", (int)map1);
        failed();
    }
    struct wmapinfo winfo;
    get_n_validate_wmap_info(&winfo, 1);     // 1 map exists
    map_exists(&winfo, map1, length1, TRUE); // map 1 exists
    printf(1, "INFO: Placed map 1 at 0x%x with length %d. \tOkay.\n", map1, length1);

    //
    // Place map 2 (fixed and anonymous)
    //
    int anon = MAP_FIXED | MAP_ANONYMOUS | MAP_SHARED;
    int addr2 = map1 + length1 + PGSIZE;
    int length2 = PGSIZE * N_PAGES + 100;
    uint map2 = wmap(addr2, length2, anon, fd);
    if (map2 != addr2) {
        printerr("wmap() returned %d\n", (int)map2);
        failed();
    }
    get_n_validate_wmap_info(&winfo, 2);     // 2 maps exist
    map_exists(&winfo, map1, length1, TRUE); // map 1 exists
    map_exists(&winfo, map2, length2, TRUE); // map 2 exists
    printf(1, "INFO: Placed map 2 at 0x%x with length %d. \tOkay.\n", map2, length2);

    //
    // Access maps
    //
    char *arr1 = (char *)map1;
    for (int i = 0; i < length1; i += PGSIZE) {
        int expected = val + (i / PGSIZE);
        if (arr1[i] != expected) {
            printerr("addr 0x%x contains %d, expected %d\n", map1 + i, arr1[i],
                     expected);
            failed();
        }
    }
    char *arr2 = (char *)map2;
    int val2 = 123;
    for (int i = 0; i < length2; i++) {
        arr2[i] = val2;
    }
    // validate memory
    for (int i = 0; i < length1; i += PGSIZE) {
        va_exists(map1 + i, TRUE);
    }
    for (int i = 0; i < length2; i += PGSIZE) {
        va_exists(map2 + i, TRUE);
    }

    //
    // Fork multiple child processes
    //
    int n_childs = 10;
    for (int i = 0; i < n_childs; i++) {
        int pid = fork();
        if (pid < 0) {
            printerr("fork() failed\n");
            failed();
        } else if (pid == 0) {
            struct wmapinfo winfo2;
            get_n_validate_wmap_info(&winfo2, 2);     // 2 maps in child process
            map_exists(&winfo2, map1, length1, TRUE); // Map 1 exists
            map_exists(&winfo2, map2, length2, TRUE); // Map 2 exists

            // validate contents of map 1
            for (int i = 0; i < length1; i++) {
                int expected = val + (i / PGSIZE);
                if (arr1[i] != expected) {
                    printerr(
                        "Child (pid %d) sees addr 0x%x contains %d, expected %d\n",
                        getpid(), map1 + i, arr1[i], expected);
                    failed();
                }
            }
            // validate contents of map 2
            for (int i = 0; i < length2; i++) {
                if (arr2[i] != val2) {
                    printerr(
                        "Child (pid %d) sees addr 0x%x contains %d, expected %d\n",
                        getpid(), map2 + i, arr2[i], val2);
                    failed();
                }
            }

            // validate memory
            for (int i = 0; i < length1; i += PGSIZE) {
                va_exists(map1 + i, TRUE);
            }
            for (int i = 0; i < length2; i += PGSIZE) {
                va_exists(map2 + i, TRUE);
            }

            printf(1, "INFO: Map 1 and 2 exist in Child (pid %d). \tOkay.\n",
                   getpid());
            exit();
        }
    }

    while (wait() > 0)
        ;

    get_n_validate_wmap_info(&winfo, 2);     // 2 maps exist
    map_exists(&winfo, map1, length1, TRUE); // map 1 exists
    map_exists(&winfo, map2, length2, TRUE); // map 2 exists
    printf(1, "INFO: Map 1 and 2 exist in Parent (pid %d). \tOkay.\n", getpid());

    // test ends
    success();
}

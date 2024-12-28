#include "tester.h"

// ====================================================================
// TEST_7
// Summary: MAP+ALLOC: access big filebacked map (checks for memory allocation)
// ====================================================================

char *test_name = "TEST_7";

int main() {
    printf(1, "\n\n%s\n", test_name);
    validate_initial_state();

    char *filename = "big.txt";
    int N_PAGES = 5;
    char val = 113;
    int filelength = create_big_file(filename, N_PAGES, val);

    //
    // place map 1 (fixed and filebacked)
    //
    int filebacked = MAP_FIXED | MAP_SHARED;
    uint addr = MMAPBASE;
    uint length = filelength;
    int fd = open_file(filename, filelength);
    uint map = wmap(addr, length, filebacked, fd);
    if (map != addr) {
        printerr("wmap() returned %d\n", (int)map);
        failed();
    }
    // validate mid state
    struct wmapinfo winfo;
    get_n_validate_wmap_info(&winfo, 1);   // 1 map exists
    map_exists(&winfo, map, length, TRUE); // map 1 exists
    printf(1, "INFO: Placed map 1 at 0x%x with length %d. \tOkay.\n", map, length);

    //
    // Access map
    //
    char *arr = (char *)map;
    for (int pg = 0; pg < N_PAGES; pg++) {
        for (int i = 0; i < PGSIZE; i++) {
            int offset = pg * PGSIZE + i;
            if (arr[offset] != val + pg) {
                printerr("addr 0x%x contains %d, expected %d\n", map + offset,
                         arr[offset], val);
                failed();
            }
        }
    }
    // validate final state
    get_n_validate_wmap_info(&winfo, 1);         // 1 map exists
    map_allocated(&winfo, map, length, N_PAGES); // 1 page loaded
    for (int i = 0; i < N_PAGES; i++) {
        va_exists(map + i * PGSIZE, TRUE);
    }
    va_exists(map + N_PAGES * PGSIZE, FALSE); // no more pages are mapped in pgdir
    printf(1, "INFO: Accessed all pages of Map 1. \tOkay.\n");

    // test ends
    success();
}

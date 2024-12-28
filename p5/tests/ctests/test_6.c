#include "tester.h"

// ====================================================================
// TEST_6
// Summary: MAP+ALLOC: Access fixed anonymous map (checks for memory allocation)
// ====================================================================

char *test_name = "TEST_6";

int main() {
    printf(1, "\n\n%s\n", test_name);
    validate_initial_state();

    // place map 1 (fixed and anonymous)
    int anon = MAP_FIXED | MAP_ANONYMOUS | MAP_SHARED;
    uint addr = MMAPBASE + PGSIZE * 2;
    int length = 2 * PGSIZE + 100;
    uint map = wmap(addr, length, anon, 0);
    if (map != addr) {
        printerr("wmap() returned %d\n", (int)map);
        failed();
    }
    // validate map 1
    struct wmapinfo winfo;
    get_n_validate_wmap_info(&winfo, 1); // 1 map exists
    map_exists(&winfo, addr, length, TRUE);
    printf(1, "INFO: Map 1 at 0x%x with length %d. \tOkay.\n", map, length);

    // access all pages of map 1
    char *arr = (char *)map;
    char val = 'p';
    for (int i = 0; i < length; i++) {
        arr[i] = val;
    }
    // validate all pages of map 1
    for (int i = 0; i < length; i++) {
        if (arr[i] != val) {
            printerr("addr 0x%x contains %d, expected %d\n", addr + i, arr[i], val);
            failed();
        }
    }
    // validate map 1 after accessing all pages
    get_n_validate_wmap_info(&winfo, 1); // 1 map exists
    int n_pages = PGROUNDUP(length) / PGSIZE;
    map_allocated(&winfo, addr, length, n_pages); // 3 pages loaded
    for (int i = 0; i < length; i += PGSIZE) {
        uint va = map + i;
        va_exists(va, TRUE); // each virtual address exists in pgdir
    }
    va_exists(addr + n_pages * PGSIZE, FALSE); // va after the map does not exist
    printf(1, "INFO: Accessed all pages of Map 1. \tOkay.\n");

    // test ends
    success();
}

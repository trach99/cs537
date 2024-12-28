#include "tester.h"

// ====================================================================
// TEST_14
// Summary: UNMAP+DEALLOC: Unmap accessed anonymous map and check for memory deallocation
// ====================================================================

char *test_name = "TEST_14";

int main() {
    printf(1, "\n\n%s\n", test_name);
    validate_initial_state();

    //
    // Place a anonymous map
    //
    int anon = MAP_FIXED | MAP_ANONYMOUS | MAP_SHARED;
    uint addr = MMAPBASE + PGSIZE * 2;
    int length = 2 * PGSIZE + 100;
    uint map = wmap(addr, length, anon, 0);
    if (map != addr) {
        printerr("wmap() returned %d\n", (int)map);
        failed();
    }
    struct wmapinfo winfo;
    get_n_validate_wmap_info(&winfo, 1); // 1 map exists
    map_exists(&winfo, addr, length, TRUE);
    printf(1, "INFO: Map 1 at 0x%x with length %d. \tOkay.\n", map, length);

    //
    // Access all pages of map 1
    //
    char *arr = (char *)map;
    char val = 'p';
    for (int i = 0; i < length; i++) {
        arr[i] = val;
    }
    get_n_validate_wmap_info(&winfo, 1); // 1 map exists
    int n_pages = PGROUNDUP(length) / PGSIZE;
    map_allocated(&winfo, addr, length, n_pages); // 3 pages loaded
    for (int i = 0; i < length; i += PGSIZE) {
        va_exists((uint)(map + i), TRUE); // each va exists in pgdir
    }
    va_exists(addr + n_pages * PGSIZE, FALSE); // va after the map does not exist
    printf(1, "INFO: Accessed all pages of Map 1. \tOkay.\n");

    //
    // Unmap the map
    //
    int ret = wunmap(map);
    if (ret < 0) {
        printerr("wunmap() returned %d\n", ret);
        failed();
    }
    get_n_validate_wmap_info(&winfo, 0);    // no maps exist
    map_exists(&winfo, map, length, FALSE); // the map does not exist
    printf(1, "INFO: Map 1 unmapped. \tOkay.\n");
    for (int i = 0; i < length; i += PGSIZE) {
        va_exists((uint)(map + i), FALSE); // each va is deallocated
    }
    printf(1, "INFO: Unmapped pages are deallocated. \tOkay.\n");

    // test ends
    success();
}

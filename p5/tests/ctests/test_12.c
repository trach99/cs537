#include "tester.h"

// ====================================================================
// TEST_12
// Summary: UNMAP: Unmap one anonymous map
// ====================================================================

char *test_name = "TEST_12";

int main() {
    printf(1, "\n\n%s\n", test_name);
    validate_initial_state();

    //
    // Place a anonymous map
    //
    uint addr = MMAPBASE + PGSIZE * 2;
    uint length = PGSIZE * 4 + 8;
    int anon = MAP_FIXED | MAP_ANONYMOUS | MAP_SHARED;
    int fd = -1;
    uint map = wmap(addr, length, anon, fd);
    if (map != addr) {
        printerr("wmap() returned %d\n", (int)map);
        failed();
    }
    struct wmapinfo winfo;
    get_n_validate_wmap_info(&winfo, 1);   // 1 map exists
    map_exists(&winfo, map, length, TRUE); // the map exists
    printf(1, "INFO: Map 1 at 0x%x with length 0x%x. \tOkay.\n", map, length);

    //
    // Access the memory
    //
    char *arr = (char *)map;
    for (int i = 0; i < length; i++) {
        arr[i] = 'a';
    }

    //
    // Unmap the map
    //
    int ret = wunmap(map);
    if (ret < 0) {
        printerr("wunmap() returned %d\n", ret);
        failed();
    }
    // validate final state
    get_n_validate_wmap_info(&winfo, 0);    // no maps exist
    map_exists(&winfo, map, length, FALSE); // the map does not exist
    printf(1, "INFO: Map 1 unmapped. \tOkay.\n");

    // test ends
    success();
}

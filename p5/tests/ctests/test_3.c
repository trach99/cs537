#include "tester.h"

// ====================================================================
// TEST_3
// Summary: MAP: Place one fixed anonymous map
// ====================================================================

char *test_name = "TEST_3";

int main() {
    printf(1, "\n\n%s\n", test_name);
    validate_initial_state();

    // place map1 at address 0x60002000
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
    // Place map with wrong flags (MAP_FIXED missing or MAP_SHARED missing)
    //
    addr = MMAPBASE + PGSIZE * 10;
    length = PGSIZE * 4;
    int wrongflag = MAP_ANONYMOUS | MAP_SHARED;
    int ret = wmap(addr, length, wrongflag, fd);
    if (ret != FAILED) {
        printerr("wmap() returned %d, expected -1\n", ret);
        failed();
    }
    int map_private = 0x0001;
    wrongflag = MAP_ANONYMOUS | map_private | MAP_FIXED;
    ret = wmap(addr, length, wrongflag, fd);
    if (ret != FAILED) {
        printerr("wmap() returned %d, expected -1\n", ret);
        failed();
    }

    // test ends
    success();
}

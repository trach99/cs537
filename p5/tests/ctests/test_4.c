#include "tester.h"

// ====================================================================
// TEST_4
// Summary: MAP: Place one fixed filebacked map
// ====================================================================

char *test_name = "TEST_4";

int main() {
    printf(1, "\n\n%s\n", test_name);
    validate_initial_state();

    // create and open a small file
    char *filename = "small.txt";
    char val = 101;
    int filelength = create_small_file(filename, val);
    int fd = open_file(filename, filelength);

    // place one map
    uint addr = MMAPBASE + PGSIZE * 47;
    uint length = filelength;
    int filebacked = MAP_FIXED | MAP_SHARED;
    uint map = wmap(addr, length, filebacked, fd);
    if (map != addr) {
        printerr("wmap() returned %d\n", (int)map);
        failed();
    }
    close(fd);

    // validate final state
    struct wmapinfo winfo;
    get_n_validate_wmap_info(&winfo, 1);   // 1 map exists
    map_exists(&winfo, map, length, TRUE); // the map exists
    printf(1, "INFO: Map 1 at 0x%x with length 0x%x. \tOkay.\n", map, length);

    // test ends
    success();
}

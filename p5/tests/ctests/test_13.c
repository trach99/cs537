#include "tester.h"

// ====================================================================
// TEST_13
// Summary: UNMAP: Unmaps a filebacked map
// ====================================================================

char *test_name = "TEST_13";

int main() {
    printf(1, "\n\n%s\n", test_name);
    validate_initial_state();

    char *filename = "small.txt";
    char val = 101;
    int filelength = create_small_file(filename, val);
    int fd = open_file(filename, filelength);

    //
    // Place a filebacked map
    //
    uint addr = MMAPBASE + PGSIZE * 47;
    uint length = filelength;
    int filebacked = MAP_FIXED | MAP_SHARED;
    uint map = wmap(addr, length, filebacked, fd);
    if (map != addr) {
        printerr("wmap() returned %d\n", (int)map);
        failed();
    }
    close(fd);
    struct wmapinfo winfo;
    get_n_validate_wmap_info(&winfo, 1);   // 1 map exists
    map_exists(&winfo, map, length, TRUE); // the map exists
    printf(1, "INFO: Map 1 at 0x%x with length 0x%x. \tOkay.\n", map, length);

    //
    // Access the memory
    //
    char *arr = (char *)map;
    for (int i = 0; i < length; i++) {
        if (arr[i] != val) {
            printf(1, "ERROR: arr[%d] = %d, expected %d\n", i, arr[i], val);
            failed();
        }
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

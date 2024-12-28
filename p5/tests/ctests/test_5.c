#include "tester.h"

// ====================================================================
// TEST_5
// Summary: MAP: Place multiple maps, verify that overlapping maps are not allowed
// ====================================================================

char *test_name = "TEST_5";

int main() {
    printf(1, "\n\n%s\n", test_name);
    validate_initial_state();

    int anon = MAP_FIXED | MAP_ANONYMOUS | MAP_SHARED;
    int filebacked = MAP_FIXED | MAP_SHARED;
    uint *maps = (uint *)malloc(MAX_WMMAP_INFO * sizeof(uint));
    uint *lengths = (uint *)malloc(MAX_WMMAP_INFO * sizeof(uint));
    int idx = 0;

    char *filename = "small.txt";
    char val = 101;
    int filelength = create_small_file(filename, val);

    //
    // place map 1 (filebacked) at MMABASE
    //
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
    printf(1, "INFO: Map 1 at 0x%x with length %d. \tOkay.\n", map, length);
    maps[idx] = map;
    lengths[idx] = length;
    idx++;

    //
    // place map 2 (anon) at MMABASE + 2 * PGSIZE
    //
    uint addr2 = addr + PGSIZE * 2;
    uint length2 = PGSIZE * 2;
    uint map2 = wmap(addr2, length2, anon, fd);
    if (map2 != addr2) {
        printerr("wmap() returned %d\n", (int)map2);
        failed();
    }
    // validate mid state
    get_n_validate_wmap_info(&winfo, 2);     // 2 maps exist
    map_exists(&winfo, map, length, TRUE);   // map 1 exists
    map_exists(&winfo, map2, length2, TRUE); // map 2 exists
    printf(1, "INFO: Map 2 at 0x%x with length %d. \tOkay.\n", map2, length2);
    maps[idx] = map2;
    lengths[idx] = length2;
    idx++;

    //
    // place map 3 (anon) - should fail
    //
    uint addr3 = addr2 + PGSIZE;
    uint length3 = PGSIZE * 3 + 8;
    int map3 = wmap(addr3, length3, anon, fd);
    if (map3 != FAILED) {
        printerr("wmap() does not fail\n", map3);
        failed();
    }
    // validate mid state
    get_n_validate_wmap_info(&winfo, 2);      // 2 maps exist
    map_exists(&winfo, map, length, TRUE);    // map 1 exists
    map_exists(&winfo, map2, length2, TRUE);  // map 2 exists
    map_exists(&winfo, map3, length3, FALSE); // map 3 does not exist
    printf(1, "INFO: Map 3 does not exist. \tOkay.\n");

    // place another anon map
    int addr4 = PGROUNDUP(addr2 + length2);
    int length4 = PGSIZE * 200;
    uint map4 = wmap(addr4, length4, anon, fd);
    if (map4 != addr4) {
        printerr("wmap() returned %d\n", (int)map4);
        failed();
    }
    // validate final state
    struct wmapinfo winfo5;
    get_n_validate_wmap_info(&winfo5, 3);      // 3 maps exist
    map_exists(&winfo5, map, length, TRUE);    // map 1 exists
    map_exists(&winfo5, map2, length2, TRUE);  // map 2 exists
    map_exists(&winfo5, map3, length3, FALSE); // map 3 does not exist
    map_exists(&winfo5, map4, length4, TRUE);  // map 4 exists
    printf(1, "INFO: Map 4 at 0x%x with length %d. \tOkay.\n", map4, length4);
    maps[idx] = map4;
    lengths[idx] = length4;
    idx++;

    // check for overlap among maps
    check_overlaps(maps, lengths, idx);

    // test ends
    success();
}

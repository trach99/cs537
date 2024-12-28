#include "tester.h"

// ====================================================================
// TEST_16
// Summary: UNMAP: Edit a filebacked map and verify its changes are reflected
// ====================================================================

char *test_name = "TEST_16";

int main() {
    printf(1, "\n\n%s\n", test_name);
    validate_initial_state();

    char *filename = "big.txt";
    int N_PAGES = 5;
    char val = 113;
    int filelength = create_big_file(filename, N_PAGES, val);

    //
    // Place map 1 (fixed and filebacked)
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
    get_n_validate_wmap_info(&winfo, 1);         // 1 map exists
    map_allocated(&winfo, map, length, N_PAGES); // 1 page loaded
    for (int i = 0; i < N_PAGES; i++) {
        va_exists(map + i * PGSIZE, TRUE); // each va exists in pgdir
    }
    va_exists(map + N_PAGES * PGSIZE, FALSE); // no more pages are mapped in pgdir
    printf(1, "INFO: Accessed all pages of Map 1. \tOkay.\n");

    //
    // Edit the map
    //
    char newval = 75;
    for (int i = 0; i < length; i++) {
        arr[i] = newval;
    }

    //
    // Unmap the map
    //
    int ret = wunmap(map);
    if (ret < 0) {
        printf(1, "Cause: `wunmap()` returned %d\n", ret);
        failed();
    }
    get_n_validate_wmap_info(&winfo, 0);     // no maps exist
    map_exists(&winfo, addr, length, FALSE); // map 1 does not exist
    printf(1, "Map 1 unmapped. \tOkay.\n");
    for (int i = 0; i < length; i += PGSIZE) {
        va_exists((uint)(map + i), FALSE); // each va is deallocated
    }
    printf(1, "Unmapped pages are deallocated. \tOkay.\n");

    // reopen the file and validate its content
    fd = open(filename, 'r');
    if (fd < 0) {
        printerr("Failed to open file %s\n", filename);
        failed();
    }
    int bufflen = 512;
    char buff[bufflen];
    for (int i = 0; i < length; i += bufflen) {
        if (read(fd, buff, bufflen) < 0) {
            printerr("Read from file %s FAILED, offset %d\n", filename, i);
            failed();
        }
        for (int j = 0; j < bufflen; j++) {
            if (buff[j] != newval) {
                printf(1, "file %s offset %d = %d, expected %d\n", filename, i + j,
                       buff[j], newval);
                failed();
            }
        }
    }
    close(fd);
    printf(1, "INFO: Edited map content is reflected in the file. \tOkay.\n");

    // test ends
    success();
}

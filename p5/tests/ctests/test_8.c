#include "tester.h"

// ====================================================================
// TEST_8
// Summary: MAP: Places a large number of maps and accesses all pages of each map
// ====================================================================

char *test_name = "TEST_8";

int main() {
    printf(1, "\n\n%s\n", test_name);
    validate_initial_state();

    int N_MAPS = MAX_WMMAP_INFO;
    uint *maps = malloc(N_MAPS * sizeof(uint));
    uint *lengths = malloc(N_MAPS * sizeof(uint));
    int anon = MAP_FIXED | MAP_ANONYMOUS | MAP_SHARED;
    int filebacked = MAP_FIXED | MAP_SHARED;
    int fd = -1;
    int idx = 0;

    char *smallfile = "smallfile";
    int smallfilelen = create_small_file("smallfile", 'a'); // create a small file
    char *bigfile = "bigfile";
    int bigfilelen = create_big_file("bigfile", 5, 'b'); // create a big file

    //
    // 1. Place Map 1 at MMABASE with length 400 pages
    //
    uint addr = MMAPBASE;
    uint length = PGSIZE * 400;
    uint map = wmap(addr, length, anon, fd);
    if (map != addr) {
        printerr("wmap() returned %d\n", (int)map);
        failed();
    }
    maps[idx] = map;
    lengths[idx] = length;
    idx++;
    printf(1, "INFO: Map 1 is placed at 0x%x. \tOkay.\n", addr);

    //
    // 2. Place Map 2 at MMABASE + 401 pages with length bigfilelen
    //
    int bigfd = open_file(bigfile, bigfilelen);
    addr = MMAPBASE + PGSIZE * 401;
    length = bigfilelen;
    map = wmap(addr, length, filebacked, bigfd);
    if (map != addr) {
        printerr("wmap() returned %d\n", (int)map);
        failed();
    }
    close(bigfd);
    maps[idx] = map;
    lengths[idx] = length;
    idx++;
    printf(1, "INFO: Map 2 is placed at 0x%x. \tOkay.\n", addr);

    //
    // 3. Place Map 3 at MMABASE + 400 pages with length smallfilelen
    //
    int smallfd = open_file(smallfile, smallfilelen);
    addr = MMAPBASE + PGSIZE * 400;
    length = smallfilelen;
    map = wmap(addr, length, filebacked, smallfd);
    if (map != addr) {
        printerr("wmap() returned %d\n", (int)map);
        failed();
    }
    close(smallfd);
    maps[idx] = map;
    lengths[idx] = length;
    idx++;
    printf(1, "INFO: Map 3 is placed at 0x%x. \tOkay.\n", addr);

    //
    // 4. Place Map 4 at MMABASE + 401 pages + bigfilelen with length 2000 pages
    //
    addr = MMAPBASE + PGSIZE * 401 + bigfilelen;
    length = PGSIZE * 2000;
    map = wmap(addr, length, anon, fd);
    if (map != addr) {
        printerr("wmap() returned %d\n", (int)map);
        failed();
    }
    maps[idx] = map;
    lengths[idx] = length;
    idx++;
    printf(1, "INFO: Map 4 is placed at 0x%x. \tOkay.\n", addr);

    //
    // Place all other maps with increasing length 25, 30, 35, 40, 45, 50, 55, 60,
    // 65, 70
    addr = maps[3] + lengths[3] + PGSIZE * 5;
    for (int i = idx; i < N_MAPS; i++) {
        length = PGSIZE * (i + 1) * 5;
        map = wmap(addr, length, anon, fd);
        if (map != addr) {
            printerr("wmap() returned %d\n", (int)map);
            failed();
        }
        maps[idx] = map;
        lengths[idx] = length;
        addr = map + length;
        idx++;
        printf(1, "INFO: Map %d is placed at 0x%x. \tOkay.\n", i + 1, map);
    }

    // check for overlap among maps
    check_overlaps(maps, lengths, N_MAPS);
    printf(1, "INFO: Map 1 ~ %d do not overlap with each other. \tOkay\n",
           N_MAPS); // NOTE

    // access all pages of each map
    for (int i = 0; i < N_MAPS; i++) {
        char *arr = (char *)maps[i];
        char val = 'p';
        for (int j = 0; j < lengths[i]; j++) {
            arr[j] = val;
        }
        printf(1, "\tAccessed Map %d. \tOkay.\n", i + 1, lengths[i]);
    }
    printf(1, "INFO: Accessed all pages of Map 1 ~ %d. \tOkay.\n", N_MAPS);
    // validate final state
    struct wmapinfo winfo;
    get_n_validate_wmap_info(&winfo, N_MAPS); // N_MAPS maps exist
    for (int i = 0; i < N_MAPS; i++) {
        int n_pages = (lengths[i] + PGSIZE - 1) / PGSIZE;
        map_allocated(&winfo, maps[i], lengths[i], n_pages);
    }
    for (int i = 0; i < N_MAPS; i++) {
        for (int j = 0; j < lengths[i]; j += PGSIZE) {
            va_exists(maps[i] + j, TRUE);
        }
        printf(1, "\tMap %d is allocated. \tOkay.\n", i + 1);
    }
    printf(1, "INFO: All pages of Map 1 ~ %d are allocated. \tOkay.\n", N_MAPS);
    // test ends
    success();
}

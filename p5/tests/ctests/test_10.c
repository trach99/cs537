#include "tester.h"

// ====================================================================
// TEST_10
// Summary: MAP+LAZY: Checks for lazy allocation in filebacked mapping
// ====================================================================

char *test_name = "TEST_10";

int main() {
    printf(1, "\n\n%s\n", test_name);
    validate_initial_state();

    char *bigfile = "bigfile";
    int bigval = 111;
    int bigfilelen = create_big_file(bigfile, 3, bigval);
    char *smallfile = "smallfile";
    int smallval = 65;
    int smallfilelen = create_small_file(smallfile, smallval);

    
    int filebacked = MAP_FIXED | MAP_SHARED;

    //
    // Place Map 1 at MMABASE with length 3 pages
    //
    int bigfd = open_file(bigfile, bigfilelen);
    uint addr = MMAPBASE;
    int length = bigfilelen;
    uint bigmap = wmap(addr, length, filebacked, bigfd);
    if (bigmap != addr) {
        printerr("wmap() returned %d\n", (int)bigmap);
        failed();
    }
    close(bigfd);
    // validate mid state
    struct wmapinfo winfo;
    get_n_validate_wmap_info(&winfo, 1); // one map exists
    printf(1, "INFO: Map 1 at 0x%x with length 0x%x. \tOkay.\n", bigmap, length);

    // Place Map 2 at MMABASE + 3 pages with length 1 page
    int smallfd = open_file(smallfile, smallfilelen);
    addr = MMAPBASE + bigfilelen + PGSIZE;
    length = smallfilelen;
    uint smallmap = wmap(addr, length, filebacked, smallfd);
    if (smallmap != addr) {
        printerr("wmap() returned %d\n", (int)smallmap);
        failed();
    }
    close(smallfd);
    // validate mid state
    // struct wmapinfo winfo3;
    get_n_validate_wmap_info(&winfo, 2); // two maps exist
    printf(1, "INFO: Map 2 at 0x%x with length 0x%x. \tOkay.\n", smallmap, length);

    //
    // Check for lazy allocation before accessing maps
    //
    map_allocated(&winfo, bigmap, bigfilelen, 0);
    map_allocated(&winfo, smallmap, smallfilelen, 0);
    printf(1, "INFO: n_loaded_pages zero. \tOkay.\n");
    for (int i = 0; i < bigfilelen; i += PGSIZE) {
        va_exists(bigmap + i, FALSE);
    }
    for (int i = 0; i < smallfilelen; i += PGSIZE) {
        va_exists(smallmap + i, FALSE);
    }
    printf(1, "INFO: No page have entry in pgdir yet. \tOkay.\n");

    //
    // Access maps
    //
    char *arr = (char *)smallmap;
    for (int i = 0; i < smallfilelen; i++) {
        if (arr[i] != smallval) {
            printerr("addr 0x%x contains %d, expected %d\n", smallmap + i, arr[i],
                     smallval);
            failed();
        }
    }
    arr = (char *)bigmap;
    for (int i = 0; i < PGSIZE; i++) {
        if (arr[PGSIZE + i] != bigval + 1) {
            printerr("addr 0x%x contains %d, expected %d\n", bigmap + PGSIZE + i,
                     arr[PGSIZE + i], bigval + 1);
            failed();
        }
    }
    printf(1, "INFO: Accessed second page. \tOkay.\n");

    //
    // validate final state
    //
    get_n_validate_wmap_info(&winfo, 2);          // two maps exist
    map_allocated(&winfo, bigmap, bigfilelen, 1); // 1 page loaded
    va_exists(smallmap, TRUE);                    // first page is allocated
    printf(1, "INFO: SmallMap first page is allocated. \tOkay.\n");
    map_allocated(&winfo, smallmap, smallfilelen, 1); // 1 page loaded
    va_exists(bigmap, FALSE);                         // first page is NOT allocated
    printf(1, "INFO: BigMap first page is NOT allocated. \tOkay.\n");
    va_exists(bigmap + PGSIZE, TRUE); // second page is allocated
    printf(1, "INFO: BigMap second page is allocated. \tOkay.\n");
    va_exists(bigmap + PGSIZE * 2, FALSE); // third page is NOT allocated
    printf(1, "INFO: Bigmap third page is NOT allocated. \tOkay.\n");

    // test ends
    success();
}

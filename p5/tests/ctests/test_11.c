#include "tester.h"

// ====================================================================
// TEST_11
// Summary: MAP+LAZY+STRESS: Checks for lazy allocation in filebacked mapping
// ====================================================================

char *test_name = "TEST_11";

int main() {
    printf(1, "\n\n%s\n", test_name);
    validate_initial_state();

    //
    // Place Map 1 at MMABASE with length 3 pages
    //
    int N_PAGES = 70000;
    int anon = MAP_FIXED | MAP_ANONYMOUS | MAP_SHARED;
    uint addr = MMAPBASE;
    int length = PGSIZE * N_PAGES;
    uint map = wmap(addr, length, anon, 0);
    if (map != addr) {
        printerr("wmap() returned %d\n", (int)map);
        failed();
    }
    // validate mid state
    struct wmapinfo winfo;
    get_n_validate_wmap_info(&winfo, 1); // one map exists
    printf(1, "INFO: Map 1 at 0x%x with length 0x%x. \tOkay.\n", map, length);

    //
    // checks for lazy allocation
    //
    map_allocated(&winfo, map, length, 0); // no pages loaded yet
    for (int i = 0; i < N_PAGES; i++) {
        va_exists(map + PGSIZE * i, FALSE); // no page is mapped yet in pgdir
    }
    printf(1, "INFO: No page have entry in pgdir yet. \tOkay.\n");

    //
    // 2. read from the second page of the mapping, causing page fault
    //
    char *arr = (char *)map;
    for (int i = 0; i < PGSIZE; i++) {
        if (arr[PGSIZE + i] != 0) {
            printerr("Expected the page to be zero initialized\n");
            failed();
        }
    }
    printf(1, "INFO: Accessed second page. \tOkay.\n");

    // validate final state
    get_n_validate_wmap_info(&winfo, 1);   // one map exists
    map_allocated(&winfo, map, length, 1); // 1 page loaded
    va_exists(map, FALSE);                 // first page is NOT allocated
    printf(1, "INFO: First page is not allocated. \tOkay.\n");
    va_exists(map + PGSIZE, TRUE); // second page is allocated
    printf(1, "INFO: Second page is allocated. \tOkay.\n");
    for (int i = 2; i < N_PAGES; i++) {
        va_exists(map + PGSIZE * i, FALSE); // all pages are allocated
    }
    printf(1, "INFO: Third to last pages are not allocated. \tOkay.\n");

    // test ends
    success();
}

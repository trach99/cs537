// whatever the header file name is, here it should be tester.h,
// because the header file is copied as "tester.h" during testing
#include "tester.h"

// ====================================================================
// TEST_1
// Summary: Checks the presence of all system calls
// ====================================================================

char *test_name = "TEST_1";

void test_va2pa(void) {
    uint va = 0x0;
    uint pa = get_n_validate_va2pa(va);
    printf(1, "INFO: va2pa(0x%x) returned 0x%x\n", va, pa);
}

void test_getwmapinfo(void) {
    struct wmapinfo info;
    get_n_validate_wmap_info(&info, 0);
    printf(1, "INFO: total_mmaps = %d\n", info.total_mmaps);
}

void test_wmap(void) {
    uint addr = 0;
    int length = 100;
    int flags = MAP_FIXED | MAP_ANONYMOUS | MAP_SHARED;
    int fd = 0;
    int ret = wmap(addr, length, flags, fd);
    printf(1, "INFO: wmap() returned %d\n", ret);
}

void test_wunmap(void) {
    uint addr = 0;
    int ret = wunmap(addr);
    printf(1, "INFO: wunmap() returned %d\n", ret);
}

int main() {
    printf(1, "\n\n%s\n", test_name);

    test_va2pa();
    test_getwmapinfo();
    test_wmap();
    test_wunmap();

    // test ends
    success();
}

#ifndef TESTER_H
#define TESTER_H

#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "fs.h"
#include "stat.h"

#include "wmap.h"

// Test Helpers
#define MMAPBASE 0x60000000
#define KERNBASE 0x80000000
#define KERNCODE 0x100000
#define PHYSTOP 0xE000000 // Top physical memory
#define PGSIZE 0x1000
#define TRUE 1
#define FALSE 0
#define PGROUNDUP(sz) (((sz) + PGSIZE - 1) & ~(PGSIZE - 1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE - 1))

#define printerr(fmt, ...)                                                          \
    printf(1,                                                                       \
           "\033[0;31m"                                                             \
           "ERROR: "                                                                \
           "\033[0m" fmt,                                                           \
           ##__VA_ARGS__)
#define printinfo(fmt, ...)                                                         \
    printf(1,                                                                       \
           "\033[0;34m"                                                             \
           "INFO: "                                                                 \
           "\033[0m" fmt,                                                           \
           ##__VA_ARGS__)

extern char *test_name;

void success() {
    printf(1, "\033[0;32mSUCCESS:\033[0m %s\t PASSED\n\n", test_name);
    exit();
}

void failed() {
    printf(1, "\n\033[0;31mFAIL:\033[0m %s\t FAILED (pid %d)\n\n", test_name,
           getpid());
    exit();
}

void reset_wmapinfo(struct wmapinfo *info) {
    info->total_mmaps = -1;
    for (int i = 0; i < MAX_WMMAP_INFO; i++) {
        info->addr[i] = -1;
        info->length[i] = -1;
        info->n_loaded_pages[i] = -1;
    }
}

/**
 * Get the wmapinfo and validate the total number of maps
 */
void get_n_validate_wmap_info(struct wmapinfo *info, int expected_total_mmaps) {
    reset_wmapinfo(info);
    int ret = getwmapinfo(info);
    if (ret != SUCCESS) {
        printerr("getwmapinfo() returned %d\n", ret);
        failed();
    }
    if (info->total_mmaps != expected_total_mmaps) {

        printerr("total_mmaps = %d, expected %d.\n", info->total_mmaps,
                 expected_total_mmaps);
        failed();
    }
}

/**
 * Check if a map with the given address and length exists in the list of maps
 */
void map_exists(struct wmapinfo *info, uint addr, int length, int expected) {
    int found = 0;
    for (int i = 0; i < info->total_mmaps; i++) {
        if (info->addr[i] == addr && info->length[i] == length) {
            found = 1;
            break;
        }
    }
    if (found != expected) {
        printf(
            1,
            "ERROR: expected mmap 0x%x with length 0x%x to %s in the list of maps\n",
            addr, length, expected ? "exist" : "NOT exist");
        failed();
    }
}

uint get_n_validate_va2pa(uint va) {
    int ret = va2pa(va);
    if (ret == FAILED) {
        printerr("va2pa(0x%x)` failed\n", va);
        failed();
    }
    uint pa = (uint)ret;
    if (pa < KERNCODE || pa >= PHYSTOP) {
        printerr("va2pa(0x%x) returned 0x%x, expected range [0x%x, 0x%x]\n", va, pa,
                 KERNCODE, PHYSTOP);
        failed();
    }
    return pa;
}

void map_allocated(struct wmapinfo *info, uint addr, int length,
                   int n_loaded_pages) {
    int found = 0;
    for (int i = 0; i < info->total_mmaps; i++) {
        if (info->addr[i] == addr && info->length[i] == length) {
            found = 1;
            if (info->n_loaded_pages[i] != n_loaded_pages) {
                printf(1, "Cause: expected %d pages to be loaded, but found %d\n",
                       n_loaded_pages, info->n_loaded_pages[i]);
                failed();
            }
            break;
        }
    }
    if (!found) {
        printf(1,
               "Cause: expected 0x%x with length %d to exist in the list of maps\n",
               addr, length);
        failed();
    }
}

void va_exists(uint va, int expected) {
    int ret = va2pa(va);
    if (ret == FAILED) { // va is not allocated
        if (expected) {
            printerr("expected va 0x%x to be allocated\n", va);
            failed();
        }
        return;
    }
    // va is allocated
    if (!expected) {
        printerr("va 0x%x has pa, expected it to be not allocated\n", va);
        failed();
    }
    uint pa = (uint)ret;
    if (pa < KERNCODE || pa >= PHYSTOP) {
        printerr("va2pa(0x%x) returned 0x%x, expected range [0x%x, 0x%x]\n", va, pa,
                 KERNCODE, PHYSTOP);
        failed();
    }
}

void no_mmaps_in_pgdir() {
    for (uint va = MMAPBASE; va < KERNBASE; va += PGSIZE) {
        int pa = va2pa(va);
        if (pa != FAILED) {
            printerr("va2pa(0x%x) returned 0x%x, expected FAILED\n", va, pa);
            failed();
        }
    }
}

void validate_initial_state() {
    struct wmapinfo winfo;
    get_n_validate_wmap_info(&winfo, 0); // no maps exist
    // no_mmaps_in_pgdir();                 // no maps in the mmap range in pgdir
    printf(1, "INFO: Initially 0 maps. \tOkay.\n");
}

void check_overlaps(uint *maps, uint *lengths, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j)
                continue;
            if (maps[i] >= maps[j] && maps[i] < maps[j] + lengths[j]) {
                printerr("Map (addr 0x%x) overlaps with Map (addr 0x%x)\n", maps[i],
                         maps[j]);
                failed();
            }
        }
    }
}

/**
 * Create a small file with 512 bytes of content
 */
int create_small_file(char *filename, char c) {
    // create a file
    int bufflen = 512;
    char buff[bufflen];
    int fd = open(filename, O_CREATE | O_RDWR);
    if (fd < 0) {
        printerr("Failed to create file %s\n", filename);
        failed();
    }
    // prepare the content to write
    for (int j = 0; j < bufflen; j++) {
        buff[j] = c;
    }
    // write to file
    if (write(fd, buff, bufflen) != bufflen) {
        printerr("Write to file FAILED\n");
        failed();
    }
    close(fd);
    printf(1, "INFO: Created file %s with length %d bytes. \tOkay.\n", filename,
           bufflen);
    return bufflen;
}

int create_big_file(char *filename, int N_PAGES, char c) {
    // create a file
    int bufflen = 1024;
    char buff[bufflen];
    int fd = open(filename, O_CREATE | O_RDWR);
    if (fd < 0) {
        printf(1, "\tCause:\tFailed to create file %s\n", filename);
        failed();
    }
    // write in steps as we cannot have a buffer larger than PGSIZE
    for (int pg = 0; pg < N_PAGES; pg++) {
        printf(1, "INFO: %d\n", c);
        int nchunks = PGSIZE / bufflen;
        for (int i = 0; i < bufflen; i++)
            buff[i] = c;
        for (int k = 0; k < nchunks; k++) {
            // write to file
            if (write(fd, buff, bufflen) != bufflen) {
                printerr("Write to file FAILED %d\n", pg * bufflen);
                failed();
            }
        }
        c++;
    }
    close(fd);
    printf(1, "INFO: Created file %s with length %d bytes. \tOkay.\n", filename,
           N_PAGES * PGSIZE);
    return N_PAGES * PGSIZE;
}

int open_file(char *filename, int filelength) {
    int fd = open(filename, O_RDWR); // open in read-write mode
    if (fd < 0) {
        printerr("Failed to open file %s\n", filename);
        failed();
    }
    struct stat st;
    if (fstat(fd, &st) < 0) {
        printerr("Failed to get file stat\n");
        failed();
    }
    if (st.size != filelength) {
        printerr("File size = %d, expected %d\n", st.size, filelength);
        failed();
    }
    return fd;
}

#endif // TESTER_H

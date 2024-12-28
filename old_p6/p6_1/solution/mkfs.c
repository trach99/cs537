#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include "wfs.h"

// function to get mmap array pointer
void create_disk_mmap(char **disk_name, int disk_cnt, void **disk_ptr, int disk_size, int disk_fd[])
{
    // printf("test 1 \n");
    // open files & create mmap pointers
    for (int i = 0; i < disk_cnt; i++)
    {
        // printf("does he know\n");
        int fd = open(disk_name[i], O_RDWR, 0777);
        disk_fd[i] = fd;
        disk_ptr[i] = mmap(NULL, disk_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    }
}

/*
Function to free memory maps & close file pointers
*/
void remove_disk_mmap(int disk_cnt, int disk_size, int disk_fd[], void **mmap_pointers)
{
    for (int i = 0; i < disk_cnt; i++)
    {
        // munmap
        munmap(mmap_pointers[i], disk_size);
        // free the file-descriptors
        close(disk_fd[i]);
    }
}

int main(int argc, char *argv[])
{

    // ################## parse the command line arguments ##################

    // command line arguments should be odd
    // each command line argument is accompanied by a flag
    if (argc % 2 == 0)
    { // confirm once
        return -1;
    }

    int raid_mode = -1;
    int cnt_data_blocks = 0;
    int cnt_inodes = 0;
    int cnt_disks = 0;
    char *disk_name[10] = {NULL};
    void *mmap_pointers[10] = {NULL};
    int disk_fd[10] = {0};

    uid_t process_uid = getuid();
    gid_t process_gid = getgid();

    // loop to iterate over all the command line args
    for (int i = 0; i < argc; i++)
    {
        // ----------- RAID MODE -------------
        if (strcmp(argv[i], "-r") == 0)
        {
            // printf("-r found\n");
            if (strcmp(argv[i + 1], "0") == 0)
            {
                raid_mode = 0;
                // printf("%d\n", raid_mode);
            }
            else if (strcmp(argv[i + 1], "1") == 0)
            {
                raid_mode = 1;
            }
            else if (strcmp(argv[i + 1], "1v") == 0)
            {
                raid_mode = 2;
            }
            else
            {
                printf("Error : Unknown RAID mode specified\n");
                return 1;
            }
        }

        // ---------------- count & store disk names ----------------
        if (strcmp(argv[i], "-d") == 0)
        {
            disk_name[cnt_disks] = argv[i + 1];
            cnt_disks++;
        }

        // ---------------- count & store inodes ----------------
        if (strcmp(argv[i], "-i") == 0)
        {
            cnt_inodes = atoi(argv[i + 1]);
        }

        // ---------------- count & store data blocks ----------------
        if (strcmp(argv[i], "-b") == 0)
        {
            cnt_data_blocks = atoi(argv[i + 1]);
        }
    }

    // ############### validate command line arguments ###############

    if (cnt_disks < 2)
        return 1;

    if (raid_mode == -1)
    {
        printf("Error: No raid mode specified.");
        return -1;
    }
    else
    {
        // printf("The raid mode is %d\n", raid_mode);
    }

    if (cnt_data_blocks == 0)
    {
        printf("Error: No data blocks specified.");
        return -1;
    }
    else
    {
        // convert num_data_blocks to closest multiple of 32
        if (cnt_data_blocks % 32 != 0)
        {
            cnt_data_blocks = 32 * (cnt_data_blocks / 32) + 32;
        }
        // printf("Number of data blocks = %d\n", cnt_data_blocks);
    }

    if (cnt_inodes == 0)
    {
        printf("Error: No inodes specified.");
        return -1;
    }
    else
    {
        if (cnt_inodes % 32 != 0)
        {
            cnt_inodes = 32 * (cnt_inodes / 32) + 32;
        }
        // printf("Number of inodes = %d\n", cnt_inodes);
    }

    struct stat file_stat;
    long disk_size = 0;
    if (stat(disk_name[0], &file_stat) == 0)
    {
        disk_size = file_stat.st_size;
    }
    else
    {
        perror("stat");
    }

    // ####################### Too many blocks Reqeusted #######################

    if (raid_mode == 0 &&
        disk_size < sizeof(struct wfs_sb) + (cnt_data_blocks + cnt_inodes) / 8 + (cnt_data_blocks + cnt_inodes) * 512)
    {
        return -1;
    }

    // for RAID1 & RAID1v
    if (disk_size < sizeof(struct wfs_sb) + (cnt_data_blocks + cnt_inodes) / 8 + (cnt_data_blocks + cnt_inodes) * 512)
    {
        // todo : add the size of supernode & bitmaps to the RHS of this if condtion
        // printf("Not enough disk size\n");

        return -1;
    }
    else
    {
        // printf("Enough Disk Size\n");
        // printf("\nsohi\n");
    }
    // printf("disk size = %d\n", (int)disk_size);
    // printf("blocks = %d\n", (cnt_data_blocks+cnt_inodes)*512);

    // ####################### Open Disk Files & Mmap #######################
    create_disk_mmap(disk_name, cnt_disks, mmap_pointers, disk_size, disk_fd);

    // *s    // unint start_addr
    // tart_addr & (mask...) <-- mask shifts
    // 0x1 -- 0x2 -- 0x4 -- 0x8 (char *) next element
    // mask << 1

    // determine time

    time_t seconds;
    seconds = time(NULL);

    // ###################### Write Data to Mmaps ######################
    // int disk_order = 0;
    for (int i = 0; i < cnt_disks; i++)
    {
        // --------------- write the supernode ---------------
        struct wfs_sb *sb = (struct wfs_sb *)mmap_pointers[i];

        sb->num_inodes = cnt_inodes;
        sb->num_data_blocks = cnt_data_blocks;
        sb->raid_mode = raid_mode;
        sb->disk_order = i;
        sb->total_disks = cnt_disks;

        // superblock bitmap pointers
        sb->i_bitmap_ptr = sizeof(struct wfs_sb);
        sb->d_bitmap_ptr = sb->i_bitmap_ptr + (cnt_inodes) / 8;

        // offset to the next block
        // every inode always starts at the location divisible by 512
        int size = sb->d_bitmap_ptr + (cnt_data_blocks) / 8;
        int offset = 0;
        if (size % 512 != 0)
        {
            offset = 512 - (size % 512);
        }

        // superblock inode pointer
        sb->i_blocks_ptr = size + offset;
        sb->d_blocks_ptr = sb->i_blocks_ptr + cnt_inodes * BLOCK_SIZE;

        // Change type of pointer to char to make it byte addressable
        char *base = (void *)mmap_pointers[i];

        // Allocate bitmaps:
        __u_int *i_bitmap = (__u_int *)(base + sb->i_bitmap_ptr);
        i_bitmap[0] = 1; // 1 inode for the root

        // ----------- write the inodes -----------
        struct wfs_inode *root_inode = (struct wfs_inode *)(base + sb->i_blocks_ptr);
        root_inode->num = 0;
        root_inode->mode = S_IFDIR | 0755;
        root_inode->uid = process_uid;
        root_inode->gid = process_gid;
        root_inode->size = 0;
        root_inode->nlinks = 1;
        root_inode->atim = seconds;
        root_inode->mtim = seconds;
        root_inode->ctim = seconds;
        memset(root_inode->blocks, -1, N_BLOCKS * (sizeof(off_t)));
    }

    // ################### Unmap & close file descriptors ###################
    remove_disk_mmap(cnt_disks, disk_size, disk_fd, mmap_pointers);

    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "wfs.h"
 
int main(int argc, char *argv[])
{
    /*SUPER_BLOCK*/
    struct wfs_sb *sb = malloc(sizeof(struct wfs_sb));
    sb->last_disk_index = 0;
    printf("superblock size: %d and last index: %d\n", (int)sizeof(sb), sb->last_disk_index);
 
    // ################## parse the command line arguments ##################
 
    // check if the number of command line arguments is odd since each argument is in pair
 
    
    if (argc % 2 == 0)
    {
        // confirm once
        return -1;
    }
 
    /*command line args*/
    sb->raid_mode = "-1";
    char *disk_name = "";
    sb->num_inodes = 0;
 
    /*Total Disk space available*/
    struct stat file_stat;
 
    /*initialize the memory map pointer to NULL*/
    for (int i = 0; i < 10; i++)
    {
        sb->mmap_disk[i] = NULL;
    }
 
    for (int i = 0; i < argc; i++)
    {
        // ----------- RAID MODE -------------
        if (strcmp(argv[i], "-r") == 0)
        {
            printf("-r found\n");
            // printf("%s\n", argv[i+1]);
            if (strcmp(argv[i + 1], "0") == 0)
            {
                sb->raid_mode = "0";
                printf("%s\n", sb->raid_mode);
            }
            else if (strcmp(argv[i + 1], "1") == 0)
            {
                sb->raid_mode = "1";
            }
            else if (strcmp(argv[i + 1], "1v") == 0)
            {
                sb->raid_mode = "1v";
            }
            else
            {
                printf("Error : Unknown raid mode specified\n");
                return -1;
            }
        }
        // ---------------- count & store disk files ----------------
        if (strcmp(argv[i], "-d") == 0)
        {
            disk_name = argv[i + 1];
            int fd = open(disk_name, O_RDWR, 0777);
            sb->fd_disk[sb->last_disk_index++] = fd;
 
            /*get the file size:*/
 
            // if (stat(disk_name, &file_stat) == 0) {
            //     printf("File size of disk %d: %ld bytes\n", sb->last_disk_index, file_stat.st_size);
            //     } else {
            //     perror("stat");
            //     }
            // disk_sz += file_stat.st_size;
        }
 
        // ---------------- count & store inodes ----------------
        if (strcmp(argv[i], "-i") == 0)
        {
            sb->num_inodes = atoi(argv[i + 1]);
        }
 
        // ---------------- count & store data blocks ----------------
        if (strcmp(argv[i], "-b") == 0)
        {
            sb->num_data_blocks = atoi(argv[i + 1]);
        }
    }
 
    // ############### check for valid command line arguments ###############
 
    if (strcmp(sb->raid_mode, "-1") == 0)
    {
        printf("Error: No raid mode specified.");
        return -1;
    }
    else
    {
        printf("The raid mode is %s\n", sb->raid_mode);
    }
 
    if (sb->num_data_blocks == 0)
    {
        printf("Error: No data blocks specified.");
        return -1;
    }
    else
    {
        printf("Number of data blocks is %d\n", (int)sb->num_data_blocks);
    }
 
    if (sb->num_inodes == 0)
    {
        printf("Error: No inodes specified.");
        return -1;
    }
    else
    {
        printf("Number of inodes is %d\n", (int)sb->num_inodes);
    }
 
    printf("No. of disks is: %d\n", sb->last_disk_index);
 
    // convert num_data_blocks to closest multiple of 32
    if (sb->num_data_blocks % 32 != 0)
    {
        sb->num_data_blocks = 32 * (sb->num_data_blocks / 32) + 32;
    }
 
    for (int i = 0; i < 10; i++)
    {
        /*valid fd is non-zero*/
        printf("%d\n", sb->fd_disk[i]);
    }
 
    /*****************************************
     Check1: file size > number of blocks ?
     *****************************************/
 
    int total_block_size = sb->num_data_blocks * BLOCK_SIZE;
    if (stat(disk_name, &file_stat) == 0)
    {
        printf("File size of disk %d: %ld bytes\n", sb->last_disk_index, file_stat.st_size);
    }
    else
    {
        perror("stat");
    }
    
    int disk_sz = file_stat.st_size * sb->last_disk_index; /*each disk is equal in size*/
    if (disk_sz < total_block_size)
        perror("Disk not enough");
    else
        printf("total disk size %d & total block size %d \n", disk_sz, total_block_size);
 
 
    /*INODE_BITMAP*/
    char* inode_bitmap = (char*)malloc(sb->num_inodes * sizeof(char));
    
    for(int i=0; i<sb->num_inodes; i++)
    {
        inode_bitmap[i] = 0;
    }
 
 
    /*DATA_BITMAP*/
    char* data_bitmap = (char*)malloc(sb->num_data_blocks * sizeof(char));
    
    for(int i=0; i<sb->num_data_blocks; i++)
    {
        data_bitmap[i] = 0;
    }
 
 
    // mmap the disk files
    for (int i = 0; i < sb->last_disk_index; i++)
    {
        int fd_curr = sb->fd_disk[i];
        // Create a memory-mapped region for the file.
        if (fd_curr != 0)
        {   
            printf("fd_curr: %d\n", fd_curr);
            sb->mmap_disk[i] = mmap(NULL, file_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_curr, 0);
            printf("addr received: %p\n", (void*)sb->mmap_disk[i]);
        }
        /* this copies the input file to the output file */
        printf("dest addr : %p\n, src addr : %p\n, size: %d\n", (void *)sb->mmap_disk[i], (void * )sb, (int)sizeof(struct wfs_sb));
        memcpy (sb->mmap_disk[i], sb, sizeof(struct wfs_sb));
        memcpy (sb->mmap_disk[i], inode_bitmap, (sb->num_inodes * sizeof(char)));
        memcpy (sb->mmap_disk[i], inode_bitmap, (sb->num_inodes * sizeof(char)));
    }
 
    
 
 
    return 0;
}
 
 
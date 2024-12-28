#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include "wfs.h"

// ############################################ Global Variables #####################################

// array of pointers to disk memory maps in the mkfs order
void *ordered_disk_mmap_ptr[10] = {NULL};

// global variable to store number of disks in wfs
int cnt_disks = 0;

int raid_mode = -1;

// ######################################### memory map functions #########################################

// function to populate array mmap pointers
void create_disk_mmap(char **disk_name, int disk_cnt, void **disk_mmap_ptr, int disk_size, int disk_fd[])
{
    // printf("test 1 \n");
    // open files & create mmap pointers
    for (int i = 0; i < disk_cnt; i++)
    {
        // printf("does he know\n");
        int fd = open(disk_name[i], O_RDWR, 0777);
        disk_fd[i] = fd;
        disk_mmap_ptr[i] = mmap(NULL, disk_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    }
}

/************************************************
Function to free memory maps & close file pointers
*************************************************/
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

// function that provides array of correctly ordered disk mmaps
void reorder_disk_mmap(int disk_cnt, void **disk_mmap_ptr, void **ordered_disk_mmap_ptr)
{
    for (int i = 0; i < disk_cnt; i++)
    {
        struct wfs_sb *sb = (struct wfs_sb *)disk_mmap_ptr[i];
        ordered_disk_mmap_ptr[sb->disk_order] = disk_mmap_ptr[i];
    }
    printf("reorder disk map done\n");
}

// ########################################### Helper functions ##########################################

// returns RAID mode
int get_raid_mode(void *disk_mmap_ptr)
{
    struct wfs_sb *sb = (struct wfs_sb *)disk_mmap_ptr;
    printf("RAID%d\n", sb->raid_mode);
    return sb->raid_mode;
}

// returns count of tokens within "path"
// populates array of tokens using strdup()
int path_parse(char *str, char **arg_arr, char *delims)
{
    printf("path parse called\n");
    int arg_cnt = 0; // cnt of the number of tokens
    // using strtok()
    // Returns pointer to first token
    char *token = strtok(str, delims);

    // Keep counting tokens while one of the
    // delimiters present in str[].
    while (token != NULL)
    {
        // printf(" % s\n", token);
        arg_cnt++;
        arg_arr[arg_cnt - 1] = strdup(token);
        token = strtok(NULL, delims);
    }

    printf("tokens found = %d\n", arg_cnt);
    return arg_cnt;
}

char *get_name_from_path(const char *path)
{
    char *copy_path = strdup(path);
    char *token_arr[10]; // Assuming that path won't have more than 10 tokens
    int token_cnt = 0;
    token_cnt = path_parse(copy_path, token_arr, "/");
    return token_arr[token_cnt - 1];
}

// ################################################ Inode Helpers ################################################

/****************************************
checks the inode bitmap of the given disk
returns the next empty inode index
else returns -1
****************************************/

int get_next_inode_index(int disk_num)
{
    struct wfs_sb *sb = (struct wfs_sb *)ordered_disk_mmap_ptr[disk_num];

    // check sb->i_bitmap for empty spot
    // Change type of pointer to char to make it byte addressable
    char *base = (void *)ordered_disk_mmap_ptr[disk_num];

    int inode_number = -1;
    __u_int *i_bitmap = (__u_int *)(base + sb->i_bitmap_ptr);

    for (int i = 0; i < sb->num_inodes / 32; i++)
    {
        uint32_t mask = 1;
        for (int j = 0; j < 32; j++)
        {
            if ((i_bitmap[i] & mask) == 0)
            {
                // set the inode bitmap
                i_bitmap[i] |= mask;

                // return the inode number
                inode_number = i * 32 + j;
                return inode_number;
            }
            mask = mask << 1;
        }
    }
    return inode_number;
}

/****************************************
sets the inode bitmap index for all disk
can set or reset based on the arguments passed
*****************************************/
void set_inode_index(int inode_number, uint32_t given_mask)
{
    // if(inode_number == 0)
    //     return;
    for (int i = 0; i < cnt_disks; i++)
    {
        struct wfs_sb *sb = (struct wfs_sb *)ordered_disk_mmap_ptr[i];
        char *base = (void *)ordered_disk_mmap_ptr[i];
        __u_int *i_bitmap = (__u_int *)(base + sb->i_bitmap_ptr);

        int row = inode_number / 32;
        int col = inode_number % 32;

        u_int32_t mask = 1;

        for (int i = 0; i < col; i++)
        {
            mask = mask << 1;
        }
        // set the inode bitmap
        if (given_mask == 1) // on given_mask=0 bit-wise OR won't work to reset the i-bitmap
            i_bitmap[row] |= mask;
        else
            i_bitmap[row] &= ~mask;
    }
}

/*************************************
// Returns pointer to inode based on
// 1. given inode_num
// 2. given disk
**************************************/
struct wfs_inode *get_inode_ptr(int inode_num, int disk_num)
{
    struct wfs_sb *sb = (struct wfs_sb *)ordered_disk_mmap_ptr[disk_num];
    char *base = (void *)ordered_disk_mmap_ptr[disk_num];

    struct wfs_inode *curr_inode = (struct wfs_inode *)(base + sb->i_blocks_ptr + inode_num * BLOCK_SIZE);
    return curr_inode;
}

// ################################################ d-block helpers ################################################

// function to return the next free d-block index if available
int get_free_d_block_index(int disk_num)
{
    struct wfs_sb *sb = (struct wfs_sb *)ordered_disk_mmap_ptr[disk_num];

    // check sb->i_bitmap for empty spot
    // Change type of pointer to char to make it byte addressable
    char *base = (void *)ordered_disk_mmap_ptr[disk_num];

    int data_block_number = -1;
    __u_int *d_bitmap = (__u_int *)(base + sb->d_bitmap_ptr);

    for (int i = 0; i < sb->num_data_blocks / 32; i++)
    {
        uint32_t mask = 1;
        for (int j = 0; j < 32; j++)
        {
            if ((d_bitmap[i] & mask) == 0)
            {
                // set the inode bitmap
                // d_bitmap[i] |= mask;

                // return the inode number
                data_block_number = i * 32 + j;
                return data_block_number;
            }
            mask = mask << 1;
        }
    }
    return data_block_number;
}

/*****************
sets the given data bitmap index to the given mask for all disks
****************/
void set_data_bmp_index(int data_block_number, uint32_t given_mask, int disk_num)
{
    // for (int i = 0; i < cnt_disks; i++)
    // {
    struct wfs_sb *sb = (struct wfs_sb *)ordered_disk_mmap_ptr[disk_num];
    char *base = (void *)sb;
    __u_int *d_bitmap = (__u_int *)(base + sb->d_bitmap_ptr);

    int row = data_block_number / 32;
    int col = data_block_number % 32;

    __u_int mask = mask = 1;

    for (int i = 0; i < col; i++)
    {
        mask = mask << 1;
    }
    // set the inode bitmap
    if (given_mask == 1) // on given_mask=0 bit-wise OR won't work to reset the i-bitmap
        d_bitmap[row] |= mask;
    else
        d_bitmap[row] &= ~mask;
    // }
}

/*****************************************
 Function to set the d_block ptr:
 index of d_block based on data bitmap;
 RAID;
 pointer to set;
 *****************************************/
void *get_d_block_ptr(int d_block_index, int disk_num)
{
    void *d_block_ptr = NULL;

    // if (raid_mode == 0)
    // {
    //     disk_num = d_block_index % cnt_disks;
    // }

    // point to the sb of the correct disk (matters for RAID 0)
    struct wfs_sb *sb = (struct wfs_sb *)ordered_disk_mmap_ptr[disk_num];
    char *base = (void *)sb;
    // if (raid_mode == 0)
    // {
    //     // point to the d_block of the first block of current inode passed
    //     d_block_ptr = (void *)(base + sb->d_blocks_ptr + (d_block_index / cnt_disks) * BLOCK_SIZE);

    // } // divided across disks
    // else
    // {
    // all blocks present in the each disks
    d_block_ptr = (void *)(base + sb->d_blocks_ptr + d_block_index * BLOCK_SIZE);
    // }
    return d_block_ptr;
}

/**************************
returns 1 if new data block should be allocated to an inode
only for the direct blocks
*****************************/
int alloc_d_block_to_dir(int inode_num)
{
    struct wfs_inode *inode_ptr = get_inode_ptr(inode_num, 0);

    // loop over all indexes in blocks
    int cnt_allocated_blocks = 0;
    for (int i = 0; i < 7; i++)
    {
        if (inode_ptr->blocks[i] != -1)
            cnt_allocated_blocks++;
    }

    // if(!(inode_ptr->size < (cnt_allocated_blocks*BLOCK_SIZE)))
    // {
    //     return 1;
    // }

    if (((inode_ptr->size == 0) && (inode_ptr->blocks[0] == -1)) ||
        ((inode_ptr->size % BLOCK_SIZE == 0) && (inode_ptr->size / BLOCK_SIZE != 7)) ||
        (!(inode_ptr->size < (cnt_allocated_blocks * BLOCK_SIZE))))
    {
        return 1;
    }
    // else if(inode_ptr->size/BLOCK_SIZE == 7){
    //     return -1;
    // }
    else
    {
        return 0;
    }
}

/*************
allocates a new data-block to the given inode
updates the inode blocks array on all disks
argument disk_num included to add raid0 support later
*************** */
int allocate_direct_block(int inode_num, int blocks_index, int disk_num)
{
    int d_block_index = get_free_d_block_index(disk_num);

    // check : data bitmap full
    if (d_block_index == -1)
    {
        return -1;
    }

    // set the data bitmap

    if (raid_mode == 0)
    {
        set_data_bmp_index(d_block_index, 1, disk_num);

        // update the inode
        if (blocks_index <= 6)
        {
            for (int i = 0; i < cnt_disks; i++)
            {
                struct wfs_inode *inode_ptr = get_inode_ptr(inode_num, i);
                inode_ptr->blocks[blocks_index] = d_block_index;
                // increment size, reason : new d-block allocated
                // inode_ptr->size += BLOCK_SIZE;
            }
        }
        else
        {
            struct wfs_inode *inode_ptr = get_inode_ptr(inode_num, disk_num);
            int indirect_block_index = inode_ptr->blocks[7];
            int indirect_block_disk = 7 % cnt_disks; // disk that holds the indirect block
            off_t *indirect_block_ptr = (off_t *)get_d_block_ptr(indirect_block_index, indirect_block_disk);
            int index_in_indirect_block = blocks_index - 7;
            indirect_block_ptr[index_in_indirect_block] = d_block_index;
        }
    }

    // raid1
    else
    {
        for (int i = 0; i < cnt_disks; i++)
        {
            set_data_bmp_index(d_block_index, 1, i);

            struct wfs_inode *inode_ptr = get_inode_ptr(inode_num, i);
            // put the newly allocated data-block into blocks array
            if (blocks_index <= 6)
                inode_ptr->blocks[blocks_index] = d_block_index;

            else
            {
                int indirect_block_index = inode_ptr->blocks[7];
                off_t *indirect_block_ptr = (off_t *)get_d_block_ptr(indirect_block_index, i);
                int index_in_indirect_block = blocks_index - 7;
                indirect_block_ptr[index_in_indirect_block] = d_block_index;
            }
            // increment size, reason : new d-block allocated
            // inode_ptr->size += BLOCK_SIZE;
        }
    }
    return d_block_index;
}

int allocate_indirect_block(int inode_num, int disk_num)
{
    int d_block_index = get_free_d_block_index(disk_num);

    // check : data bitmap full
    if (d_block_index == -1)
    {
        return -1;
    }

    if (raid_mode == 0)
    {
        set_data_bmp_index(d_block_index, 1, disk_num);
        memset(get_d_block_ptr(d_block_index, disk_num), -1, BLOCK_SIZE * (sizeof(char)));
        for (int i = 0; i < cnt_disks; i++)
        {
            // set_data_bmp_index(d_block_index, 1, i);

            struct wfs_inode *inode_ptr = get_inode_ptr(inode_num, i);
            // put the newly allocated data-block into blocks array
            inode_ptr->blocks[7] = d_block_index;

            // increment size, reason : new d-block allocated
            // inode_ptr->size += BLOCK_SIZE;
        }
    }

    // set the data bitmap
    // raid1
    else
    {
        for (int i = 0; i < cnt_disks; i++)
        {
            set_data_bmp_index(d_block_index, 1, i);

            struct wfs_inode *inode_ptr = get_inode_ptr(inode_num, i);
            // put the newly allocated data-block into blocks array
            inode_ptr->blocks[7] = d_block_index;
            memset(get_d_block_ptr(d_block_index, i), -1, BLOCK_SIZE * (sizeof(char)));
            // inode_ptr->size += BLOCK_SIZE;

            // increment size, reason : new d-block allocated
            // inode_ptr->size += BLOCK_SIZE;
        }
    }
    return d_block_index;
}

/*********************
return dentry pointer to the next empty slot or the the last filled slot
based on value of dcr
***********************/
struct wfs_dentry *get_dentry_ptr(int inode_num, int disk_num, int dcr)
{
    struct wfs_inode *inode_ptr = get_inode_ptr(inode_num, disk_num);

    int index_in_blocks = inode_ptr->size / BLOCK_SIZE;
    int offset = inode_ptr->size % BLOCK_SIZE;

    int d_block_index = inode_ptr->blocks[index_in_blocks];
    char *dentry_ptr = (char *)get_d_block_ptr(d_block_index, disk_num);
    return (struct wfs_dentry *)(dentry_ptr + offset - (dcr * sizeof(struct wfs_dentry)));
}

void remove_dentry_block(int inode_num)
{
    int d_block_index = -1;
    int index_in_blocks = -1;
    for (int i = 0; i < cnt_disks; i++)
    {
        struct wfs_inode *inode_ptr = get_inode_ptr(inode_num, i);
        // for(int j=1; j<7; j++)
        for (int j = 1; j < 7; j++)
        {
            if (inode_ptr->blocks[j] == -1)
            {
                index_in_blocks = j - 1;
                if (index_in_blocks != 0)
                {
                    d_block_index = inode_ptr->blocks[index_in_blocks];
                    inode_ptr->blocks[index_in_blocks] = -1;
                }
                break;
            }
        }
    }
    if (index_in_blocks != 0)
    {
        if (raid_mode == 0)
        {
            set_data_bmp_index(d_block_index, 0, index_in_blocks % cnt_disks);
        }
        else
        {
            for (int i = 0; i < cnt_disks; i++)
            {
                set_data_bmp_index(d_block_index, 0, i);
            }
        }
    }
}

/******************
// ONLY called in read for RAID1V
returns the disk which has the data-block with the correct data
***********************/
int get_correct_disk_num(int d_block_index, int read_size)
{
    int maxcount = 0;
    int disk_num_having_max_freq = 0;
    for (int i = 0; i < cnt_disks; i++)
    {
        int count = 0;
        char *curr_disk_d_block_ptr = (char *)get_d_block_ptr(d_block_index, i);
        for (int j = 0; j < cnt_disks; j++)
        {
            char *next_disk_d_block_ptr = (char *)get_d_block_ptr(d_block_index, j);
            if (memcmp(curr_disk_d_block_ptr, next_disk_d_block_ptr, read_size) == 0)
                count++;
        }

        if (count > maxcount)
        {
            maxcount = count;
            disk_num_having_max_freq = i;
        }
    }
    return disk_num_having_max_freq;
}

// ###################################### Traversal ######################################

/********************************************************
Function to find the inode number of the next element in path
Takes in inode number of the parent directory & name of the
directory to find
return inode number of the directory if found, else -1
**********************************************************/
int get_child_inode_num(int inode_num, char *child_name)
{
    printf("get_child_inode_num() called on inode_num %d & child_name %s\n", inode_num, child_name);
    // ---- step-1 : get the inode pointer ----
    struct wfs_inode *curr_inode = get_inode_ptr(inode_num, 0);

    if (curr_inode->size == 0)
        return -1;

    // ---- step-2 : Search dirents on each data block ----
    // int search_size = 0; // store the bytes of data that has been searched
    for (int i = 0; i < 7; i++)
    {
        int d_block_index = curr_inode->blocks[i];
        if (d_block_index == -1)
            return -1;
        int disk_num = (raid_mode == 0) ? i % cnt_disks : 0;
        void *d_block_ptr = get_d_block_ptr(d_block_index, disk_num);

        // check the 16 directory entries in d-block
        // since each dentry is 32B and each d-block is 512B
        struct wfs_dentry *dentry_ptr = (struct wfs_dentry *)d_block_ptr;
        for (int j = 0; j < 16; j++)
        {
            printf("i = %d & j = %d\n", i, j);
            if (strcmp(child_name, dentry_ptr->name) == 0)
            {
                // if match, then return the next inode block index;
                return dentry_ptr->num;
            }
            dentry_ptr = dentry_ptr + 1;
            // search_size += sizeof(struct wfs_dentry);
            // if (search_size >= curr_inode->size)
            //     return -1;
        }
    }

    // search the indirect blocks
    return -1;
}

/*
Returns the inode_num of the last element in path

*/
int path_traversal(const char *path, int token_cnt_dcr)
{
    char *copy_path = strdup(path);
    char *token_arr[10]; // Assuming that path won't have more than 10 tokens
    int token_cnt = 0;
    token_cnt = path_parse(copy_path, token_arr, "/");

    // Algorithm to determine inode
    int inode_num = 0;
    int next_inode_num = 0;
    int i = 0;
    for (i = 0; i < (token_cnt - token_cnt_dcr); i++)
    {
        // find the inode number of the child
        next_inode_num = get_child_inode_num(inode_num, token_arr[i]);
        if (next_inode_num == -1)
        {
            return -1;
        }
        else
        {
            inode_num = next_inode_num;
        }
    }
    return inode_num;
}

// ###################################### call-back functions ######################################

static int wfs_getattr(const char *path, struct stat *stbuf)
{
    printf("wfs_getattr() called on %s\n", path);

    // return code
    int res = 0;

    // corner case : path = "/"
    if (strcmp(path, "/") == 0)
    {
        struct wfs_inode *root_inode = get_inode_ptr(0, 0);
        memset(stbuf, 0, sizeof(struct stat));
        stbuf->st_uid = root_inode->uid;
        stbuf->st_gid = root_inode->gid;
        stbuf->st_atime = root_inode->atim;
        stbuf->st_mtime = root_inode->mtim;
        stbuf->st_mode = root_inode->mode;
        stbuf->st_size = root_inode->size;
        printf("root inode size = %d\n", (int)root_inode->size);
        return res;
    }

    // ---------------------- Path Parse -------------------------------
    int inode_num = path_traversal(path, 0);

    if (inode_num == -1)
    {
        res = -ENOENT;
        return res;
    }

    // get the inode pointer
    struct wfs_inode *curr_inode = get_inode_ptr(inode_num, 0);

    memset(stbuf, 0, sizeof(struct stat));
    stbuf->st_uid = curr_inode->uid;
    stbuf->st_gid = curr_inode->gid;
    stbuf->st_atime = time(NULL);
    stbuf->st_mtime = curr_inode->mtim;
    stbuf->st_mode = curr_inode->mode;
    stbuf->st_size = curr_inode->size;

    printf("returning from wfs_getattr\n");

    return res; // Return 0 on success
}

/********************
1. allocate a new inode using the inode bitmap
2. add a new directory entry to the parent inode
******************** */
static int wfs_mkdir(const char *path, mode_t mode)
{
    printf("wfs_mkdir called on %s\n", path);
    int res = 0;

    // check : file exists
    if (path_traversal(path, 0) != -1)
    {
        res = -EEXIST;
        return res;
    }

    // if(mode == (S_IFDIR | 0755))
    //     printf("same mode\n");
    // else
    //     printf("mode is %d\n", (int)mode);

    // get : parent inode number
    int parent_inode_num = path_traversal(path, 1);
    printf("parent inode = %d\n", parent_inode_num);

    // get : next empty inode bitmap index
    int inode_bmp_idx = get_next_inode_index(0);
    printf("inode_bitmap_index = %d\n", inode_bmp_idx);

    // check : inode bitmap full
    if (inode_bmp_idx == -1)
    {
        res = -ENOSPC;
        return res;
    }

    // set the inode bit map
    set_inode_index(inode_bmp_idx, 1);

    // calculate time
    time_t seconds;
    seconds = time(NULL);

    uid_t process_uid = getuid();
    gid_t process_gid = getgid();

    // create : new inode on each disk
    for (int i = 0; i < cnt_disks; i++)
    {
        struct wfs_inode *curr_inode = get_inode_ptr(inode_bmp_idx, i);
        curr_inode->num = inode_bmp_idx;
        // curr_inode->mode = S_IFDIR | 0755;
        curr_inode->mode = mode | S_IFDIR;
        curr_inode->uid = process_uid;
        curr_inode->gid = process_gid;
        curr_inode->size = 0;
        curr_inode->nlinks = 2;
        curr_inode->atim = seconds;
        curr_inode->mtim = seconds;
        curr_inode->ctim = seconds;
        memset(curr_inode->blocks, -1, N_BLOCKS * (sizeof(off_t)));
    }
    printf("Inode created for the new directory\n");

    // check : parent inode needs new data-block to hold new dentry
    int d_block_index = -1;
    int disk_num = -1;
    int blocks_index = -1;

    // check : new d-block needed for creating a dentry
    if (alloc_d_block_to_dir(parent_inode_num))
    {
        // d_block_index = get_free_d_block_index(0);
        // int blocks_index = -1;

        struct wfs_inode *parent_inode = get_inode_ptr(parent_inode_num, 0);
        // find the first index in blocks array which is empty
        for (int i = 0; i < 7; i++)
        {
            if (parent_inode->blocks[i] == -1)
            {
                blocks_index = i;
                break;
            }
        }
        disk_num = blocks_index % cnt_disks;

        d_block_index = allocate_direct_block(parent_inode_num, blocks_index, disk_num);

        // check : data bitmap full
        if (d_block_index == -1)
        {
            set_inode_index(inode_bmp_idx, 0);
            res = -ENOSPC;
            return res;
        }

        // update : parent inode
        // for (int i = 0; i < cnt_disks; i++)
        // {
        //     set_data_bmp_index(d_block_index, 1, i);

        //     struct wfs_inode *parent_inode = get_inode_ptr(parent_inode_num, i);
        //     int index_in_blocks = parent_inode->size / BLOCK_SIZE;
        //     parent_inode->blocks[index_in_blocks] = d_block_index;
        //     printf("parent inode updated\n");
        // }
    }
    else
    {
        // check : Parent data blocks full
        if (get_inode_ptr(parent_inode_num, 0)->size / BLOCK_SIZE == 7)
        {
            set_inode_index(inode_bmp_idx, 0);
            res = -ENOSPC;
            return res;
        }

        // you didn't allocate a new data block for the dentry
        // so you need the last data block which is half-filled to put the new dentry
        struct wfs_inode *parent_inode = get_inode_ptr(parent_inode_num, 0);
        for (int i = 0; i < 7; i++)
        {
            if (parent_inode->blocks[i] == -1)
            {
                blocks_index = i - 1;
                break;
            }
        }
        disk_num = blocks_index % cnt_disks;
    }

    // create : dentry in the parent
    if (raid_mode == 0)
    {
        struct wfs_dentry *dentry = get_dentry_ptr(parent_inode_num, disk_num, 0);
        strcpy(dentry->name, get_name_from_path(path));
        dentry->num = inode_bmp_idx;
    }
    else
    {
        for (int i = 0; i < cnt_disks; i++)
        {
            struct wfs_dentry *dentry = get_dentry_ptr(parent_inode_num, i, 0);
            strcpy(dentry->name, get_name_from_path(path));
            dentry->num = inode_bmp_idx;
        }
    }

    // update : parent inode
    seconds = time(NULL);
    for (int i = 0; i < cnt_disks; i++)
    {
        struct wfs_inode *parent_inode = get_inode_ptr(parent_inode_num, i);
        parent_inode->size += sizeof(struct wfs_dentry);
        parent_inode->mtim = seconds;
        parent_inode->ctim = seconds;
        parent_inode->atim = seconds;
        parent_inode->nlinks++;
        printf("parent inode updated size of parent = %d\n", (int)parent_inode->size);
    }

    return res;
}

static int wfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
    printf("wfs_mknod called on %s\n", path);
    int res = 0;

    // check : file exists
    if (path_traversal(path, 0) != -1)
    {
        res = -EEXIST;
        return res;
    }

    // get : parent inode number
    int parent_inode_num = path_traversal(path, 1);
    printf("parent inode = %d\n", parent_inode_num);

    // get : next empty inode bitmap index
    int inode_bmp_idx = get_next_inode_index(0);
    printf("inode_bitmap_index = %d\n", inode_bmp_idx);

    // check : inode bitmap full
    if (inode_bmp_idx == -1)
    {
        res = -ENOSPC;
        return res;
    }

    // set the inode bit map
    set_inode_index(inode_bmp_idx, 1);

    // calculate time
    time_t seconds;
    seconds = time(NULL);

    uid_t process_uid = getuid();
    gid_t process_gid = getgid();

    // create : new inode on each disk
    for (int i = 0; i < cnt_disks; i++)
    {
        struct wfs_inode *curr_inode = get_inode_ptr(inode_bmp_idx, i);
        curr_inode->num = inode_bmp_idx;
        curr_inode->mode = mode | S_IFREG;
        curr_inode->uid = process_uid;
        curr_inode->gid = process_gid;
        curr_inode->size = 0;
        curr_inode->nlinks = 1;
        curr_inode->atim = seconds;
        curr_inode->mtim = seconds;
        curr_inode->ctim = seconds;
        memset(curr_inode->blocks, -1, N_BLOCKS * (sizeof(off_t)));
    }
    printf("Inode created for the new file\n");

    // check : parent inode needs new data-block to hold new dentry
    int d_block_index = -1;
    int disk_num = -1;
    int blocks_index = -1;
    if (alloc_d_block_to_dir(parent_inode_num))
    {
        printf("new data block allocated for dentries\n");
        struct wfs_inode *parent_inode = get_inode_ptr(parent_inode_num, 0);
        for (int i = 0; i < 7; i++)
        {
            if (parent_inode->blocks[i] == -1)
            {
                blocks_index = i;
                break;
            }
        }
        disk_num = blocks_index % cnt_disks;

        d_block_index = allocate_direct_block(parent_inode_num, blocks_index, disk_num);

        // check : data bitmap full
        if (d_block_index == -1)
        {
            set_inode_index(inode_bmp_idx, 0);
            res = -ENOSPC;
            return res;
        }
        // d_block_index = get_free_d_block_index(0);

        // // check : data bitmap full
        // if (d_block_index == -1)
        // {
        //     set_inode_index(inode_bmp_idx, 0);
        //     res = -ENOSPC;
        //     return res;
        // }

        // // update : parent inode
        // for (int i = 0; i < cnt_disks; i++)
        // {
        //     set_data_bmp_index(d_block_index, 1, i);

        //     struct wfs_inode *parent_inode = get_inode_ptr(parent_inode_num, i);
        //     int index_in_blocks = parent_inode->size / BLOCK_SIZE;
        //     parent_inode->blocks[index_in_blocks] = d_block_index;
        //     printf("parent inode updated\n");
        // }
    }
    else
    {
        // check : Parent data blocks full
        if (get_inode_ptr(parent_inode_num, 0)->size / BLOCK_SIZE == 7)
        {
            set_inode_index(inode_bmp_idx, 0);
            res = -ENOSPC;
            return res;
        }
        struct wfs_inode *parent_inode = get_inode_ptr(parent_inode_num, 0);
        for (int i = 0; i < 7; i++)
        {
            if (parent_inode->blocks[i] == -1)
            {
                blocks_index = i - 1;
                break;
            }
        }
        disk_num = blocks_index % cnt_disks;
    }

    // create : dentry in the parent
    if (raid_mode == 0)
    {
        struct wfs_dentry *dentry = get_dentry_ptr(parent_inode_num, disk_num, 0);
        strcpy(dentry->name, get_name_from_path(path));
        dentry->num = inode_bmp_idx;
    }
    else
    {
        for (int i = 0; i < cnt_disks; i++)
        {
            struct wfs_dentry *dentry = get_dentry_ptr(parent_inode_num, i, 0);
            strcpy(dentry->name, get_name_from_path(path));
            dentry->num = inode_bmp_idx;
        }
    }

    seconds = time(NULL);
    // update : parent inode
    for (int i = 0; i < cnt_disks; i++)
    {
        struct wfs_inode *parent_inode = get_inode_ptr(parent_inode_num, i);
        parent_inode->size += sizeof(struct wfs_dentry);
        // parent_inode->nlinks++;
        parent_inode->mtim = seconds;
        parent_inode->ctim = seconds;
        parent_inode->atim = seconds;
        printf("parent inode updated, size = %d\n", (int)parent_inode->size);
    }
    return res;
}

/****************
1. find the data block corresponding to the offset being written to
2. copy size bytes data from the write buffer into the data block(s)
3. writes may be split across data blocks or span multiple data-blocks
******************/
static int wfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("wfs_write called on %s\n", path);

    int res = 0;
    int copy_size = size;

    // check : file exists
    int inode_num = path_traversal(path, 0);
    if (inode_num == -1)
    {
        res = -ENOENT;
        return res;
    }
    printf("%s exists\n", path);

    // check : inode is a regular file
    struct wfs_inode *inode_ptr = get_inode_ptr(inode_num, 0);

    // determine : data block to be written
    int index_in_blocks = offset / BLOCK_SIZE;
    int offset_within_block = offset % BLOCK_SIZE;

    int d_block_index = -1;

    // cnt of the number of d-blocks to write
    int loop_cnt = (offset_within_block + size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // loop to write the blocks
    // 1. check if a data_block is allocated for the found index_in_blocks
    // 2. else allocate a page
    // 3. write to the correct offset_within_block until the end of the block or size
    // 4. repeat

    printf("Before the main for loop\n");

    for (int i = 0; i < loop_cnt; i++)
    {
        // flag : set if you allocate a new d-block to write
        // int wrote_on_a_new_block = 0;
        if (index_in_blocks < 7)
        {
            d_block_index = inode_ptr->blocks[index_in_blocks];
        }
        else
        {
            // check if blocks[7] is allocated
            int indirect_block_index = inode_ptr->blocks[7];
            // if no then allocate it first
            if (indirect_block_index == -1)
            {
                indirect_block_index = allocate_indirect_block(inode_num, 7 % cnt_disks);
                if (indirect_block_index == -1)
                {
                    res = -ENOSPC;
                    return res;
                }
            }
            // then find out the correct block in the new page
            off_t *indirect_block_ptr = (off_t *)get_d_block_ptr(indirect_block_index, 7 % cnt_disks);
            int index_in_indirect_block = index_in_blocks - 7;
            d_block_index = indirect_block_ptr[index_in_indirect_block];
        }

        // allocate a new page
        if (d_block_index == -1)
        {
            // allocate a page
            d_block_index = allocate_direct_block(inode_num, index_in_blocks, index_in_blocks % cnt_disks);

            // check : no space to allocate new data block
            if (d_block_index == -1)
            {
                res = -ENOSPC;
                return res;
            }
            // wrote_on_a_new_block = 1;
        }

        // for raid1

        // size to be written to the given d-block
        int write_size = 0;

        if (raid_mode == 0)
        {
            char *d_block_ptr = (char *)get_d_block_ptr(d_block_index, index_in_blocks % cnt_disks);
            d_block_ptr += offset_within_block;
            int space_in_d_block = 512 - offset_within_block;

            if (size > space_in_d_block)
            {
                write_size = space_in_d_block;
            }
            else
            {
                write_size = size;
            }
            memcpy(d_block_ptr, buf, write_size);

            // if (wrote_on_a_new_block)
            // {
            for (int j = 0; j < cnt_disks; j++)
            {
                inode_ptr = get_inode_ptr(inode_num, j);
                inode_ptr->size += write_size;
            }
            // }
        }
        else
        {
            for (int j = 0; j < cnt_disks; j++)
            {
                // get inode ptr for current disk
                inode_ptr = get_inode_ptr(inode_num, j);

                // put the newly allocated data-block into blocks array
                // inode_ptr->blocks[index_in_blocks] = d_block_index;

                // get pointer to the correct data-block
                char *d_block_ptr = (char *)get_d_block_ptr(d_block_index, j);
                d_block_ptr += offset_within_block;

                // the space in the chosen d-block given it has some data already on it
                int space_in_d_block = 512 - offset_within_block;

                if (size > space_in_d_block)
                {
                    write_size = space_in_d_block;
                }
                else
                {
                    write_size = size;
                }
                memcpy(d_block_ptr, buf, write_size);

                // ------------------------ handling overwriting ------------------------

                // printf("size being written %d\n", write_size);
                // if (offset < inode_ptr->size)
                // {   
                //     printf("if case in write\n");
                //     if (write_size + offset > inode_ptr->size)
                //     {
                //         // spill case
                //         printf("spill case\n");
                //         inode_ptr->size = (write_size + offset);
                //     }
                //     else
                //     {
                //         printf("no spill case\n");
                //          inode_ptr->size += write_size;
                //     }
                // }
                // else
                // {
                //     printf("else case in write\n");
                    inode_ptr->size += write_size;
                // }
                
            }
        }
        // if(wrote_on_a_new_block)
        // {
        //     ino
        // }
        buf += write_size;
        size -= write_size;
        index_in_blocks++;

        // austin
        offset_within_block = 0;
    }

    printf("after the main for loop\n");
    res = copy_size;
    return res;
}

/**********************************
1. free (unallocate) any data blocks associated with the file,
2. free it's inode,
3. remove the directory entry pointing to the file from the parent inode.
***********************************/

static int wfs_unlink(const char *path)
{
    printf("wfs_unlink called on %s\n", path);

    int res = 0;

    // get : parent inode number
    int parent_inode_num = path_traversal(path, 1);
    printf("parent inode = %d\n", parent_inode_num);

    // get : current inode number
    int curr_inode_num = path_traversal(path, 0);

    // get : current inode pointer
    struct wfs_inode *curr_inode = get_inode_ptr(curr_inode_num, 0);

    // -------------------------------------- free the d-block bitmap --------------------------------------
    if (curr_inode->size != 0)
    {
        // loop over all the indices in blocks array
        for (int i = 0; i < 8; i++)
        {
            // skip if blocks[i] == -1

            // free if blocks[i] != -1
            if (raid_mode == 0)
            {
                // get the inode pointer from the correct disk
                curr_inode = get_inode_ptr(curr_inode_num, i % cnt_disks);

                // handle indirect block
                if (i == 7 && curr_inode->blocks[7] != -1)
                {
                    int indirect_block_index = curr_inode->blocks[7];
                    off_t *indirect_block_ptr = (off_t *)get_d_block_ptr(indirect_block_index, 7 % cnt_disks);

                    // each indirect block is a d-block =512B
                    // each block_index entry in it is of type off_t = long (8B)
                    // total possible entries in an indirec t block is 512/8 = 64
                    for (int k = 0; k < BLOCK_SIZE / sizeof(off_t); k++)
                    {
                        if (indirect_block_ptr[k] == -1)
                        {
                            break;
                        }
                        set_data_bmp_index(indirect_block_ptr[k], 0, (k + 7) % cnt_disks);
                        indirect_block_ptr[k] = -1;
                    }
                }
                set_data_bmp_index(curr_inode->blocks[i], 0, i % cnt_disks);
                for (int j = 0; j < cnt_disks; j++)
                {
                    curr_inode = get_inode_ptr(curr_inode_num, j % cnt_disks);
                    curr_inode->blocks[i] = -1;
                }
            }
            else
            {
                // loop over all the disks
                for (int j = 0; j < cnt_disks; j++)
                {
                    curr_inode = get_inode_ptr(curr_inode_num, j);
                    // handle indirect blocks
                    if (i == 7)
                    {
                        int indirect_block_index = curr_inode->blocks[7];
                        off_t *indirect_block_ptr = (off_t *)get_d_block_ptr(indirect_block_index, j);

                        // each indirect block is a d-block =512B
                        // each block_index entry in it is of type off_t = long (8B)
                        // total possible entries in an indirec t block is 512/8 = 64
                        for (int k = 0; k < BLOCK_SIZE / sizeof(off_t); k++)
                        {
                            if (indirect_block_ptr[k] == -1)
                            {
                                break;
                            }
                            set_data_bmp_index(indirect_block_ptr[k], 0, j);
                            indirect_block_ptr[k] = -1;
                        }
                    }
                    set_data_bmp_index(curr_inode->blocks[i], 0, j);
                    curr_inode->blocks[i] = -1;
                }
            }
            curr_inode->blocks[i] = -1;
        }
    }

    // -------------------------------------- free inode --------------------------------------
    set_inode_index(curr_inode_num, 0);

    // -------------------------------------- remove the dentry --------------------------------------

    if (raid_mode == 0)
    {
        // get : pointer to parent inode
        struct wfs_inode *parent_inode_ptr = get_inode_ptr(parent_inode_num, 0);

        for (int i = 0; i < 7; i++)
        {
            int d_block_index = parent_inode_ptr->blocks[i];

            struct wfs_dentry *dentry_ptr = (struct wfs_dentry *)get_d_block_ptr(d_block_index, i % cnt_disks);
            // struct wfs_dentry *last_dentry_ptr = get_dentry_ptr(parent_inode_num, 0, 1);

            // loop over all possible dentrys in a d-block
            int j = 0;
            for (j = 0; j < BLOCK_SIZE / sizeof(struct wfs_dentry); j++)
            {
                // struct wfs_dentry *dentry_ptr = (struct wfs_dentry *)get_d_block_ptr(d_block_index, j);
                if (dentry_ptr->num == curr_inode_num)
                {
                    // for (int k = 0; k < cnt_disks; k++)
                    // {
                    // dentry_ptr = get_dentry_ptr(parent_inode_num, k, j);
                    int disk_num = 0;
                    for (int k = 1; k < 8; k++)
                    {
                        if (parent_inode_ptr->blocks[i] == -1)
                        {
                            disk_num = (i - 1) % cnt_disks;
                            break;
                        }
                    }

                    struct wfs_dentry *last_dentry_ptr = get_dentry_ptr(parent_inode_num, disk_num, 1);
                    strcpy(dentry_ptr->name, last_dentry_ptr->name);
                    dentry_ptr->num = last_dentry_ptr->num;
                    // parent_inode_ptr = get_inode_ptr(parent_inode_num, k);
                    for (int k = 0; k < cnt_disks; k++)
                    {
                        parent_inode_ptr = get_inode_ptr(parent_inode_num, k);
                        parent_inode_ptr->size -= sizeof(struct wfs_dentry);
                    }

                    memset(last_dentry_ptr, 0, sizeof(struct wfs_dentry));
                    // }
                    // return res;
                    break;
                }
                dentry_ptr += 1;
            }
            if (j < BLOCK_SIZE / sizeof(struct wfs_dentry))
                break;
        }
    }

    else
    {
        for (int k = 0; k < cnt_disks; k++)
        {
            struct wfs_inode *parent_inode_ptr = get_inode_ptr(parent_inode_num, k);

            // loop over the blocks array of the parent
            for (int i = 0; i < 7; i++)
            {
                int d_block_index = parent_inode_ptr->blocks[i];
                struct wfs_dentry *dentry_ptr = (struct wfs_dentry *)get_d_block_ptr(d_block_index, k);
                // struct wfs_dentry *last_dentry_ptr = get_dentry_ptr(parent_inode_num, 0, 1);

                // loop over all possible dentrys in a d-block
                int j = 0;
                for (j = 0; j < BLOCK_SIZE / sizeof(struct wfs_dentry); j++)
                {
                    // struct wfs_dentry *dentry_ptr = (struct wfs_dentry *)get_d_block_ptr(d_block_index, j);
                    if (dentry_ptr->num == curr_inode_num)
                    {
                        // for (int k = 0; k < cnt_disks; k++)
                        // {
                        // dentry_ptr = get_dentry_ptr(parent_inode_num, k, j);
                        struct wfs_dentry *last_dentry_ptr = get_dentry_ptr(parent_inode_num, k, 1);
                        strcpy(dentry_ptr->name, last_dentry_ptr->name);
                        dentry_ptr->num = last_dentry_ptr->num;
                        // parent_inode_ptr = get_inode_ptr(parent_inode_num, k);
                        parent_inode_ptr->size -= sizeof(struct wfs_dentry);
                        memset(last_dentry_ptr, 0, sizeof(struct wfs_dentry));
                        // }
                        // return res;
                        break;
                    }
                    dentry_ptr += 1;
                }
                if (j < BLOCK_SIZE / sizeof(struct wfs_dentry))
                    break;
            }
        }
    }

    // struct wfs_inode *parent_inode_ptr = get_inode_ptr(parent_inode_num, 0);
    // if (parent_inode_ptr->size % BLOCK_SIZE == 0)
    // {
    //     // remove the last d-block allocated for dentrys/ dentry block

    //     remove_dentry_block(parent_inode_num);
    // }

    // loop over the blocks array of the parent
    // for (int i = 0; i < 7; i++)
    // {
    //     int d_block_index = parent_inode_ptr->blocks[i];
    //     struct wfs_dentry *dentry_ptr = (struct wfs_dentry *)get_d_block_ptr(d_block_index, 0);
    //     // struct wfs_dentry *last_dentry_ptr = get_dentry_ptr(parent_inode_num, 0, 1);

    //     // loop over all possible dentrys in a d-block
    //     for(int j=0; j<BLOCK_SIZE/sizeof(struct wfs_dentry); j++)
    //     {
    //         struct wfs_dentry *dentry_ptr = (struct wfs_dentry *)get_d_block_ptr(d_block_index, j);
    //         if(dentry_ptr->num == curr_inode_num)
    //         {
    //             for(int k=0; k<cnt_disks; k++)
    //             {
    //                 // dentry_ptr = get_dentry_ptr(parent_inode_num, k, j);
    //                 struct wfs_dentry *last_dentry_ptr = get_dentry_ptr(parent_inode_num, 0, 1);
    //                 strcpy(dentry_ptr->name, last_dentry_ptr->name);
    //                 dentry_ptr->num = last_dentry_ptr->num;
    //                 parent_inode_ptr = get_inode_ptr(parent_inode_num, k);
    //                 parent_inode_ptr->size -= sizeof(struct wfs_dentry);
    //             }
    //             return res;
    //         }
    //         dentry_ptr += 1;
    //     }
    // }
    return res;
}

static int wfs_rmdir(const char *path)
{
    printf("wfs_rmdir called on %s\n", path);

    int res = 0;

    // get : parent inode number
    int parent_inode_num = path_traversal(path, 1);
    printf("parent inode = %d\n", parent_inode_num);

    // get : current inode number
    int curr_inode_num = path_traversal(path, 0);

    // get : current inode pointer
    struct wfs_inode *curr_inode = get_inode_ptr(curr_inode_num, 0);

    // rmdir should succeed only if the directory is empty
    if (curr_inode->size != 0)
    {
        res = -1;
        return res;
    }

    // -------------------------------------- free the d-block bitmap --------------------------------------
    if (curr_inode->size != 0)
    {
        for (int i = 0; i < 7; i++)
        {
            if (raid_mode == 0)
            {
                if (i == 7)
                {
                    int indirect_block_index = curr_inode->blocks[7];
                    off_t *indirect_block_ptr = (off_t *)get_d_block_ptr(indirect_block_index, 7 % cnt_disks);

                    // each indirect block is a d-block =512B
                    // each block_index entry in it is of type off_t = long (8B)
                    // total possible entries in an indirec t block is 512/8 = 64
                    for (int k = 0; k < BLOCK_SIZE / sizeof(off_t); k++)
                    {
                        if (indirect_block_ptr[k] == -1)
                        {
                            break;
                        }
                        set_data_bmp_index(indirect_block_ptr[k], 0, (k + 7) % cnt_disks);
                        indirect_block_ptr[k] = -1;
                    }
                }
                set_data_bmp_index(curr_inode->blocks[i], 0, i % cnt_disks);
            }
            else
            {
                for (int j = 0; j < cnt_disks; j++)
                {
                    curr_inode = get_inode_ptr(curr_inode_num, j);
                    if (i == 7)
                    {
                        int indirect_block_index = curr_inode->blocks[7];
                        off_t *indirect_block_ptr = (off_t *)get_d_block_ptr(indirect_block_index, j);

                        // each indirect block is a d-block =512B
                        // each block_index entry in it is of type off_t = long (8B)
                        // total possible entries in an indirec t block is 512/8 = 64
                        for (int k = 0; k < BLOCK_SIZE / sizeof(off_t); k++)
                        {
                            if (indirect_block_ptr[k] == -1)
                            {
                                break;
                            }
                            set_data_bmp_index(indirect_block_ptr[k], 0, j);
                            indirect_block_ptr[i] = -1;
                        }
                    }
                    set_data_bmp_index(curr_inode->blocks[i], 0, j);
                    curr_inode->blocks[i] = -1;
                }
            }
            curr_inode->blocks[i] = -1;
        }
    }

    // -------------------------------------- free inode --------------------------------------
    set_inode_index(curr_inode_num, 0);

    // -------------------------------------- remove the dentry --------------------------------------

    for (int k = 0; k < cnt_disks; k++)
    {
        struct wfs_inode *parent_inode_ptr = get_inode_ptr(parent_inode_num, k);

        // loop over the blocks array of the parent
        for (int i = 0; i < 7; i++)
        {
            int d_block_index = parent_inode_ptr->blocks[i];
            struct wfs_dentry *dentry_ptr = (struct wfs_dentry *)get_d_block_ptr(d_block_index, k);
            // struct wfs_dentry *last_dentry_ptr = get_dentry_ptr(parent_inode_num, 0, 1);

            // loop over all possible dentrys in a d-block
            int j = 0;
            for (j = 0; j < BLOCK_SIZE / sizeof(struct wfs_dentry); j++)
            {
                // struct wfs_dentry *dentry_ptr = (struct wfs_dentry *)get_d_block_ptr(d_block_index, j);
                if (dentry_ptr->num == curr_inode_num)
                {
                    // for (int k = 0; k < cnt_disks; k++)
                    // {
                    // dentry_ptr = get_dentry_ptr(parent_inode_num, k, j);
                    struct wfs_dentry *last_dentry_ptr = get_dentry_ptr(parent_inode_num, k, 1);
                    strcpy(dentry_ptr->name, last_dentry_ptr->name);
                    dentry_ptr->num = last_dentry_ptr->num;
                    // parent_inode_ptr = get_inode_ptr(parent_inode_num, k);
                    parent_inode_ptr->size -= sizeof(struct wfs_dentry);
                    memset(last_dentry_ptr, 0, sizeof(struct wfs_dentry));
                    // }
                    // return res;
                    break;
                }
                dentry_ptr += 1;
            }
            if (j < BLOCK_SIZE / sizeof(struct wfs_dentry))
                break;
        }
    }

    // loop over the blocks array of the parent
    // for (int i = 0; i < 7; i++)
    // {
    //     int d_block_index = parent_inode_ptr->blocks[i];
    //     struct wfs_dentry *dentry_ptr = (struct wfs_dentry *)get_d_block_ptr(d_block_index, 0);
    //     // struct wfs_dentry *last_dentry_ptr = get_dentry_ptr(parent_inode_num, 0, 1);

    //     // loop over all possible dentrys in a d-block
    //     for(int j=0; j<BLOCK_SIZE/sizeof(struct wfs_dentry); j++)
    //     {
    //         struct wfs_dentry *dentry_ptr = (struct wfs_dentry *)get_d_block_ptr(d_block_index, j);
    //         if(dentry_ptr->num == curr_inode_num)
    //         {
    //             for(int k=0; k<cnt_disks; k++)
    //             {
    //                 // dentry_ptr = get_dentry_ptr(parent_inode_num, k, j);
    //                 struct wfs_dentry *last_dentry_ptr = get_dentry_ptr(parent_inode_num, 0, 1);
    //                 strcpy(dentry_ptr->name, last_dentry_ptr->name);
    //                 dentry_ptr->num = last_dentry_ptr->num;
    //                 parent_inode_ptr = get_inode_ptr(parent_inode_num, k);
    //                 parent_inode_ptr->size -= sizeof(struct wfs_dentry);
    //             }
    //             return res;
    //         }
    //         dentry_ptr += 1;
    //     }
    // }
    return res;
}

/*******************************
1. find the data block corresponding to the offset being read from, and
2. copy data from the data block(s) to the read buffer.
3. As with writes, reads may be split across data blocks, or span multiple data blocks.
*******************************/
static int wfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("wfs_read called on %s\n", path);
    printf("offset is %d\n", (int)offset);

    int res = 0;

    // check : file exists
    int curr_inode_num = path_traversal(path, 0);

    // check : inode is a regular file
    struct wfs_inode *inode_ptr = get_inode_ptr(curr_inode_num, 0);

    int size_to_read = inode_ptr->size - offset + 1;
    if (size < size_to_read)
    {
        size_to_read = size;
    }

    int read_bytes = size_to_read;

    // check : offset is greater than size
    if (offset >= inode_ptr->size)
    {
        // return error
        res = 0;
        return res;
    }

    // determine : data block to be written
    int index_in_blocks = offset / BLOCK_SIZE;
    int offset_within_block = offset % BLOCK_SIZE;

    int d_block_index = -1;

    // cnt of the number of d-blocks to write
    int loop_cnt = (offset_within_block + size_to_read + BLOCK_SIZE - 1) / BLOCK_SIZE;

    printf("Before the main for loop\n");

    // loop for number of pages to be read
    for (int i = 0; (i < loop_cnt) && (size_to_read > 0); i++)
    {
        if (index_in_blocks < 7)
        {
            inode_ptr = get_inode_ptr(curr_inode_num, i % cnt_disks);
            d_block_index = inode_ptr->blocks[index_in_blocks];
        }
        else
        {
            // check if blocks[7] is allocated
            int indirect_block_index = inode_ptr->blocks[7];
            // check : indirect block is allocated
            // if (indirect_block_index == -1)
            // {
            // indirect_block_index = allocate_indirect_block(inode_num, 0);
            if (indirect_block_index == -1)
            {
                // return the size of bytes read
                res = read_bytes;
                return res;
            }
            // }
            // then find out the correct block in the new page
            off_t *indirect_block_ptr = (off_t *)get_d_block_ptr(indirect_block_index, 7 % cnt_disks);
            int index_in_indirect_block = index_in_blocks - 7;
            d_block_index = indirect_block_ptr[index_in_indirect_block];
        }

        if (d_block_index == -1)
        {
            // return the size of bytes read
            res = read_bytes;
            return res;
        }

        int read_size = 0;
        // for (int j = 0; j < cnt_disks; j++)
        // {
        // get inode ptr for current disk
        // inode_ptr = get_inode_ptr(inode_num, j);

        // put the newly allocated data-block into blocks array
        // inode_ptr->blocks[index_in_blocks] = d_block_index;

        // get pointer to the correct data-block
        char *d_block_ptr = (char *)get_d_block_ptr(d_block_index, index_in_blocks % cnt_disks);
        d_block_ptr += offset_within_block;

        // the space in the chosen d-block given it has some data already on it
        int space_in_d_block = BLOCK_SIZE - offset_within_block;

        if (size_to_read > space_in_d_block)
        {
            read_size = space_in_d_block;
        }
        else
        {
            read_size = size_to_read;
        }
        int disk_num = 0;
        if (raid_mode == 2)
        {
            disk_num = get_correct_disk_num(d_block_index, read_bytes);
            d_block_ptr = get_d_block_ptr(d_block_index, disk_num);
        }
        memcpy(buf, d_block_ptr, read_size);

        // // austin
        // offset_within_block = 0;
        // if (wrote_on_a_new_block)
        // {
        //     inode_ptr->size += write_size;
        // }
        // }
        // if(wrote_on_a_new_block)
        // {
        //     ino
        // }
        buf += read_size;
        size_to_read = size_to_read - read_size;
        index_in_blocks++;

        // austin
        offset_within_block = 0;
    }

    printf("after the main for loop\n");
    res = read_bytes;
    return res;
}

static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    printf("wfs_readdir called on %s\n", path);
    int res = 0;

    int inode_num = path_traversal(path, 0);

    struct wfs_inode *inode_ptr = get_inode_ptr(inode_num, 0);
    int size_read = 0;

    for (int i = 0; i < 7; i++)
    {
        int d_block_index = inode_ptr->blocks[i];
        if (d_block_index == -1)
            break;

        struct wfs_dentry *d_block_ptr = (struct wfs_dentry *)get_d_block_ptr(d_block_index, i % cnt_disks);
        for (int j = 0; j < BLOCK_SIZE / sizeof(struct wfs_dentry); j++)
        {
            if (size_read == inode_ptr->size)
            {
                // exit
                return res;
            }
            filler(buf, d_block_ptr->name, NULL, 0);
            d_block_ptr += 1;
            size_read += sizeof(struct wfs_dentry);
        }
    }

    return res;
}

static struct fuse_operations ops = {
    .getattr = wfs_getattr,
    .mknod = wfs_mknod,
    .mkdir = wfs_mkdir,
    .write = wfs_write,
    .unlink = wfs_unlink,
    .rmdir = wfs_rmdir,
    .read = wfs_read,
    .readdir = wfs_readdir,
};

int main(int argc, char *argv[])
{
    // ###################################### parse command line arguments ######################################
    // int raid_mode = -1;
    // int cnt_data_blocks = 0;
    // int cnt_inodes = 0;
    // int cnt_disks = 0;
    int disk_size = 0;
    char *disk_name[10] = {NULL};
    void *disk_mmap_ptr[10] = {NULL};
    int disk_fd[10] = {0};

    // assuming the order is maintained in the cmd-line args
    // ./wfs disk1 disk2 [FUSE options] mount_point
    int fuse_options_flag = 0; // flag indicates FUSE options have been parsed

    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "-s") == 0)
        {
            fuse_options_flag = 1;
        }

        if (fuse_options_flag == 0 && i != 0)
        {
            disk_name[cnt_disks] = argv[i];
            printf("%s\n", disk_name[cnt_disks]);
            cnt_disks++;
        }
    }

    // #################################### Validate the cmd line args ####################################

    if (cnt_disks == 0)
    {
        return -1;
    }

    struct stat file_stat;
    // long disk_size = 0;
    if (stat(disk_name[0], &file_stat) == 0)
    {
        disk_size = file_stat.st_size;
    }
    else
    {
        perror("stat");
    }

    // --------------------------------- mmap disks & reorder them ---------------------------------

    create_disk_mmap(disk_name, cnt_disks, disk_mmap_ptr, disk_size, disk_fd);

    struct wfs_sb *sb = (struct wfs_sb *)disk_mmap_ptr[0];
    if (sb->total_disks != cnt_disks)
    {
        printf("Error: not enough disks.\n");
        remove_disk_mmap(cnt_disks, disk_size, disk_fd, disk_mmap_ptr);
        return -1;
    }
    raid_mode = sb->raid_mode;
    reorder_disk_mmap(cnt_disks, disk_mmap_ptr, ordered_disk_mmap_ptr);

    raid_mode = get_raid_mode(ordered_disk_mmap_ptr[0]);

    // #################################### modify argc & argv ########################################

    // decrement argc
    argc = argc - cnt_disks - 1;
    // printf("%d\n", argc);

    // increment argv
    for (int i = 0; i < (cnt_disks + 1); i++)
    {
        argv++;
    }

    for (int i = 0; i < argc; i++)
    {
        printf("%s\n", argv[i]);
    }

    // return 0;

    // ######################################## call fuse_main ########################################

    printf("started\n");
    // Initialize FUSE with specified operations
    // Filter argc and argv here and then pass it to fuse_main
    return fuse_main(argc, argv, &ops, NULL);
}

#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "wmap.h" // p5

struct file
{
  enum
  {
    FD_NONE,
    FD_PIPE,
    FD_INODE
  } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe;
  struct inode *ip;
  uint off;
};

int sys_fork(void)
{
  return fork();
}

int sys_exit(void)
{
  exit();
  return 0; // not reached
}

int sys_wait(void)
{
  return wait();
}

int sys_kill(void)
{
  int pid;

  if (argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int sys_getpid(void)
{
  return myproc()->pid;
}

int sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

int sys_sleep(void)
{
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (myproc()->killed)
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// ########################### p5 ###########################

// to request physical memory pages
int sys_wmap(void)
{
  // code here
  int start_va;   // starting virtual address requested by the user
  int mem_length; // size of memory requested by user
  int flags_val;  // MAP_FIXED | MAP_SHARED | MAP_ANONYMOUS
  int fd;         // file descriptor

  // Extract arguments for sys_call
  // Check : valid arguments provided
  if (argint(0, &start_va) < 0 || argint(1, &mem_length) < 0 || argint(2, &flags_val) < 0 || argint(3, &fd) < 0)
  {
    return FAILED;
  }

  // check : maximum number of memory maps per process = 16
  if (myproc()->num_maps >= 16)
  {
    return FAILED;
  }

  // check : virtual address is a multiple of PGSIZE(4096)
  if (start_va % PGSIZE != 0)
  {
    // cprintf("debug\n");
    return FAILED;
  }

  // check : virtual address belongs [0x60000000, 0x80000000)
  if (!(start_va >= 0x60000000 && start_va < 0x80000000))
  {
    return FAILED;
  }

  // last va of the user-requested wmap
  int end_va = start_va + mem_length;

  // check : overlapping maps
  for (int i = 0; i < 16; i++)
  {
    if (myproc()->mapinfo[i].start_addr != -1)
    {
      int wmap_start = myproc()->mapinfo[i].start_addr;
      int wmap_end = myproc()->mapinfo[i].start_addr + myproc()->mapinfo[i].map_length;

      // new wmap starts within an allocated wmap
      if (start_va >= wmap_start && start_va < wmap_end)
      {
        // cprintf("debug1\n");
        return FAILED;
      }

      // new wmap ends within an allocated wmap
      if (end_va >= wmap_start && end_va < wmap_end)
      {
        // cprintf("debug2\n");
        return FAILED;
      }
    }
  }

  // copy of the starting address that the user requested
  // int copy_va = start_va;

  // check : flags val --> 4-bit number e.g. 1110

  // check for MAP_SHARED 0x0002
  if (!(flags_val & MAP_SHARED))
  {
    // MAP_SHARED not set
    return -1;
  }

  // check for MAP_FIXED 0x0004
  if (!(flags_val & MAP_FIXED))
  {
    // MAP_FIXED not set
    return -1;
  }

  // single page size = 4096 bytes
  // assign multiple pages if requested memory size > 4096
  // loop to allocate multiple physical pages, if needed
  int num_pages = mem_length / PGSIZE;

  // if requested memory is not a multiple of page size
  if (mem_length % PGSIZE != 0)
  {
    num_pages += 1;
  }

  // for(int i=0; i<num_pages; i++)
  // {
  //   // allocate single page
  //   char *mem = kalloc();
  //   // cprintf("physical address of allocated page %d : %x\n", i, V2P(mem));

  //   // create PTE -> store PPN & flags
  //   if (mappages(myproc()->pgdir, (char*)copy_va, 4096, V2P(mem), PTE_W | PTE_U) == -1)
  //   {
  //     return -1;
  //   }

  //   // starting address of the next virtual page
  //   copy_va += 0x1000;
  // }

  // -------------- Handle file-backed mapping --------------

  // // check for MAP_ANONYMOUS 0x0004
  // if(!(flags_val & MAP_ANONYMOUS))
  // {
  //   // MAP_ANONYMOUS not set -> file-backed mapping
  //   struct file *f;
  //   if((f=myproc()->ofile[fd]) == 0)
  //   {
  //     return FAILED;
  //   }
  //   fileread(f, (char*)start_va, mem_length);

  // }

  // -------------- update memory mapping meta-data --------------

  // increment number of wmaps
  myproc()->num_maps += 1;

  for (int i = 0; i < 16; i++)
  {
    if (myproc()->mapinfo[i].start_addr == -1)
    {
      myproc()->mapinfo[i].start_addr = start_va;
      myproc()->mapinfo[i].end_addr = start_va + mem_length - 1;
      myproc()->mapinfo[i].map_length = mem_length;
      myproc()->mapinfo[i].pages_in_map = 0;

      if (!(flags_val & MAP_ANONYMOUS))
      {
        int dup_fd;
        struct file *f = myproc()->ofile[fd];
        dup_fd = fdalloc(f);
        filedup(f);
        myproc()->mapinfo[i].file_desc = dup_fd;
      }
      break;
    }
  }

  // return the starting va of the newly created mapping
  return start_va;
}

int sys_wunmap(void)
{
  // int start_va;
  // if (argint(0, &start_va) < 0)
  // {
  //   cprintf("argint failed\n");
  //   return -1;
  // }

  // // int copy_va = start_va;

  // // find if a memory map for start_va exists
  // int map_index = -1;
  // for (int i = 0; i < 16; i++)
  // {
  //   if (myproc()->mapinfo[i].start_addr == start_va)
  //   {
  //     // cprintf("mapping found\n");
  //     map_index = i;
  //     break;
  //   }
  // }

  // // no such memory map exists
  // if (map_index == -1)
  // {
  //   cprintf("map_index = -1\n");
  //   return -1;
  // }

  // // -------- Handle file-backed mapping -------

  // // write the changes made to the mapped memory back to disk
  // // when you're removing the mapping.
  // // You can assume that the offset is always 0.

  // // check : if file-backed only then do the following
  // int fd = myproc()->mapinfo[map_index].file_desc;
  // // if(fd != -1)
  // // {
  // // calculate the number of pages in wmap
  // int num_pages = myproc()->mapinfo[map_index].map_length / PGSIZE;

  // // if requested memory is not a multiple of page size
  // if (myproc()->mapinfo[map_index].map_length % PGSIZE != 0)
  // {
  //   num_pages += 1;
  // }

  // int addr = myproc()->mapinfo[map_index].start_addr;
  // for (int i = 0; i < num_pages; i++)
  // {
  //   pte_t *pte = walkpgdir(myproc()->pgdir, (char *)addr, 0); // get the page-table entry
  //   if (pte == 0)
  //   {
  //     // page not allocated
  //     addr += 4096;
  //     continue;
  //   }

  //   if((*pte & PTE_P)==0)
  //   {
  //     cprintf("%d\n", *pte);
  //   }

  //   if (fd != -1)
  //   {
  //     // File-backed mapping
  //     struct file *f;
  //     if ((f = myproc()->ofile[fd]) == 0)
  //     {
  //       // kill the process
  //       kill(myproc()->pid);
  //     }
  //     f->off = addr - myproc()->mapinfo[map_index].start_addr;
  //     filewrite(f, (char *)addr, 4096);
  //   }
  //   addr += 4096;

  //   int physical_address = PTE_ADDR(*pte); // Access the upper 20-bit of PTE
  //   kfree(P2V(physical_address));          // free the physical memory
  //   *pte = 0;                              // convert to kernel va, free the PTE
  //   // copy_va += 0x1000;                                            // Increment va to next va
  //   // myproc()->mapinfo[map_index].pages_in_map -= 1;
  // }
  // // }

  // // struct file *f;
  // // if((f=myproc()->ofile[myproc()->mapinfo[map_index].file_desc]) == 0)
  // // {
  // //   return FAILED;
  // // }

  // // filewrite(f, (char*)myproc()->mapinfo[map_index].start_addr, myproc()->mapinfo[map_index].map_length);

  // // reset the metadata

  // myproc()->mapinfo[map_index].start_addr = -1;
  // myproc()->mapinfo[map_index].end_addr = -1;
  // myproc()->mapinfo[map_index].map_length = -1;
  // myproc()->mapinfo[map_index].file_desc = -1;

  // // free each page in the map
  // // for(int i=; i<16; i++)
  // // {
  // //   // pte_t *pte = walkpgdir(myproc()->pgdir, (char*)copy_va, 0);   // get the page-table entry
  // //   // int physical_address = PTE_ADDR(*pte);                        // Access the upper 20-bit of PTE
  // //   // kfree(P2V(physical_address));                                 // free the physical memory
  // //   // *pte = 0;                                                     // convert to kernel va, free the PTE
  // //   // copy_va += 0x1000;                                            // Increment va to next va

  // // }

  // // myproc()->pages_in_map[map_index] = -1;
  // myproc()->mapinfo[map_index].pages_in_map = -1;
  // myproc()->num_maps -= 1;

  // return 0;
  return unmap();
}

/*
To translate a virtual address to physical address
according to the page table for the calling process.
*/
int sys_va2pa(void)
{
  //
  int user_va;
  if (argint(0, &user_va) < 0)
  {
    return -1;
  }

  // page-table entry for the given virtual address
  pte_t *pte = walkpgdir(myproc()->pgdir, (char *)user_va, 0);

  if (!pte)
  {
    return FAILED;
  }

  // cprintf("pte=%x\n", pte);
  //  check if PTE is present
  if ((*pte & PTE_P) == 0)
  {
    return FAILED;
  }

  int ppn = PTE_ADDR(*pte);

  int offset = user_va & 0xFFF;

  int pa = ppn | offset;

  // cprintf("ppn=%x\noffset=%x\npa=%x\n", ppn, offset, pa);
  return pa;
}

int sys_getwmapinfo(void)
{
  // pointer for the struct argument
  struct wmapinfo *ptr;

  // check if argument are present & within allocated
  // address space
  if (argptr(0, (void *)&ptr, sizeof(*ptr)) < 0)
  {
    return -1;
  }

  // Null pointer handled
  if (ptr == 0)
  {
    return -1;
  }

  int index = 0;
  for (int i = 0; i < 16; i++)
  {
    if (myproc()->mapinfo[i].start_addr != -1)
    {
      // ptr->addr[index] = myproc()->start_addr[i];
      // ptr->length[index] = myproc()->map_length[i];
      // ptr->n_loaded_pages[index] = myproc()->pages_in_map[i];

      ptr->addr[index] = myproc()->mapinfo[i].start_addr;
      ptr->length[index] = myproc()->mapinfo[i].map_length;
      ptr->n_loaded_pages[index] = myproc()->mapinfo[i].pages_in_map;
      index++;
    }
    else
    {
      ptr->addr[index] = 0;
      ptr->length[index] = 0;
      ptr->n_loaded_pages[index] = 0;
    }
  }
  ptr->total_mmaps = index;
  return 0;
}

int sys_test(void)
{
  return myproc()->pid;
}

// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"
#include "proc.h"
#include "x86.h"

unsigned char pg_ref_cnt[1024 * 1024] = {0};

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct run
{
  struct run *next;
};

struct
{
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
} kmem;

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  freerange(vstart, vend);
}

void kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
}

void freerange(void *vstart, void *vend)
{
  char *p;
  p = (char *)PGROUNDUP((uint)vstart);
  for (; p + PGSIZE <= (char *)vend; p += PGSIZE)
    kfree(p);
}
// PAGEBREAK: 21
//  Free the page of physical memory pointed at by v,
//  which normally should have been returned by a
//  call to kalloc().  (The exception is when
//  initializing the allocator; see kinit above.)
void kfree(char *v)
{
  struct run *r;

  if (kmem.use_lock)
  {
    int index = V2P(v) / PGSIZE;
    pg_ref_cnt[index]--;
    if (pg_ref_cnt[index] > 0)
    {
      // cprintf("ref count greater than 0\n");
      return;
    }
  }

  if ((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  if (kmem.use_lock)
    acquire(&kmem.lock);
  r = (struct run *)v;
  r->next = kmem.freelist;
  kmem.freelist = r;
  if (kmem.use_lock)
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char *
kalloc(void)
{
  struct run *r;

  if (kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;

  // increment the reference count of the pages
  char *mem = (char *)r;
  int index = V2P(mem) / PGSIZE;
  pg_ref_cnt[index] += 1;

  if (r)
    kmem.freelist = r->next;
  if (kmem.use_lock)
    release(&kmem.lock);

  // cprintf("ref count for %x is %d in kalloc\n", index, pg_ref_cnt [index]);

  return (char *)r;
}

void duplicate_page(pte_t *pte, int alloc_va)
{
  struct proc *curproc = myproc();

  // determine the flags & pa
  uint pa, flags;
  flags = PTE_FLAGS(*pte);
  pa    = PTE_ADDR(*pte);

  // cprintf("%d process are accessing this page\n", pg_ref_cnt[pa/PGSIZE]);

  // Error case : Ref Count = 0;
  if(get_ref_cnt(pa) == 0)
  {
    cprintf("Ref cnt = 0, Error\n");
    kill(curproc->pid);
    return;
  }


  // Case : Ref Cnt = 1
  if(get_ref_cnt(pa) == 1)
  {
    // make the page WRITABLE
    flags |= (uint)PTE_W;

    *pte = pa | flags;

    // reload tlb
    lcr3(V2P(curproc->pgdir));
    return;
  }

  // Case : Ref Count > 1

  // make the new page writable
  flags = flags | (uint)PTE_W;

  // clear out the pte of the child
  *pte = 0;

  // create a new page
  char *mem = kalloc();
  memmove(mem, (char *)P2V(pa), PGSIZE);
  mappages(curproc->pgdir, (void *)alloc_va, PGSIZE, V2P(mem), flags);

  // decrement ref count of the old page
  ref_cnt_decrementer(pa);

  // reload tlb
  lcr3(V2P(curproc->pgdir));
  return;
}











// incrementer function for ref_cnt of each physical page
int ref_cnt_incrementer(uint pa)
{
  if (kmem.use_lock)
    acquire(&kmem.lock);

  pg_ref_cnt[pa / PGSIZE]++;

  if (kmem.use_lock)
    release(&kmem.lock);
  // cprintf("incremented page ref cnt %d to %d\n", pa, pg_ref_cnt[pa]);
  return 0;
}

// incrementer function for ref_cnt of each physical page
int ref_cnt_decrementer(uint pa)
{
  if (kmem.use_lock)
    acquire(&kmem.lock);

  pg_ref_cnt[pa / PGSIZE]--;

  if (kmem.use_lock)
    release(&kmem.lock);
  // cprintf("incremented page ref cnt %d to %d\n", pa, pg_ref_cnt[pa]);
  return 0;
}

int get_ref_cnt(uint pa)
{
  int temp = 0;
  if (kmem.use_lock)
    acquire(&kmem.lock);

  temp = pg_ref_cnt[pa / PGSIZE];

  if (kmem.use_lock)
    release(&kmem.lock);
  // cprintf("incremented page ref cnt %d to %d\n", pa, pg_ref_cnt[pa]);
  return temp;
}

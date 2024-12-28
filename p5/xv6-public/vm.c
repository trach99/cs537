#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"

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

// array for page reference count
// number of pages = 4GB/4KB = 1MB = 1024*1024
// uchar pg_ref_cnt [1024*1024];
extern unsigned char pg_ref_cnt[1024*1024];





extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler()

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
void
seginit(void)
{
  struct cpu *c;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  c = &cpus[cpuid()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
  lgdt(c->gdt, sizeof(c->gdt));
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
  } else {
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;

  a = (char*)PGROUNDDOWN((uint)va);
  last = (char*)PGROUNDDOWN(((uint)va) + size - 1);
  for(;;){
    if((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    if(*pte & PTE_P)
      panic("remap");
    *pte = pa | perm | PTE_P;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
} kmap[] = {
 { (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space
 { (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
 { (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
 { (void*)DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

// Set up kernel part of a page table.
pde_t*
setupkvm(void)
{
  pde_t *pgdir;
  struct kmap *k;

  if((pgdir = (pde_t*)kalloc()) == 0)
    return 0;
  memset(pgdir, 0, PGSIZE);
  if (P2V(PHYSTOP) > (void*)DEVSPACE)
    panic("PHYSTOP too high");
  for(k = kmap; k < &kmap[NELEM(kmap)]; k++)
    if(mappages(pgdir, k->virt, k->phys_end - k->phys_start,
                (uint)k->phys_start, k->perm) < 0) {
      freevm(pgdir);
      return 0;
    }
  return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
  kpgdir = setupkvm();
  switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
  lcr3(V2P(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p)
{
  if(p == 0)
    panic("switchuvm: no process");
  if(p->kstack == 0)
    panic("switchuvm: no kstack");
  if(p->pgdir == 0)
    panic("switchuvm: no pgdir");

  pushcli();
  mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
                                sizeof(mycpu()->ts)-1, 0);
  mycpu()->gdt[SEG_TSS].s = 0;
  mycpu()->ts.ss0 = SEG_KDATA << 3;
  mycpu()->ts.esp0 = (uint)p->kstack + KSTACKSIZE;
  // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
  // forbids I/O instructions (e.g., inb and outb) from user space
  mycpu()->ts.iomb = (ushort) 0xFFFF;
  ltr(SEG_TSS << 3);
  lcr3(V2P(p->pgdir));  // switch to process's address space
  popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);
  memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz, uint flags)
{
  uint i, pa, n;
  pte_t *pte;

  if((uint) addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
      panic("loaduvm: address should exist");

    // Update flags with appropriate conditions
    if(flags & PTE_W)
    {
      *pte |= PTE_W;      // set writable if ELF flags allow writing 
    }
    else
    {
      *pte &= ~PTE_W;     // ensure its not writable if the flag disallows it
    }

    pa = PTE_ADDR(*pte);
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, P2V(pa), offset+i, n) != n)
      return -1;
  }
  return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  char *mem;
  uint a;

  if(newsz >= KERNBASE)
    return 0;
  if(newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  for(; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
      cprintf("allocuvm out of memory (2)\n");
      deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  pte_t *pte;
  uint a, pa;

  if(newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for(; a  < oldsz; a += PGSIZE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(!pte)
      a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
    else if((*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte);
      if(pa == 0)
        panic("kfree");
      char *v = P2V(pa);
      kfree(v);
      *pte = 0;
    }
  }
  return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir)
{
  uint i;

  if(pgdir == 0)
    panic("freevm: no pgdir");
  deallocuvm(pgdir, KERNBASE, 0);
  for(i = 0; i < NPDENTRIES; i++){
    if(pgdir[i] & PTE_P){
      char * v = P2V(PTE_ADDR(pgdir[i]));
      kfree(v);
    }
  }
  kfree((char*)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if(pte == 0)
    panic("clearpteu");
  *pte &= ~PTE_U;
}

// Given a parent process's page table, create a copy
// of it for a child.
pde_t*
copyuvm(pde_t *pgdir, uint sz)
{
  // cprintf("copyuvm called\n");
  pde_t *d;
  pte_t *pte;
  uint pa, i, flags;
  // char *mem;

  if((d = setupkvm()) == 0)
    return 0;
  for(i = 0; i < sz; i += PGSIZE)
  {
    if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
      panic("copyuvm: pte should exist");
    if(!(*pte & PTE_P))
      panic("copyuvm: page not present");
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);
    // if((mem = kalloc()) == 0)
    //   goto bad;
    // memmove(mem, (char*)P2V(pa), PGSIZE);
    // // cprintf("\nmemmove called by pid = %d\n", myproc()->pid);
    // if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {
    //   kfree(mem);
    //   goto bad;
    // }
    // ######################## p5 #############################
    
    // set the COW bit on the flags -> using bit-9
    // cprintf("PTE_W = %d\n",flags & PTE_W);
    if((flags & PTE_W) || (flags& PTE_COW))
    {
      flags = flags | PTE_COW;              // page was READ/WRITE in past
    }
    else
    {
      flags = flags & ~PTE_COW;             // page was READ-ONLY in past
    }
    // cprintf("%x\n", flags);
    // set the flags to READ-ONLY
    flags &= ~PTE_W;

    // make the parent's page READ-ONLY
    *pte = (pa | flags);
    // cprintf("pte in copyuvm = %x\n", *pte);
    // cprintf("PTE_COW = %d\n", (*pte)&PTE_COW);

    // create P->V mapping in child's page table
    if(mappages(d, (void*)i, PGSIZE, pa, flags) < 0) {
      // kfree(mem);
      goto bad;
    }

    // increment the reference count
    // pg_ref_cnt[pa/PGSIZE]++;
    ref_cnt_incrementer(pa);
    // cprintf("ref count of page %x in copyuvm to %d\n", pa/PGSIZE, pg_ref_cnt[pa/PGSIZE]);
    

    
    // ########################################################
    
  }

  // parent pgdir reloaded
  lcr3(V2P(pgdir));
  // cprintf("copyuvm folr loop end\n");
  return d;

bad:
  // cprintf("BAD\n");
  freevm(d);
  return 0;
}

//PAGEBREAK!
// Map user virtual address to kernel address.
char*
uva2ka(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if((*pte & PTE_P) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  return (char*)P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
  char *buf, *pa0;
  uint n, va0;

  buf = (char*)p;
  while(len > 0){
    va0 = (uint)PGROUNDDOWN(va);
    pa0 = uva2ka(pgdir, (char*)va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if(n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

// p5
// allocate a page from kernel & create a PTE
int alloc_page (struct proc *p, int * start_ptr)
{
  // allocate pages
    char *mem = kalloc();

    // create PTE mapping VPN -> PPN
    return mappages(p->pgdir, start_ptr, 4096, V2P(mem), PTE_W | PTE_U);

}

// // incrementer function for ref_cnt of each physical page
// int ref_cnt_incrementer(uint pa)
// {
//   // if(pa > KERNBASE)
//   // {
//   //   return -1;
//   // }
//   pg_ref_cnt[pa/PGSIZe]++;
//   // cprintf("incremented page ref cnt %d to %d\n", pa, pg_ref_cnt[pa]);
//   return 0;
// }

// int ref_cnt_decrementer(char * pa)
// {
//   pg_ref_cnt[(int) pa]--;
//   return 0;
// }



// p5
int unmap(void)
{
  // get the starting address of the wmap to be unmapped
  int start_va;
  if (argint(0, &start_va) < 0)
  {
    cprintf("argint failed\n");
    return -1;
  }


  // find if a wmap for that start_va exists
  int map_index = -1;
  for (int i = 0; i < 16; i++)
  {
    if (myproc()->mapinfo[i].start_addr == start_va)
    {
      // cprintf("mapping found\n");
      map_index = i;
      break;
    }
  }

  // no such wmap exists
  if (map_index == -1)
  {
    cprintf("map_index = -1\n");
    return -1;
  }
  
  // ##################### Wmap Exists #####################

  // -------- Removing mappings -------

  // calculate the number of pages in wmap
  int num_pages = myproc()->mapinfo[map_index].map_length / PGSIZE;

  // wmap memory not a multiple of page size
  if (myproc()->mapinfo[map_index].map_length % PGSIZE != 0)
  {
    num_pages += 1;
  }

  // store last page bytes
  int last_page_bytes = myproc()->mapinfo[map_index].map_length % PGSIZE;

  int addr = myproc()->mapinfo[map_index].start_addr;

  // check : if file-backed only then do the following
  int fd = myproc()->mapinfo[map_index].file_desc;

  for (int i = 0; i < num_pages; i++)
  {
    pte_t *pte = walkpgdir(myproc()->pgdir, (char *)addr, 0); // get the page-table entry
    if (pte == 0)
    {
      // page not allocated
      addr += 4096;
      continue;
    }

    if((*pte & PTE_P)==0)
    {
      cprintf("%d\n", *pte);
    }
  
    if (fd != -1)
    {
      // File-backed mapping
      // write the changes made to the mapped memory back to disk
      // when you're removing the mapping.
      // You can assume that the offset is always 0.

      struct file *f;
      if ((f = myproc()->ofile[fd]) == 0)
      {
        // kill the process
        kill(myproc()->pid);
      }
      f->off = addr - myproc()->mapinfo[map_index].start_addr;
      if(i == num_pages-1 && last_page_bytes != 0)
        filewrite(f, (char *)addr, last_page_bytes);
      else
        filewrite(f, (char *)addr, 4096);
    }
    addr += 4096;

    int physical_address = PTE_ADDR(*pte); // Access the upper 20-bit of PTE
    
    // don't free page if reference count greater than 1
    if(pg_ref_cnt[physical_address/PGSIZE] > 1)
    {
      pg_ref_cnt[physical_address/PGSIZE]--;
    }
    else{
        kfree(P2V(physical_address));          // free the physical memory
    }
    
    *pte = 0;                              // convert to kernel va, free the PTE
    // copy_va += 0x1000;                                            // Increment va to next va
    // myproc()->mapinfo[map_index].pages_in_map -= 1;
  }
  // }

  // struct file *f;
  // if((f=myproc()->ofile[myproc()->mapinfo[map_index].file_desc]) == 0)
  // {
  //   return FAILED;
  // }

  // filewrite(f, (char*)myproc()->mapinfo[map_index].start_addr, myproc()->mapinfo[map_index].map_length);

  // reset the metadata

  myproc()->mapinfo[map_index].start_addr = -1;
  myproc()->mapinfo[map_index].end_addr = -1;
  myproc()->mapinfo[map_index].map_length = -1;
  myproc()->mapinfo[map_index].file_desc = -1;

  // free each page in the map
  // for(int i=; i<16; i++)
  // {
  //   // pte_t *pte = walkpgdir(myproc()->pgdir, (char*)copy_va, 0);   // get the page-table entry
  //   // int physical_address = PTE_ADDR(*pte);                        // Access the upper 20-bit of PTE
  //   // kfree(P2V(physical_address));                                 // free the physical memory
  //   // *pte = 0;                                                     // convert to kernel va, free the PTE
  //   // copy_va += 0x1000;                                            // Increment va to next va

  // }

  // myproc()->pages_in_map[map_index] = -1;
  myproc()->mapinfo[map_index].pages_in_map = -1;
  myproc()->num_maps -= 1;

  return 0;
}


int copy_mappings(struct proc* parent, struct proc *child)
{
  //pde_t *d;
  pte_t *pte;
  uint pa, flags;
  //char *mem;
  // cprintf("copy_mappings called\n");
  for(int i = 0x60000000; i < (0x80000000-PGSIZE); i += PGSIZE)
  {
    //cprintf("i = %x\n", i);
    if((pte = walkpgdir(parent->pgdir, (void *) i, 0)) == 0)
    {
      continue;
    }
    
    // cprintf("inside for loop 2\n");
    if(!(*pte & PTE_P))
    {
      // cprintf("inside pte* condition\n");
      continue;
    }

    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);
    // if((mem = kalloc()) == 0)
      
    // memmove(mem, (char*)P2V(pa), PGSIZE);
    // cprintf("\nmemmove called by pid = %d\n", myproc()->pid);
    if(mappages(child->pgdir, (void*)i, PGSIZE, pa, flags) < 0) 
    {
      cprintf("inside mappages\n");
      return -1;
    }

    // increment the reference count of the page
    // int index = pa/PGSIZE;
    ref_cnt_incrementer(pa);
    // cprintf("reference count for %x is %d\n", index, pg_ref_cnt [index]);

  }
  // cprintf("outside the for loop\n");
  return 0;
}

int remove_mappings(struct proc * child_proc)
{
  pte_t *pte;
  for(int i=0x60000000; i< KERNBASE-PGSIZE; i+=PGSIZE)
  {
    if((pte = walkpgdir(child_proc->pgdir, (void *) i, 0)) == 0)
    {
      continue;
    }
    
    // cprintf("inside for loop 2\n");
    if(!(*pte & PTE_P))
    {
      // cprintf("inside pte* condition\n");
      continue;
    }
    *pte = 0;

    uint pa = PTE_ADDR(*pte);
    //int index = pa/PGSIZE;
    ref_cnt_decrementer(pa);
  }
  lcr3(V2P(child_proc->pgdir));
  return 0;
}


//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.


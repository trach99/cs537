// test.c
#include "types.h"
#include "stat.h"
#include "user.h"
#include "wmap.h"

int main(void) 
{

  struct wmapinfo wminfo;
  // printf(1, "The process ID is: %d\n", test());

  printf(1, "The physical address is: %x\n", va2pa(0x60000123));


  // printf(1, "The return value of getwmapinfo: %d\n", getwmapinfo(&wminfo));
  // printf(1, "total wmaps = %d\n", wminfo.total_mmaps);
  

  uint address = wmap(0x60000000, 8192, MAP_FIXED | MAP_SHARED | MAP_ANONYMOUS, -1);
  printf(1, "The address returned by wmap: %x\n", address);


  printf(1, "The return value of getwmapinfo: %d\n", getwmapinfo(&wminfo));
  printf(1, "total wmaps = %d\n", wminfo.total_mmaps);
  printf(1, "address length pages\n");
  for(int i=0; i<16; i++)
  {
    printf(1, "%x %d %d\n", wminfo.addr[i], wminfo.length[i], wminfo.n_loaded_pages[i]);
  }


  printf(1, "The physical address is: %x\n", va2pa(0x60000123));

  // address = wmap(0x60001000, 8192, MAP_FIXED | MAP_SHARED | MAP_ANONYMOUS, -1);
  // printf(1, "The address returned by wmap: %x\n", address);


  printf(1, "Return value wunmap is: %d\n", wunmap(0x60000000));
  
  
  printf(1, "The return value of getwmapinfo: %d\n", getwmapinfo(&wminfo));
  printf(1, "total wmaps = %d\n", wminfo.total_mmaps);
  printf(1, "address length pages\n");
  for(int i=0; i<16; i++)
  {
    printf(1, "%x %d %d\n", wminfo.addr[i], wminfo.length[i], wminfo.n_loaded_pages[i]);
  }

  printf(1, "The physical address is: %x\n", va2pa(0x60000123));
  exit();
}

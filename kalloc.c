// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

// for kinit1() 4MB size : total pages : 1024
// for kinit2() remaining mem : total pages : 57344
// total approx size : 59000

char* freelist_arr[60000];

// struct run {
//   struct run *next;
// };

struct {
  struct spinlock lock;
  int use_lock;
  // struct run *freelist;
  // free_mem : stores the next position of the last available memory in the freelist array
  int free_mem; 
} kmem;

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  kmem.free_mem = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree(p);
}
//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  // struct run *r;
  char* fl;

  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  if(kmem.use_lock)
    acquire(&kmem.lock);
  fl = (char*)v;
  // r->next = kmem.freelist;
  // kmem.freelist = r;

  freelist_arr[kmem.free_mem] = fl;
  kmem.free_mem++;

  if(kmem.use_lock)
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  // struct run *r;
  char* fl;

  if(kmem.use_lock)
    acquire(&kmem.lock);
  // r = kmem.freelist;

  if(kmem.free_mem == 0) { // if memory is not availabel
    release(&kmem.lock);
    return 0;
  }
  kmem.free_mem -= 1;
  fl = freelist_arr[kmem.free_mem];

  // if(r)
  //   kmem.freelist = r->next;

  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)fl;
}


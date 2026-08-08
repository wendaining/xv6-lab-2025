// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

#define NPAGE ((PHYSTOP - KERNBASE) / PGSIZE)

struct {
  struct spinlock lock;
  int count[NPAGE];
} kref;

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

static int
paindex(uint64 pa)
{
  return (pa - KERNBASE) / PGSIZE;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&kref.lock, "kref");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    // kfree() drops a reference, so give each boot-time page one
    // reference before adding it to the free list.
    acquire(&kref.lock);
    kref.count[paindex((uint64)p)] = 1;
    release(&kref.lock);
    kfree(p);
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  int index = paindex((uint64)pa);
  acquire(&kref.lock);
  if(kref.count[index] < 1)
    panic("kfree: no reference");
  kref.count[index]--;
  if(kref.count[index] > 0){
    release(&kref.lock);
    return;
  }
  release(&kref.lock);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
    acquire(&kref.lock);
    if(kref.count[paindex((uint64)r)] != 0)
      panic("kalloc: referenced page");
    kref.count[paindex((uint64)r)] = 1;
    release(&kref.lock);
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
  return (void*)r;
}

void
krefinc(uint64 pa)
{
  if((pa % PGSIZE) != 0 || (char*)pa < end || pa >= PHYSTOP)
    panic("krefinc");

  int index = paindex(pa);
  acquire(&kref.lock);
  if(kref.count[index] < 1)
    panic("krefinc: free page");
  kref.count[index]++;
  release(&kref.lock);
}

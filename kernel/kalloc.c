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

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

uint32 page_rc[(PHYSTOP - KERNBASE) / PGSIZE] = {0};/* page ref counter */

void check_pg_invaild(uint64 pa) {

  if(pa % PGSIZE != 0)
    panic("check_pg_invaild : pa err");

  if(pa < KERNBASE || pa >= PHYSTOP) {
    printf("[!] pa is %p\n", pa);
    panic("check_pg_invaild : pa over boundary");
  }

}

uint64 refidx(uint64 pa) {

  check_pg_invaild(pa);
    
  return (pa - KERNBASE) / PGSIZE;
}

/// @brief add a page reference counter by one
/// @param va
void add_pgref(uint64 pa) {
  check_pg_invaild(pa);
    
  page_rc[refidx(pa)] += 1;
}


/// @brief 
/// @param pa decrease a page reference counter by one
void dec_pgref(uint64 pa) {

  check_pg_invaild((uint64)pa);
  if(pa == 0) panic("dec_pgref : pa is zero");

  uint64 idx = refidx(pa);
    
  page_rc[idx] -= 1;

  if(page_rc[idx] == 0)
    kfree((void *)pa);
}

/// @brief initialize all page refercounter to 1
/// @param start 
/// @param end 
void init_pgref(uint64 start, uint64 end) {

  for(uint32 i = start; i < end; i += PGSIZE) {
    page_rc[refidx(i)] = 1;// 最开始有一次全部free
  }

}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  init_pgref((uint64)end, PHYSTOP);
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  if(page_rc[refidx((uint64)pa)] >= 2) {
    page_rc[refidx((uint64)pa)] -= 1;
    return;
  }else if(page_rc[refidx((uint64)pa)] == 1) {
    page_rc[refidx((uint64)pa)] -= 1;
  }
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
    memset((char*)r, 5, PGSIZE); // fill with junk
    add_pgref((uint64)r);
  }
  return (void*)r;
}

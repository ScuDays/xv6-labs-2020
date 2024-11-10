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

struct run
{
  struct run *next;
};

struct
{
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void kinit()
{
  for (int i = 0; i < NCPU; i++)
  {
    initlock(&kmem[i].lock, "kmem");
  }

  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  // 这里转化为 char* 是因为 PGSIZE 是以字节为单位的，所以需要转为 char* 使得地址每次以 1字节为单位增减。
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

// acquire(&kmem.lock);
// r->next = kmem.freelist;
// kmem.freelist = r;
// release(&kmem.lock);
#ifdef lab_lock
  push_off();
  int id = cpuid();
  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  release(&kmem[id].lock);
  pop_off();
#endif
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
// struct run *r;

// acquire(&kmem.lock);
// r = kmem.freelist;
// if(r)
//   kmem.freelist = r->next;
// release(&kmem.lock);

// if(r)
//   memset((char*)r, 5, PGSIZE); // fill with junk
// return (void*)r;
#ifdef lab_lock
  push_off();
  int id = cpuid();
  struct run *r;
  acquire(&kmem[id].lock);
  r = kmem[id].freelist;
  if (r)
  {
    kmem[id].freelist = r->next;
    release(&kmem[id].lock);
  }
  else
  {
    release(&kmem[id].lock);

    for (int i = 0; i < NCPU; i++)
    {
      if(i == id)continue;
      acquire(&kmem[i].lock);
      r = kmem[i].freelist;
      if (r)
      {
        kmem[i].freelist = r->next;
        //freerange(r, r + PGSIZE);
        release(&kmem[i].lock);
        break;
      }
      else
      {
        release(&kmem[i].lock);
      }
    }
  }
  // release(&kmem[id].lock);

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  pop_off();
  return (void *)r;
#endif
}

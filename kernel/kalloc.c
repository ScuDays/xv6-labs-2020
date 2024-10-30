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

#ifdef lab_cow
int PageRefCount[PHYSTOP / PGSIZE]; // 记录每一个page的引用数量

#endif
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
} kmem;

void kinit()
{
  initlock(&kmem.lock, "kmem");
  // memset(PageRefCount, 0, sizeof(sizeof(int) * PHYSTOP / PGSIZE));
  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
  {

#ifdef lab_cow
    PageRefIncrease(((uint64)p) / PGSIZE);
#endif

    kfree(p);
  }
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

  PageRefDecrease(((uint64)pa) / PGSIZE);
#ifdef lab_cow
  if (PageRefCount[(uint64)pa / PGSIZE] == 0)
  {
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);
    r = (struct run *)pa;
    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }

#endif

  // // Fill with junk to catch dangling refs.
  // memset(pa, 1, PGSIZE);

  // r = (struct run*)pa;

  // acquire(&kmem.lock);
  // r->next = kmem.freelist;
  // kmem.freelist = r;
  // release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{

#ifdef lab_cow

  struct run *r;
  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if (r)
  {
    memset((char *)r, 5, PGSIZE); // fill with junk
    PageRefCount[((uint64)r) / PGSIZE] = 1;
  }
  return (void *)r;

#endif

#ifndef lab_cow
  struct run *r;
  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
#endif
}

#ifdef lab_cow

void PageRefIncrease(int i)
{
  PageRefCount[i]++;
}

void PageRefDecrease(int i)
{
  PageRefCount[i]--;
}

// 对设置了 PTE_C 的页面进行分配
uint64 cowAlloc(pagetable_t pagetable, uint64 error_va)
{

  uint64 mem;
  uint64 pa;
  uint flags;
  pte_t *pte;
  uint64 alloc_va;

  // 实际需要进行映射的虚拟地址
  alloc_va = PGROUNDDOWN(error_va);
  pte = walk(pagetable, alloc_va, 0);
  flags = PTE_FLAGS(*pte);
  pa = PTE2PA(*pte);

  printf("r_stval:%p\n", r_stval());
  printf("PGROUNDDOWN(r_stval()):%p\n", PGROUNDDOWN(r_stval()));
  printf("flags:%p\n", flags);
  printf("pa:%p\n", pa);
  printf("r_sepc:%p\n", r_sepc());
  printf("Page:%d\n", PageRefCount[pa/PGSIZE]);
  printf("r_scause:%p\n", r_scause());
  printf("-------------------------------------\n\n");
 

  // 判断是否是 COW 页面
  if ((flags & PTE_C) != 0)
  {

    if (PageRefCount[pa / PGSIZE] == 1)
    {
      // printf(" ==1\n");
      //  清除 COW 位并加上读权限
      *pte |= PTE_W;
      *pte &= ~PTE_C;
      return pa;
    }
    else
    {
      // printf("r_scause:%p  before:flags:%p\n", r_scause(), flags);
      // 清除 COW 位并加上读权限
      flags = flags | PTE_W;
      flags = (flags & (~PTE_C));
      mem = (uint64)kalloc();
      if (mem == 0)
      {
        printf("kalloc失败\n");
        return -1;
      }
      memmove((void *)mem, (const void *)(pa), PGSIZE);
      uvmunmap(pagetable, alloc_va, 1, 0);

      if (mappages(pagetable, alloc_va, PGSIZE, (uint64)mem, flags) != 0)
      {
        printf("mappages失败\n");
        kfree((void *)mem);
        return -1;
      }
      // 减少原本指向的内存的引用
      PageRefDecrease(pa / PGSIZE);
      return mem;
    }
  }
  return -1;
}
#endif
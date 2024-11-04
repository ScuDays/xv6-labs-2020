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
struct ref_stru
{
  struct spinlock lock;
  int cnt[PHYSTOP / PGSIZE]; // 引用计数
} ref;
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
  initlock(&ref.lock, "ref");
  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
  {

#ifdef lab_cow
    ref.cnt[(uint64)p / PGSIZE] = 1;
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

  acquire(&ref.lock);
#ifdef lab_cow
  if (--ref.cnt[(uint64)pa / PGSIZE] == 0)
  {
    release(&ref.lock);
    // Fill with junk to catch dangling refs.
    r = (struct run *)pa;
    memset(pa, 1, PGSIZE);
    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
  else{
     release(&ref.lock);
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
  if (r){
    kmem.freelist = r->next;
    acquire(&ref.lock);
    ref.cnt[(uint64)r / PGSIZE] = 1;  // 将引用计数初始化为1
    release(&ref.lock);
  }
  release(&kmem.lock);

  if (r)
  {
    memset((char *)r, 5, PGSIZE); // fill with junk
    
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

int PageRefIncrease(void* pa)
{
   if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    return -1;
  acquire(&ref.lock);
  ref.cnt[(uint64)pa / PGSIZE]++;
  release(&ref.lock);
  return 0;
}


// 对设置了 PTE_C 的页面进行分配
void* cowAlloc(pagetable_t pagetable, uint64 error_va)
{

   if(error_va >= MAXVA)
    return 0;
  char* mem;
  uint64 pa;
  uint flags;
  pte_t *pte;
  uint64 alloc_va;

  // 实际需要进行映射的虚拟地址
  alloc_va = PGROUNDDOWN(error_va);
  pte = walk(pagetable, alloc_va, 0);
  flags = PTE_FLAGS(*pte);
  pa = PTE2PA(*pte);
  if(pa == 0){
    return 0;
  }
 
  // printf("r_stval:%p\n", r_stval());
  // printf("PGROUNDDOWN(r_stval()):%p\n", PGROUNDDOWN(r_stval()));
  // printf("flags:%p\n", flags);
  // printf("pa:%p\n", pa);
  // printf("r_sepc:%p\n", r_sepc());
  // printf("Page:%d\n", ref.cnt[pa / PGSIZE]);
  // printf("r_scause:%p\n", r_scause());
  // printf("-------------------------------------\n\n");

  // 判断是否是 COW 页面
  if ((flags & PTE_C) != 0)
  {
    // 只有一个进程对此物理地址存在引用
    // 则直接修改PTE
    if (ref.cnt[pa / PGSIZE] == 1)
    {
      //  printf(" ==1\n");
      //  清除 COW 位并加上读权限
      *pte |= PTE_W;
      *pte &= ~PTE_C;
      return (void *)pa;
    }
    else
    {
      // printf("r_scause:%p  before:flags:%p\n", r_scause(), flags);
      // 清除 COW 位并加上读权限
      flags = flags | PTE_W;
      flags = (flags & (~PTE_C));
      mem = kalloc();
      if (mem == 0)
      {
        //printf("kalloc失败\n");
        return 0;
      }
      memmove(mem, (char*)pa, PGSIZE);
     // uvmunmap(pagetable, alloc_va, 1, 0);
       *pte &= ~PTE_V;
      if (mappages(pagetable, alloc_va, PGSIZE, (uint64)mem, flags) != 0)
      {
        printf("mappages失败\n");
        kfree((void *)mem);
        return 0;
      }
      // 减少原本指向的内存的引用
      // 问题:
      // 原因:copyout()中)调用cowAlloc()会存在需要释放内存的情况，但在usertrap()调用cowAlloc()处理中不会，
      // 一直考虑的是cowAlloc()中释放内存的情况，导致错误。
      // 原本代码：
      //PageRefDecrease((void*)(PGROUNDDOWN(pa)));
      // 修正后代码：
      kfree((char*)PGROUNDDOWN(pa));
      return mem;
    }
  }
  return 0;
}

int cowpage(pagetable_t pagetable, uint64 va) {
  if(va >= MAXVA)
    return -1;
  pte_t* pte = walk(pagetable, va, 0);
  if(pte == 0)
    return -1;
  if((*pte & PTE_V) == 0)
    return -1;
  return (*pte & PTE_C ? 0 : -1);
}
#endif
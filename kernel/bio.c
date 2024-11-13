#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define lab_lock_buffer 1
#define BucketNumber 13

#define hashing(i) ((i) % (BucketNumber))

#ifndef lab_lock_buffer
struct
{
  struct spinlock lock;
  struct buf buf[NBUF];
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;
#endif

#ifdef lab_lock_buffer
struct HashBufferBucket
{
  struct buf head;      // 头节点
  struct spinlock lock; // 锁
};

struct
{
  struct spinlock lock;
  struct buf buf[NBUF];
  // 缓存区哈希表
  struct HashBufferBucket BufBucket[BucketNumber];
} bcache;

#endif
void binit(void)
{

#ifndef lab_lock_buffer
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for (b = bcache.buf; b < bcache.buf + NBUF; b++)
  {
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
#endif

#ifdef lab_lock_buffer
  char LockName[16];
  initlock(&bcache.lock, "bcache");
  // 初始化桶
  for (int i = 0; i < BucketNumber; i++)
  {
    // 初始化桶的锁
    snprintf(LockName, sizeof(LockName), "bcache_%d", i);
    initlock(&bcache.BufBucket[i].lock, LockName);
    // 初始化每个桶的头节点
    // 之所以采用这个头节点的方式，是为了复用源代码，方便编写
    bcache.BufBucket[i].head.prev = &bcache.BufBucket[i].head;
    bcache.BufBucket[i].head.next = &bcache.BufBucket[i].head;
  }

  struct buf *b;
  // 将所有的缓冲块都先放到哈希表的桶 0 当中
  for (b = bcache.buf; b < bcache.buf + NBUF; b++)
  {
    b->next = bcache.BufBucket[0].head.next;
    b->prev = &bcache.BufBucket[0].head;
    initsleeplock(&b->lock, "buffer");
    bcache.BufBucket[0].head.next->prev = b;
    bcache.BufBucket[0].head.next = b;
  }
#endif
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{
#ifndef lab_lock_buffer
  struct buf *b;

  acquire(&bcache.lock);

  // Is the block already cached?
  for (b = bcache.head.next; b != &bcache.head; b = b->next)
  {
    if (b->dev == dev && b->blockno == blockno)
    {
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for (b = bcache.head.prev; b != &bcache.head; b = b->prev)
  {
    if (b->refcnt == 0)
    {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
#endif

#ifdef lab_lock_buffer

  struct buf *b;
  // acquire(&bcache.lock);
  //  块应该在的桶
  uint TheBucketNumber = hashing(blockno);
  // 1：查看磁盘块是否已缓存
  acquire(&bcache.BufBucket[TheBucketNumber].lock);

  for (b = bcache.BufBucket[TheBucketNumber].head.next; b != &bcache.BufBucket[TheBucketNumber].head; b = b->next)
  // for (b = bcache.BufBucket[TheBucketNumber].head.prev; b != &bcache.BufBucket[TheBucketNumber].head; b = b->prev)
  {
    if (b->dev == dev && b->blockno == blockno)
    {
      b->refcnt++;
      release(&bcache.BufBucket[TheBucketNumber].lock);
      // release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  /* 问题:*/
  /* 这里是一个很关键的问题，涉及到一个磁盘块对应了两个缓冲区
     一开始我是先释放了了应在桶的锁，后面需要的时候在获取
     这就会导致一个问题，
     如果我们释放了应在桶的锁，这个时候刚好，别的进程使用同样的参数调用bget(),
     进入应在桶搜索之后同样发现不存在，又会去寻找一个空闲的缓存块
     这样，就会出现同时有两个进程在为同一个磁盘块寻找空闲的缓存块，
     导致了一个磁盘块对应多个缓存块，出现 panic: freeing free block

  */
  // 未缓存，先释放当前获取的桶的锁
  //  release(&bcache.BufBucket[TheBucketNumber].lock);

  // 2：磁盘块未缓存
  // 需要寻找一块未使用的缓存块来使用
  // 先在当前的桶中查找
  // 如果循环回到当前桶，说明全部桶中都没有空闲块
  for (int i = TheBucketNumber, cycle = 0; cycle != BucketNumber; i++, cycle++)
  {
    // 当前查询的桶的序号
    int j = hashing(i);
    // 获取所要查询的桶的锁
    // 如果当前查询桶与 TheBucketNumber 号桶不同，则要获取当前查询桶的锁
    // 如果相同，不能获取锁，不然会导致死锁
    if (j != TheBucketNumber)
    {
      acquire(&bcache.BufBucket[j].lock);
    }
    // 查询这个桶，双向列表，若重复到桶的头部，说明没有空闲的块，退出。
    for (b = bcache.BufBucket[j].head.prev; b != &bcache.BufBucket[j].head; b = b->prev)
    // for (b = bcache.BufBucket[j].head.next; b != &bcache.BufBucket[j].head; b = b->next)
    {
      if (b->refcnt == 0)
      {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        // 不管是哪个桶中的，移动到 TheBucketNumber 号桶中
        // 将该缓存块插入到应该插的桶的最后
        /*问题：*/
        // 错误所在，困扰了1-2个小时，就是把 TheBucketNumber 写成了 j
        b->next->prev = b->prev;
        b->prev->next = b->next;
        b->next = bcache.BufBucket[TheBucketNumber].head.next;
        b->prev = &bcache.BufBucket[TheBucketNumber].head;
        bcache.BufBucket[TheBucketNumber].head.next->prev = b;
        bcache.BufBucket[TheBucketNumber].head.next = b;
        if (j != TheBucketNumber)
        {
          release(&bcache.BufBucket[j].lock);
        }
        release(&bcache.BufBucket[TheBucketNumber].lock);

        acquiresleep(&b->lock);
        return b;
      }
    }
    if (j != TheBucketNumber)
    {
      release(&bcache.BufBucket[j].lock);
    }
  }
  for (b = bcache.BufBucket[TheBucketNumber].head.prev; b != &bcache.BufBucket[TheBucketNumber].head; b = b->prev)
  {
    printf("%d\n", b->refcnt);
  }
  panic("bget: no buffers");
#endif
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid)
  {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int i = hashing(b->blockno);
  // acquire(&bcache.lock);
  acquire(&bcache.BufBucket[i].lock);
  b->refcnt--;
  // no one is waiting for it.
  if (b->refcnt == 0)
  {
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.BufBucket[i].head.next;
    b->prev = &bcache.BufBucket[i].head;
    bcache.BufBucket[i].head.next->prev = b;
    bcache.BufBucket[i].head.next = b;
  }

  release(&bcache.BufBucket[i].lock);
  // release(&bcache.lock);
}

void bpin(struct buf *b)
{
  int i = hashing(b->blockno);
  acquire(&bcache.BufBucket[i].lock);
  b->refcnt++;
  release(&bcache.BufBucket[i].lock);
}

void bunpin(struct buf *b)
{
  int i = hashing(b->blockno);
  acquire(&bcache.BufBucket[i].lock);
  b->refcnt--;
  release(&bcache.BufBucket[i].lock);
}

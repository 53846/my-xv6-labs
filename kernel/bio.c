// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13

struct {
  struct spinlock evict_lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf buckets[NBUCKET];
  struct spinlock bucket_lock[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;
  struct spinlock *l;

  initlock(&bcache.evict_lock, "bcache_evict");
  for(l = bcache.bucket_lock; l < bcache.bucket_lock + NBUCKET; l++){
    initlock(l, "bcache_bucket");
  }

  for(b = bcache.buckets; b < bcache.buckets + NBUCKET; b++){
    b->next = 0;
  }
  for(b = bcache.buf; b < bcache.buf + NBUF; b++){
    b->refcnt = 0;
    b->next = bcache.buckets[0].next;
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint k;

  k = blockno % NBUCKET;
  acquire(&bcache.bucket_lock[k]);

  // Is the block already cached?
  for(b = bcache.buckets[k].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket_lock[k]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.bucket_lock[k]);
  acquire(&bcache.evict_lock);

  // recheck since bucket_lock[k] has been freed
  // protect the identities of the cached blocks
  acquire(&bcache.bucket_lock[k]);
  for(b = bcache.buckets[k].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket_lock[k]);
      release(&bcache.evict_lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket_lock[k]);
  
  struct buf *lru_prev = 0;
  uint bucketno = -1;
  for(int i = 0; i < NBUCKET; i++) {
    acquire(&bcache.bucket_lock[i]);
    int flag = 0;
    for(b = &bcache.buckets[i]; b->next; b = b->next){
      if(b->next->refcnt == 0 && (!lru_prev || lru_prev->timestamp > b->next->timestamp)) {
        lru_prev = b;
        flag = 1;
      }
    }    
    if(flag) {
      if(bucketno != -1) {
        release(&bcache.bucket_lock[bucketno]);
      }
      bucketno = i;
    } else{
      release(&bcache.bucket_lock[i]);
    }
  }

  if(!lru_prev) {
    panic("bget: no buffers");
  }
  b = lru_prev->next;
  if(bucketno != k) {
    lru_prev->next = b->next;
    release(&bcache.bucket_lock[bucketno]);
    acquire(&bcache.bucket_lock[k]);
    b->next = bcache.buckets[k].next;
    bcache.buckets[k].next = b;
  }
  b->dev = dev;
  b->blockno = blockno;
  b->refcnt = 1;
  b->valid = 0;
  release(&bcache.bucket_lock[k]);
  release(&bcache.evict_lock);
  acquiresleep(&b->lock);
  return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint k = b->blockno % NBUCKET;
  acquire(&bcache.bucket_lock[k]);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->timestamp = ticks;
  }
  
  release(&bcache.bucket_lock[k]);
}

void
bpin(struct buf *b) {
  uint k = b->blockno % NBUCKET;

  acquire(&bcache.bucket_lock[k]);
  b->refcnt++;
  release(&bcache.bucket_lock[k]);
}

void
bunpin(struct buf *b) {
  uint k = b->blockno % NBUCKET;

  acquire(&bcache.bucket_lock[k]);
  b->refcnt--;
  release(&bcache.bucket_lock[k]);
}



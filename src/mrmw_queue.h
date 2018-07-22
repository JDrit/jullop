#ifndef __mrmw_queue_h__
#define __mrmw_queue_h__

#include <sys/eventfd.h>

enum MrMwQueueResult {
  MRMW_QUEUE_SUCCESS,
  MRMW_QUEUE_FAIL,
};

typedef struct MrMwQueue {
  /* max size of the queue */
  size_t max_size;
  /* how many items are currently in the queue */
  size_t current_size;
  /* stores the pointers to items in the queue */
  void **ring_buffer;
  /* the offset in the buffer where items will be added to */
  size_t enqueue_offset;
  /* the offset in the buffer where items will be removed from */
  size_t dequeue_offset;
  /* the number of threads waiting to push to the queue but cannot due
   * to there not being enough space */
  size_t num_waiting_full;
  /* the number of threads waiting to pop from the queue but cannot due
   * to there not being any items available */
  size_t num_waiting_empty;
  
  pthread_mutex_t lock;
  pthread_cond_t is_empty;
  pthread_cond_t is_full;
} MrMwQueue;


/**
 * Creates a concurrent queue that has a max capacity of max_size. Operations
 * will block if more than that many items try to be added to the queue.
 */
MrMwQueue *mrmw_queue_init(size_t max_size);

void mrmw_queue_destroy(MrMwQueue *queue);

/**
 * Gets the current amount of items that are in the queue. This is a
 * thread-safe operation.
 */
size_t mrmw_queue_size(MrMwQueue *queue);

/**
 * Enqueues an item onto the queue. This blocks till the operation can
 * complete.
 */
void mrmw_queue_push(MrMwQueue *queue, void *ptr);

enum MrMwQueueResult mrmw_queue_trypush(MrMwQueue *queue, void *ptr);

void mrmw_queue_pop(MrMwQueue *queue, void **data);

enum MrMwQueueResult mrmw_queue_trypop(MrMwQueue *queue, void **ptr);

#endif

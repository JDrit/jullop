#ifndef __queue_h__
#define __queue_h__

#include <sys/eventfd.h>

enum QueueResult {
  QUEUE_SUCCESS,
  QUEUE_FAIL,
};

typedef struct Queue {
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
} Queue;


/**
 * Creates a concurrent queue that has a max capacity of max_size. Operations
 * will block if more than that many items try to be added to the queue.
 */
Queue *queue_init(size_t max_size);

void queue_destroy(Queue *queue);

/**
 * Gets the current amount of items that are in the queue. This is a
 * thread-safe operation.
 */
size_t queue_size(Queue *queue);

/**
 * Enqueues an item onto the queue. This blocks till the operation can
 * complete.
 */
void queue_push(Queue *queue, void *ptr);

enum QueueResult queue_trypush(Queue *queue, void *ptr);

void queue_pop(Queue *queue, void **data);

enum QueueResult queue_trypop(Queue *queue, void **ptr);

#endif

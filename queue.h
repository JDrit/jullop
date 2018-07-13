#ifndef __queue_h__
#define __queue_h__

enum QueueResult {
  QUEUE_SUCCESS,
  QUEUE_FAIL,
};

struct Queue;
typedef struct Queue Queue;

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

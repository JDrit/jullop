#define _GNU_SOURCE

#include <pthread.h>
#include <stdlib.h>

#include "logging.h"
#include "queue.h"

struct Node {
  void *ptr;
  struct Node *next;
};

struct Queue {
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
};

static inline int is_full(Queue *queue) {
  return queue->current_size == queue->max_size;
}

static inline int is_empty(Queue *queue) {
  return queue->current_size == 0;
}

Queue *queue_init(size_t max_size) {
  Queue *queue = (Queue*) CHECK_MEM(calloc(1, sizeof(Queue)));
  queue->max_size = max_size;
  queue->ring_buffer = CHECK_MEM(calloc(max_size, sizeof(void*)));

  int r = pthread_mutex_init(&queue->lock, NULL);
  CHECK(r != 0, "Failed to create mutex");

  r = pthread_cond_init(&queue->is_empty, NULL);
  CHECK(r != 0, "Failed to create cond var");

  r = pthread_cond_init(&queue->is_full, NULL);
  CHECK(r != 0, "Failed to create cond var");

  return queue;
}

void queue_destroy(Queue *queue) {
  free(queue->ring_buffer);

  int r = pthread_mutex_destroy(&queue->lock);
  CHECK(r != 0, "Failed to destory mutex");

  r = pthread_cond_destroy(&queue->is_empty);
  CHECK(r != 0, "Failed to destroy cond var");

  r = pthread_cond_destroy(&queue->is_full);
  CHECK(r != 0, "Failed to destroy cond var");

  free(queue);
}

size_t queue_size(Queue *queue) {
  int r = pthread_mutex_lock(&queue->lock);
  CHECK(r != 0, "Failed to lock mutex");

  size_t size = queue->current_size;

  r = pthread_mutex_unlock(&queue->lock);
  CHECK(r != 0, "Failed to unlock mutex");

  return size;
}

void queue_push(Queue *queue, void *ptr) {

  int r = pthread_mutex_lock(&queue->lock);
  CHECK(r != 0, "Failed to lock mutex");

  while (is_full(queue)) {
    queue->num_waiting_full++;
    r = pthread_cond_wait(&queue->is_full, &queue->lock);
    CHECK(r != 0, "Faled to wait on conditional variable");
    queue->num_waiting_full--;
  }

  queue->ring_buffer[queue->enqueue_offset] = ptr;
  queue->enqueue_offset = (queue->enqueue_offset + 1) % queue->max_size;
  queue->current_size++;

  if (queue->num_waiting_empty) {
    r = pthread_cond_signal(&queue->is_empty);
    CHECK(r != 0, "Failed to signal empty conditional variable");
  }

  pthread_mutex_unlock(&queue->lock);
}

enum QueueResult queue_trypush(Queue *queue, void *ptr) {

  int r = pthread_mutex_lock(&queue->lock);
  CHECK(r != 0, "Failed to lock mutex");

  if (is_full(queue)) {
    r = pthread_mutex_unlock(&queue->lock);
    CHECK(r != 0, "Failed to unlock mutex");
    return QUEUE_FAIL;
  }

  queue->ring_buffer[queue->enqueue_offset] = ptr;
  queue->enqueue_offset = (queue->enqueue_offset + 1) % queue->max_size;
  queue->current_size++;
  
  if (queue->num_waiting_empty) {
    r = pthread_cond_signal(&queue->is_empty);
    CHECK(r != 0, "Failed to signal empty conditional variable");
  }

  r = pthread_mutex_unlock(&queue->lock);
  CHECK(r != 0, "Failed to unlock mutex");
  return QUEUE_SUCCESS;
}

void queue_pop(Queue *queue, void **data) {
  int r = pthread_mutex_lock(&queue->lock);
  CHECK(r != 0, "Failed to lock mutex");

  while (is_empty(queue)) {
    queue->num_waiting_empty++;
    r = pthread_cond_wait(&queue->is_empty, &queue->lock);
    CHECK(r != 0, "Failed to wait on codnitional variable");
    queue->num_waiting_empty--;
  }

  *data = queue->ring_buffer[queue->dequeue_offset];
  queue->dequeue_offset = (queue->dequeue_offset + 1) % queue->max_size;
  queue->current_size--;

  if (queue->num_waiting_full) {
    r = pthread_cond_signal(&queue->is_full);
    CHECK(r != 0, "Failed to signal full conditional variable");
  }
  
  r = pthread_mutex_unlock(&queue->lock);
  CHECK(r != 0, "Failed to unlock mutex");
}

enum QueueResult queue_trypop(Queue *queue, void **ptr) {
  int r = pthread_mutex_lock(&queue->lock);
  CHECK(r != 0, "Failed to lock mutex");

  if (is_empty(queue)) {
    r = pthread_mutex_unlock(&queue->lock);
    CHECK(r != 0, "Failed to unlock mutex");
    return QUEUE_FAIL;
  }

  *ptr = queue->ring_buffer[queue->dequeue_offset];
  queue->dequeue_offset = (queue->dequeue_offset + 1) % queue->max_size;
  queue->current_size--;

  if (queue->num_waiting_full) {
    r = pthread_cond_signal(&queue->is_full);
    CHECK(r != 0, "Failed to signal full conditional variable");    
  }

  r = pthread_mutex_unlock(&queue->lock);
  CHECK(r != 0, "Failed to unlock mutex");
  return QUEUE_SUCCESS;
}

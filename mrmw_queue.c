
#ifdef LOCK_QUEUE

#define _GNU_SOURCE

#include <pthread.h>
#include <stdlib.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include "logging.h"
#include "queue.h"

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
  
  pthread_mutex_t lock;

  int add_event;
  
} Queue;


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

  queue->add_event = eventfd(0, EFD_NONBLOCK);
  
  return queue;
}

void queue_destroy(Queue *queue) {
  free(queue->ring_buffer);

  int r = pthread_mutex_destroy(&queue->lock);
  CHECK(r != 0, "Failed to destory mutex");
  close(queue->add_event);
  
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

int queue_add_event_fd(Queue *queue) {
  return queue->add_event;
}

enum QueueResult queue_push(Queue *queue, RequestContext *request_context) {
  int r = pthread_mutex_lock(&queue->lock);
  CHECK(r != 0, "Failed to lock mutex");

  if (is_full(queue)) {
    r = pthread_mutex_unlock(&queue->lock);
    CHECK(r != 0, "Failed to unlock mutex");
    return QUEUE_FAILURE;
  }

  queue->ring_buffer[queue->enqueue_offset] = request_context;
  queue->enqueue_offset = (queue->enqueue_offset + 1) % queue->max_size;
  queue->current_size++;

  r = eventfd_write(queue->add_event, 1);
  CHECK(r != 0, "Failed to send event notification");

  r = pthread_mutex_unlock(&queue->lock);
  CHECK(r != 0, "Failed to unlock mutex");
  return QUEUE_SUCCESS;
}

RequestContext *queue_pop(Queue *queue) {
  int r = pthread_mutex_lock(&queue->lock);
  CHECK(r != 0, "Failed to lock mutex");

  if (is_empty(queue)) {
    r = pthread_mutex_unlock(&queue->lock);
    CHECK(r != 0, "Failed to unlock mutex");
    return NULL;
  }

  RequestContext *request_context = queue->ring_buffer[queue->dequeue_offset];
  queue->dequeue_offset = (queue->dequeue_offset + 1) % queue->max_size;
  queue->current_size--;

  r = pthread_mutex_unlock(&queue->lock);
  CHECK(r != 0, "Failed to unlock mutex");
  return request_context;
}

#endif

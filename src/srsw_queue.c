#define _GNU_SOURCE

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "logging.h"
#include "queue.h"

typedef struct Queue {
  /* the max amount of elements allowed in the mailbox. */
  size_t max_size;
  size_t mask;

  /* the offset into the ring buffer that items should be added to. */
  _Atomic size_t push_offset;

  /* the offset info the ring buffer that items should be removed from. */
  _Atomic size_t pop_offset;
  
  /* the internal buffer used to store the messages. This should never be 
   * accessed by any external caller. By using a ring buffer, we prevent
   * any memory allocations being required during the request. */
  void **ring_buffer;

  /* the event file descriptor used to detect when a new message was added
   * to the mailbox. */
  int add_event;
  
} Queue;

const char *queue_result_name(enum QueueResult result) {
  switch (result) {
  case QUEUE_SUCCESS: return "QUEUE_SUCCESS";
  case QUEUE_FAILURE: return "QUEUE_FAILURE";
  default: return "QUEUE_UNKNOWN";
  }
}

static inline size_t pow_2_size(size_t value) {
  value--;
  value |= value >> 1;
  value |= value >> 2;
  value |= value >> 4;
  value |= value >> 8;
  value |= value >> 16;
  value++;
  return value;
}

Queue *queue_init(size_t size) {
  // memalign must be a power of two
  size = pow_2_size(size);

  Queue *queue = (Queue*) CHECK_MEM(calloc(1, sizeof(Queue)));
  queue->max_size = size;
  queue->mask = size - 1;
  queue->push_offset = ATOMIC_VAR_INIT(0);
  queue->pop_offset = ATOMIC_VAR_INIT(0);

  /* aligns the items in the queue to the word. */
  int r = posix_memalign((void**) &queue->ring_buffer, sizeof(size_t), size);
  CHECK(r != 0, "Failed to malloc an aligned memory region");

  queue->add_event = eventfd(0, EFD_NONBLOCK);

  LOG_DEBUG("Queue of size %zu created", queue->max_size);
  return queue;
}

void queue_destroy(Queue *queue) {
  free(queue->ring_buffer);
  close(queue->add_event);
  free(queue);
}
int queue_add_event_fd(Queue *queue) {
  return queue->add_event;
}

void queue_print(Queue *queue) {
  LOG_INFO("Queue: size=%zu", queue_size(queue));
}

size_t queue_size(Queue *queue) {
  size_t push = atomic_load(&queue->push_offset);
  size_t pop = atomic_load(&queue->pop_offset);
  if (push >= pop) {
    return push - pop;
  } else {
    return queue->max_size - pop + push;
  }
}

enum QueueResult queue_push(Queue *queue, void *ptr) {
  size_t push_offset = atomic_load_explicit(&queue->push_offset,
					    __ATOMIC_RELAXED);
  size_t pop_offset = atomic_load_explicit(&queue->pop_offset,
					   __ATOMIC_ACQUIRE);

  if (((push_offset + 1) & queue->mask) == pop_offset) {
    return QUEUE_FAILURE;
  } else {
    queue->ring_buffer[push_offset] = ptr;
    atomic_store_explicit(&queue->push_offset,
			  (push_offset + 1) & queue->mask,
			  __ATOMIC_RELEASE);

    /* Uses the event file descriptor as the notification system */
    int r = eventfd_write(queue->add_event, 1);
    CHECK(r != 0, "Failed to send event notification");
    return QUEUE_SUCCESS;
  }
}

void *queue_pop(Queue *queue) {
  size_t pop_offset = atomic_load_explicit(&queue->pop_offset,
					   __ATOMIC_RELAXED);
  size_t push_offset = atomic_load_explicit(&queue->push_offset,
					    __ATOMIC_ACQUIRE);

  if (pop_offset == push_offset) {
    return NULL;
  } else {
    void *ptr = queue->ring_buffer[pop_offset];

    atomic_store_explicit(&queue->pop_offset,
			  (pop_offset + 1) & queue->mask,
			  __ATOMIC_RELEASE);
    return ptr;
  }
}


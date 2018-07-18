#define _GNU_SOURCE

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "logging.h"
#include "queue.h"
#include "request_context.h"
#include "time_stats.h"

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

Queue *queue_init(size_t max_size) {
  // memalign must be a power of two
  max_size = pow_2_size(max_size);

  Queue *queue = (Queue*) CHECK_MEM(calloc(1, sizeof(Queue)));
  queue->max_size = max_size;
  queue->mask = max_size - 1;
  queue->head = ATOMIC_VAR_INIT(0);
  queue->tail = ATOMIC_VAR_INIT(0);

  /* aligns the items in the queue to the word. */
  int r = posix_memalign((void**) &queue->ring_buffer, sizeof(void*), max_size + 1);
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
  size_t tail = atomic_load(&queue->tail);
  size_t head = atomic_load(&queue->head);
  return head - tail;
}

enum QueueResult queue_push(Queue *queue, RequestContext *request_context) {
	
  size_t head = atomic_load(&queue->head);
  size_t tail = atomic_load(&queue->tail);

  if (((tail - (head + 1)) & queue->mask) >= 1) {
    /* Starts tracking the time spent in the queue for this request. */
    request_set_queue_start(&request_context->time_stats);

    queue->ring_buffer[head & queue->mask] = request_context;
    atomic_store(&queue->head, head + 1);

    /* Uses the event file descriptor as the notification system */
    int r = eventfd_write(queue->add_event, 1);
    CHECK(r != 0, "Failed to send event notification");
    return QUEUE_SUCCESS;
  } else {
    return QUEUE_FAILURE;
  }
}

enum QueueResult queue_pop(Queue *queue, RequestContext **request_context) {

  size_t tail = atomic_load(&queue->tail);
  size_t head = atomic_load(&queue->head);

  if (((head - tail) & queue->mask) >= 1) {
    *request_context = queue->ring_buffer[tail & queue->mask];

    /* Finishes tracking the time spent in the queue for this request. */
    request_set_queue_end(&(*request_context)->time_stats);
    
    atomic_store(&queue->tail, tail + 1);
    return QUEUE_SUCCESS;
  } else {
    return QUEUE_FAILURE;
  }
}


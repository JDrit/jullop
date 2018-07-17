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

Queue *queue_init(size_t max_size) {
  Queue *queue = (Queue*) CHECK_MEM(calloc(1, sizeof(Queue)));
  queue->max_size = max_size;
  queue->push_offset = ATOMIC_VAR_INIT(0);
  queue->pop_offset = ATOMIC_VAR_INIT(0);
  queue->ring_buffer = (RequestContext**) CHECK_MEM(calloc(max_size, sizeof(RequestContext*)));
  queue->add_event = eventfd(0, EFD_NONBLOCK);

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
  size_t push_offset = atomic_load(&queue->push_offset);
  size_t pop_offset = atomic_load(&queue->pop_offset);

  if (push_offset > pop_offset) {
    return push_offset - pop_offset;
  } else {
    return queue->max_size - pop_offset + push_offset;
  }
}

enum QueueResult queue_push(Queue *queue, RequestContext *request_context) {
  /* Starts tracking the time spent in the queue for this request. */
  request_set_queue_start(&request_context->time_stats);
	
  size_t pop_offset = atomic_load(&queue->pop_offset);
  size_t push_offset = atomic_load(&queue->push_offset);

  if ((push_offset + 1) % queue->max_size == pop_offset) {
    return QUEUE_FAILURE;
  }

  queue->ring_buffer[push_offset] = request_context;

  atomic_store(&queue->push_offset, (push_offset + 1) % queue->max_size);
  int r = eventfd_write(queue->add_event, 1);
  CHECK(r != 0, "Failed to send event notification");  
  return QUEUE_SUCCESS;
}

enum QueueResult queue_pop(Queue *queue, RequestContext **request_context) {

  size_t pop_offset = atomic_load(&queue->pop_offset);
  size_t push_offset = atomic_load(&queue->push_offset);
  size_t new_offset;
  
  do {
    if (pop_offset == push_offset) {
      return QUEUE_FAILURE;
    }
    *request_context = queue->ring_buffer[pop_offset];
    new_offset = (pop_offset + 1) % queue->max_size;
    
  } while (!atomic_compare_exchange_weak(&queue->pop_offset, &pop_offset, new_offset));

  request_set_queue_end(&(*request_context)->time_stats);
  return QUEUE_SUCCESS;
}


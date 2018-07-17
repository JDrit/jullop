#ifndef __mailbox_h__
#define __mailbox_h__

#include <stdatomic.h>
#include <stdint.h>

#include "request_context.h"

/**
 * The queue is designed for efficient inter-thread communication when there is only 1 thread
 * writing sending messages and multiple threads receiving them.
 * 
 * There is no blocking / conditional variables used in this code-base. If the queue is full, then
 * push operation will fail. To allow the receiver of messages to wait for new data, the queue
 * exposes an event file descriptor and sends a notification on it every time that there is a new
 * message.
 */

enum QueueResult {
  QUEUE_SUCCESS,
  QUEUE_FAILURE,
};

typedef struct Queue {
  /* the max amount of elements allowed in the mailbox. */
  size_t max_size;

  /* the offset into the ring buffer that items should be added to. */
  _Atomic size_t push_offset;

  /* the offset info the ring buffer that items should be removed from. */
  _Atomic size_t pop_offset;
  
  /* the internal buffer used to store the messages. This should never be 
   * accessed by any external caller. By using a ring buffer, we prevent
   * any memory allocations being required during the request. */
  RequestContext **ring_buffer;

  /* the event file descriptor used to detect when a new message was added
   * to the mailbox. */
  int add_event;
  
} Queue;

const char *queue_result_name(enum QueueResult result);

/**
 * Constructs a mailbox to send / receive requests on with a given max size.
 */
Queue *queue_init(size_t max_size);

void queue_destroy(Queue *queue);

/**
 * Prints to stdout the status of the given queu;
 */
void queue_print(Queue *queue);

/**
 * Gets the current size of the mailbox.
 */
size_t queue_size(Queue *queue);

/**
 * Returns the file descriptor to use to receive event notifications for
 * when new items have been added to the queue.
 */
int queue_add_event_fd(Queue *queue);

/**
 * Tries to add a new message to the queue. This will return a failure
 * result if the queue is full. If a message is successfully inserted,
 * then a notification is sent to the event file descriptor.
 *
 * If a successful status code is returned, then the data that was passed
 * in has been added and for concurrency control, the caller thread must
 * not use that memory after this call finishes.
 */
enum QueueResult queue_push(Queue *queue, RequestContext *request_context);

/**
 * Tries to read a new message from the queue. This will return a
 * failure result if the queue is empty.
 *
 * If a successful status code is returned, then the data pointer is modified
 * to point to the new payload. The caller thread now has full ownership of
 * the payload and is responsible for freeing any memory involved.
 */
enum QueueResult queue_pop(Queue *queue, RequestContext **request_context);

#endif

#ifndef __mailbox_h__
#define __mailbox_h__

#include <stdatomic.h>
#include <stdint.h>

#include "request_context.h"

/**
 * The queue is designed for efficient inter-thread communication designed for a
 * single-reader single-writer mode.
 * 
 * There is no blocking / conditional variables used in this code-base. If the
 * queue is full, then push operation will fail. To allow the receiver of
 * messages to wait for new data, the queue exposes an event file descriptor and
 * sends a notification on it every time that there is a new message.
 */

enum QueueResult {
  QUEUE_SUCCESS = 0,
  QUEUE_FAILURE = 1,
};

typedef struct Queue Queue;

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
 * Returns the file descriptor to use to receive event notifications for when
 * new items have been added to the queue.
 */
int queue_add_event_fd(Queue *queue);

/**
 * Tries to add a new message to the queue. This will return a failure result if
 * the queue is full. If a message is successfully inserted, then a notification
 * is sent to the event file descriptor.
 *
 * If a successful status code is returned, then the data that was passed in has
 * been added and for concurrency control, the caller thread must not use that
 * memory after this call finishes.
 */
enum QueueResult queue_push(Queue *queue, RequestContext *request_context);

/**
 * Tries to read a new message from the queue. It returns the next request
 * context to use. Null is returned if the queue is empty.
 */
RequestContext *queue_pop(Queue *queue);

#endif

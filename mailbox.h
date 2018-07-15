#ifndef __mailbox_h__
#define __mailbox_h__

#include <stdint.h>

enum MailResult {
  MAIL_SUCCESS,
  MAIL_FAILURE,
};

typedef struct Mailbox {
  uint16_t max_size;

  uint64_t state;
  
  void **ring_buffer;
  
  int add_event;
  
} Mailbox;


/**
 * Constructs a mailbox to send / receive requests on with a given max size.
 */
Mailbox *mailbox_init(uint16_t max_size);

void mailbox_destroy(Mailbox *mailbox);

/**
 * Returns the file descriptor to use to receive event notifications for
 * when new items have been added to the queue.
 */
int mailbox_add_event_fd(Mailbox *mailbox);

/**
 * Tries to add a new message to the mailbox. This will return a failure
 * result if the mailbox is full. If a message is successfully inserted,
 * then a notification is sent to the event file descriptor.
 *
 * If a successful status code is returned, then the data that was passed
 * in has been added and for concurrency control, the caller thread must
 * not use that memory after this call finishes.
 */
enum MailResult mailbox_send(Mailbox *mailbox, void *data);

enum MailResult mailbox_recv(Mailbox *mailbox, void **data);

#endif

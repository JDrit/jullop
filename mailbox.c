#define _GNU_SOURCE

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "logging.h"
#include "mailbox.h"

typedef struct State {
  uint16_t current_size;
  uint16_t send_ptr;
  uint16_t recv_ptr;
  uint16_t __trash;
} State;

Mailbox *mailbox_init(uint16_t max_size) {
  Mailbox *mailbox = (Mailbox*) CHECK_MEM(calloc(1, sizeof(Mailbox)));
  mailbox->max_size = max_size;
  mailbox->ring_buffer = (void**) CHECK_MEM(calloc(max_size, sizeof(void*)));
  mailbox->add_event = eventfd(0, EFD_NONBLOCK);

  return mailbox;
}

void mailbox_destroy(Mailbox *mailbox) {
  free(mailbox->ring_buffer);
  close(mailbox->add_event);
  free(mailbox);
}

static inline void load_state(Mailbox *mailbox, uint64_t *state_encoded) {
  __atomic_load(&mailbox->state, state_encoded, __ATOMIC_SEQ_CST);
}

static inline bool cas_state(Mailbox *mailbox, uint64_t *expected,
			     uint64_t *desired) {
  return __atomic_compare_exchange(&mailbox->state, expected,
				   desired, true, __ATOMIC_SEQ_CST,
				   __ATOMIC_RELAXED);
}

enum MailResult mailbox_send(Mailbox *mailbox, void *data) {
  /* the state only has to be loaded once since when the CAS operation
   * fails, it modifies the expected state to be the actual current
   * value. */
  uint64_t cur_state_encoded;
  load_state(mailbox, &cur_state_encoded);
  
  while (1) {
    State state;
    *((uint64_t*) &state) = cur_state_encoded;
    
    if (state.current_size == mailbox->max_size) {
      return MAIL_FAILURE;
    }
    
    mailbox->ring_buffer[state.send_ptr] = data;
    state.send_ptr = (state.send_ptr + 1) % mailbox->max_size;
    state.current_size++;

    uint64_t new_state_encoded;
    *((State*) &new_state_encoded) = state;

    if (cas_state(mailbox, &cur_state_encoded, &new_state_encoded) == true) {
      break;
    }
  }
  
  int r = eventfd_write(mailbox->add_event, 1);
  CHECK(r != 0, "Failed to send event notification");  
  return MAIL_SUCCESS;
}

enum MailResult mailbox_recv(Mailbox *mailbox, void **data) {
  uint64_t cur_state_encoded;
  load_state(mailbox, &cur_state_encoded);

  while (1) {
    State state;
    *((uint64_t*) &state) = cur_state_encoded;

    if (state.current_size == 0) {
      return MAIL_FAILURE;
    }

    void *new_data = mailbox->ring_buffer[state.recv_ptr];

    state.recv_ptr = (state.recv_ptr + 1) % mailbox->max_size;
    state.current_size--;
    
    uint64_t new_state_encoded;
    *((State*) &new_state_encoded) = state;
    if (cas_state(mailbox, &cur_state_encoded, &new_state_encoded) == true) {
      *data = new_data;
      return MAIL_SUCCESS;
    }
  }
}

  

